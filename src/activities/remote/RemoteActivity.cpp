#include "RemoteActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <WiFi.h>
#include <esp_random.h>

#include <memory>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"
#include "remote/WsFrame.h"
#include "voice/SecureRequest.h"
#include "voice/TrustRoots.h"

namespace {

constexpr const char* kTokenPath = "/.sticky/tv.json";
constexpr int kColumns = 3;
constexpr int kRows = 5;
constexpr int kGridTop = 60;

// Only commands the spec allows: no power, Wi-Fi or trackpad.
struct Button {
  const char* label;
  const char* command;
};
constexpr Button kButtons[kColumns * kRows] = {
    {"Home", R"({"type":"browser.home"})"},
    {"Up", R"({"type":"key.press","key":"ArrowUp","modifiers":[]})"},
    {"Back", R"({"type":"browser.back"})"},
    {"Left", R"({"type":"key.press","key":"ArrowLeft","modifiers":[]})"},
    {"OK", R"({"type":"key.press","key":"Enter","modifiers":[]})"},
    {"Right", R"({"type":"key.press","key":"ArrowRight","modifiers":[]})"},
    {"-10s", R"({"type":"media.seek","seconds":-10})"},
    {"Down", R"({"type":"key.press","key":"ArrowDown","modifiers":[]})"},
    {"+10s", R"({"type":"media.seek","seconds":10})"},
    {"Vol -", R"({"type":"volume.change","delta":-5})"},
    {"Play", R"({"type":"media.playPause"})"},
    {"Vol +", R"({"type":"volume.change","delta":5})"},
    {"Mute", R"({"type":"volume.mute"})"},
    {"Screen off", R"({"type":"display.sleep"})"},
    {"Screen on", R"({"type":"display.wake"})"},
};

}  // namespace

void RemoteActivity::showMessage(const std::string& text) {
  message = text;
  requestUpdate(true);
}

void RemoteActivity::onEnter() {
  Activity::onEnter();
  voice::Config config;
  if (!voice::loadConfig(config) || config.tvHost.empty()) {
    showMessage("Add tvHost to /.sticky/config.json first.");
    return;
  }
  tvHost = config.tvHost;
  std::string saved;
  if (voice::readTextFile(kTokenPath, saved)) {
    JsonDocument doc;
    if (!deserializeJson(doc, saved)) tvToken = doc["token"] | "";
  }
  if (WiFi.status() == WL_CONNECTED) {
    onWifiReady();
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             showMessage("Not connected.");
                           } else {
                             WiFi.setSleep(false);  // Modem sleep would add seconds of lag to every press.
                             onWifiReady();
                           }
                         });
}

void RemoteActivity::onExit() {
  Activity::onExit();
  socket.stop();
  WiFi.disconnect(false);
  delay(30);
  WiFi.mode(WIFI_OFF);
}

void RemoteActivity::onWifiReady() {
  if (tvToken.empty()) {
    requestPairing();
    return;
  }
  ready = true;
  showMessage(ensureConnected() ? "" : "TV not reachable. Tap a button to retry.");
}

void RemoteActivity::requestPairing() {
  std::string response;
  voice::Request request;
  request.host = tvHost;
  request.method = "POST";
  request.target = "/api/pair/request";
  request.contentType = "application/json";
  request.bodyLength = 2;
  request.sendBody = [](freeink::SecureClient& c) {
    return voice::writeAll(c, reinterpret_cast<const uint8_t*>("{}"), 2);
  };
  request.onBody = [&response](const uint8_t* data, const size_t size) {
    if (response.size() < 1024) response.append(reinterpret_cast<const char*>(data), size);
  };
  const int status = voice::secureRequest(request);
  JsonDocument doc;
  if (status != 200 || deserializeJson(doc, response)) {
    showMessage("TV not reachable. Are you on the home network?");
    return;
  }
  pairingId = doc["pairingId"] | "";
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Code on the TV", "", 6),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             showMessage("Pairing cancelled.");
                             return;
                           }
                           completePairing(std::get<KeyboardResult>(result.data).text);
                         });
}

