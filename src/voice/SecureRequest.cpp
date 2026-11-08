#include "SecureRequest.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include "TrustRoots.h"
#include "VoiceFormat.h"

namespace voice {
namespace {
constexpr uint32_t kResponseTimeoutMs = 30000;
constexpr size_t kMaxHead = 4096;
constexpr const char* kConfigPath = "/.sticky/config.json";
}  // namespace

bool writeAll(freeink::SecureClient& client, const uint8_t* data, size_t size) {
  while (size > 0) {
    const size_t written = client.write(data, size);
    if (written == 0) return false;
    data += written;
    size -= written;
  }
  return true;
}

int secureRequest(const Request& request) {
  freeink::SecureClient client;
  client.setCACert(kTrustRoots);
  if (!client.connect(request.host.c_str(), 443)) {
    LOG_ERR("NET", "TLS connect to %s failed",
            request.host.c_str());  // SecureClient's own detail needs a host-side DTR.
    return -1;
  }

  std::string head = std::string(request.method) + " " + request.target + " HTTP/1.0\r\nHost: " + request.host;
  if (!request.token.empty()) head += "\r\nAuthorization: Bearer " + request.token;
  if (request.contentType) {
    head += std::string("\r\nContent-Type: ") + request.contentType +
            "\r\nContent-Length: " + std::to_string(request.bodyLength);
  }
  head += "\r\nConnection: close\r\n\r\n";
  if (!writeAll(client, reinterpret_cast<const uint8_t*>(head.data()), head.size()) ||
      (request.sendBody && !request.sendBody(client))) {
    client.stop();
    return -1;
  }

  std::string responseHead;
  int status = -1;
  bool inBody = false;
  long expected = -1;  // Content-Length, when the server sent one.
  size_t received = 0;
  uint8_t buffer[512];
  uint32_t lastData = millis();
  while (millis() - lastData < kResponseTimeoutMs) {
    if (client.available() <= 0) {
      if (!client.connected()) break;
      delay(5);
      continue;
    }
    const int got = client.read(buffer, sizeof(buffer));
    if (got <= 0) continue;
    lastData = millis();
    if (inBody) {
      received += got;
      if (request.onBody) request.onBody(buffer, got);
      continue;
    }
    if (responseHead.size() < kMaxHead) responseHead.append(reinterpret_cast<char*>(buffer), got);
    const size_t end = responseHead.find("\r\n\r\n");
    if (end == std::string::npos) continue;
    status = parseHttpStatus(responseHead);
    expected = parseContentLength(responseHead.substr(0, end + 2));
    inBody = true;
    received = responseHead.size() - end - 4;
    if (request.onBody && received > 0) {
      request.onBody(reinterpret_cast<const uint8_t*>(responseHead.data()) + end + 4, received);
    }
  }
  client.stop();
  // A connection that dropped mid-body still parsed a status. Without a Content-Length nothing can tell.
  if (expected >= 0 && received != static_cast<size_t>(expected)) {
    LOG_ERR("NET", "Reply from %s cut short: %u of %ld bytes", request.host.c_str(), static_cast<unsigned>(received),
            expected);
    return -1;
  }
  return status;
}

bool readTextFile(const std::string& path, std::string& out, const size_t maxBytes) {
  HalFile file;
  if (!Storage.openFileForRead("VOICE", path, file)) return false;
  if (file.fileSize() > maxBytes) {
    file.close();
    return false;
  }
  out.resize(file.fileSize());
  const bool ok = out.empty() || file.read(out.data(), out.size()) == static_cast<int>(out.size());
  file.close();
  return ok;
}

bool loadConfig(Config& config) {
  std::string text;
  if (!Storage.exists(kConfigPath) || !readTextFile(kConfigPath, text)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, text)) return false;
  // shortcut: the token sits unencrypted on the card, revoke it in the PWA if the card is lost.
  config.host = doc["host"] | "";
  config.token = doc["token"] | "";
  return !config.host.empty() && !config.token.empty();
}

}  // namespace voice