void RemoteActivity::completePairing(const std::string& code) {
  JsonDocument body;
  body["pairingId"] = pairingId;
  body["code"] = code;
  body["deviceName"] = "Sticky";
  std::string json;
  serializeJson(body, json);

  std::string response;
  voice::Request request;
  request.host = tvHost;
  request.method = "POST";
  request.target = "/api/pair/complete";
  request.contentType = "application/json";
  request.bodyLength = json.size();
  request.sendBody = [&json](freeink::SecureClient& c) {
    return voice::writeAll(c, reinterpret_cast<const uint8_t*>(json.data()), json.size());
  };
  request.onBody = [&response](const uint8_t* data, const size_t size) {
    if (response.size() < 1024) response.append(reinterpret_cast<const char*>(data), size);
  };
  const int status = voice::secureRequest(request);
  JsonDocument doc;
  if (status != 200 || deserializeJson(doc, response) || !(doc["token"] | "")[0]) {
    showMessage("That code didn't work. Open Remote to try again.");
    return;
  }
  tvToken = doc["token"] | "";
  HalFile file;
  if (Storage.openFileForWrite("REMOTE", kTokenPath, file)) {
    JsonDocument saved;
    saved["token"] = tvToken;
    serializeJson(saved, file);
    file.close();
  }
  ready = true;
  showMessage(ensureConnected() ? "" : "TV not reachable. Tap a button to retry.");
}

bool RemoteActivity::ensureConnected() {
  if (socket.connected()) return true;
  socket.stop();
  socket.setCACert(voice::kTrustRoots);
  if (!socket.connect(tvHost.c_str(), 443)) return false;
  // The server does not check the key beyond its format, so a fixed one is fine for this private link.
  const std::string handshake = "GET /ws?token=" + tvToken + " HTTP/1.1\r\nHost: " + tvHost +
                                "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                                "Sec-WebSocket-Key: c3RpY2t5LXJlbW90ZS1rZXk=\r\nSec-WebSocket-Version: 13\r\n\r\n";
  if (!voice::writeAll(socket, reinterpret_cast<const uint8_t*>(handshake.data()), handshake.size())) return false;
  std::string head;
  const uint32_t started = millis();
  uint8_t buffer[128];
  while (head.find("\r\n\r\n") == std::string::npos && millis() - started < 5000 && head.size() < 2048) {
    if (socket.available() <= 0) {
      if (!socket.connected()) return false;
      delay(5);
      continue;
    }
    const int got = socket.read(buffer, sizeof(buffer));
    if (got > 0) head.append(reinterpret_cast<char*>(buffer), got);
  }
  return head.rfind("HTTP/1.1 101", 0) == 0;
}

void RemoteActivity::send(const char* commandJson) {
  if (!ensureConnected()) {
    showMessage("TV not reachable. Tap a button to retry.");
    return;
  }
  const std::string payload = "{\"id\":" + std::to_string(nextId++) + ",\"command\":" + commandJson + "}";
  const std::string frame = remote::encodeTextFrame(payload, esp_random());
  if (!voice::writeAll(socket, reinterpret_cast<const uint8_t*>(frame.data()), frame.size())) socket.stop();
  if (!message.empty()) showMessage("");
}

void RemoteActivity::loop() {
  // The TV pushes its state to every client. Nothing here uses it, but unread data would back up.
  uint8_t discard[256];
  while (ready && socket.available() > 0) socket.read(discard, sizeof(discard));

  if (ready) {
    int x;
    int y;
    if (mappedInput.wasScreenTapped(x, y)) {
      const int cell = remote::gridCell(x, y, kGridTop, renderer.getScreenWidth(),
                                        renderer.getScreenHeight() - kGridTop, kColumns, kRows);
      if (cell >= 0) send(kButtons[cell].command);
    }
    // The side keys work without looking, which suits a remote.
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) send(kButtons[11].command);
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) send(kButtons[9].command);
  }
}

void RemoteActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_10_FONT_ID, 20, message.empty() ? "TV Remote" : message.c_str());
  if (ready) {
    const int width = renderer.getScreenWidth();
    const int height = renderer.getScreenHeight() - kGridTop;
    const int cellWidth = width / kColumns;
    const int cellHeight = height / kRows;
    for (int row = 0; row < kRows; ++row) {
      for (int column = 0; column < kColumns; ++column) {
        const int x = column * cellWidth;
        const int y = kGridTop + row * cellHeight;
        renderer.drawRect(x + 4, y + 4, cellWidth - 8, cellHeight - 8);
        const char* label = kButtons[row * kColumns + column].label;
        const int textWidth = renderer.getTextWidth(UI_12_FONT_ID, label);
        renderer.drawText(UI_12_FONT_ID, x + (cellWidth - textWidth) / 2, y + cellHeight / 2 - 10, label);
      }
    }
  }
  renderer.displayBuffer();
}
