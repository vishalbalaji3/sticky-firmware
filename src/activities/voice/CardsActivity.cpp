#include "CardsActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Logging.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include <WiFi.h>

#include "activities/reader/AutoProgressSync.h"
#include "fontIds.h"
#include "network/SavedWifi.h"
#include "network/WifiPowerSaveGuard.h"
#include "remote/WsFrame.h"
#include "voice/SecureRequest.h"
#include "voice/VoiceFormat.h"
#include "voice/VoiceNotes.h"
#include "voice/VoiceUpload.h"

namespace {
constexpr const char* kLabels[] = {"Again", "Hard", "Good", "Easy"};
constexpr int kRatingBarHeight = 110;
constexpr float kCardFontPointSize = 18.0f;
}  // namespace

void CardsActivity::onEnter() {
  Activity::onEnter();
  loadCards();
  requestUpdate();
}

void CardsActivity::onExit() {
  if (WiFi.getMode() != WIFI_MODE_NULL) stopWifi();
  Activity::onExit();
}

void CardsActivity::loadCards() {
  cards.clear();
  index = 0;
  revealed = false;
  std::string text;
  if (voice::readTextFile(voice::kCardsPath, text, 256 * 1024)) {
    // Cards rated since the last sync are already waiting to upload, so they are not asked again.
    std::string pending;
    voice::readTextFile(voice::kReviewsPath, pending, 256 * 1024);
    JsonDocument doc;
    if (!deserializeJson(doc, text)) {
      for (JsonObjectConst card : doc["cards"].as<JsonArrayConst>()) {
        const std::string id = card["id"] | "";
        if (id.empty() || pending.find("\"" + id + "\"") != std::string::npos) continue;
        Card row;
        row.id = id;
        row.front = card["front"] | "";
        row.back = card["back"] | "";
        row.lastReview = card["lastReview"].isNull() ? "null" : std::to_string(card["lastReview"].as<uint64_t>());
        cards.push_back(std::move(row));
      }
    }
  }
}

void CardsActivity::syncNow() {
  syncPending = false;
  {
    // The card shares SPI with the display, and the cards change under the render task.
    RenderLock lock(*this);
    const bool online =
        WiFi.status() == WL_CONNECTED || (hasSavedWifi() && joinSavedWifi(autosync::kJoinTimeoutMs));
    if (online) {
      WifiPowerSaveGuard psGuard;
      voice::syncCardsAndHome();
      loadCards();
    }
  }
  // Off as soon as it is done: the recorder needs it off.
  if (WiFi.getMode() != WIFI_MODE_NULL) stopWifi();
  requestUpdate();
}

void CardsActivity::rate(const int rating) {
  const Card& card = cards[index];
  std::string line = "{\"cardId\":\"" + card.id + "\",\"rating\":" + std::to_string(rating);
  const time_t now = time(nullptr);
  if (now >= voice::kFirstValidUnixTime) line += ",\"reviewedAt\":" + std::to_string(static_cast<uint64_t>(now) * 1000);
  line += ",\"expectedLastReview\":" + card.lastReview + "}\n";

  HalFile file = Storage.open(voice::kReviewsPath, O_WRONLY | O_CREAT | O_APPEND);
  if (!file.isOpen() || file.write(line.data(), line.size()) != line.size()) {
    LOG_ERR("CARDS", "Could not save a rating");  // The card stays up, so the rating is not silently lost.
    return;
  }
  file.close();
  index++;
  revealed = false;
  requestUpdate();
}

void CardsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goHome();
    return;
  }
  // Only before the first rating, so a sync never swaps the deck mid-review.
  if (syncPending && index == 0 && !revealed && !voice::isRecording() && activityManager.currentScreenPainted()) {
    syncNow();
    return;
  }
  if (index >= cards.size()) return;
  int x;
  int y;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  if (!revealed) {
    revealed = true;
    requestUpdate();
    return;
  }
  const int barTop = renderer.getScreenHeight() - kRatingBarHeight;
  const int cell = remote::gridCell(x, y, barTop, renderer.getScreenWidth(), kRatingBarHeight, 4, 1);
  if (cell >= 0) rate(cell + 1);
}

void CardsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  if (index >= cards.size()) {
    renderer.drawCenteredText(
        UI_12_FONT_ID, height / 2,
        cards.empty() ? syncPending ? "Syncing..." : "No cards due. Open again later to sync." : "All done until next sync.");
    renderer.displayBuffer();
    return;
  }

  const Card& card = cards[index];
  const std::string progress = std::to_string(index + 1) + " / " + std::to_string(cards.size());
  renderer.drawCenteredText(UI_10_FONT_ID, 24, progress.c_str());

  int cardFont = SETTINGS.getReaderFontId();
  const int larger = renderer.resolveFontVariant(cardFont, kCardFontPointSize / SETTINGS.fontPointSize);
  if (larger != 0) cardFont = larger;

  int y = 90;
  const auto drawBlock = [&](const std::string& text) {
    size_t start = 0;
    while (start <= text.size()) {
      const size_t end = text.find('\n', start);
      const std::string line = text.substr(start, end == std::string::npos ? end : end - start);
      for (const std::string& wrapped : renderer.wrappedText(cardFont, line.c_str(), width - 48, 8)) {
        renderer.drawCenteredText(cardFont, y, wrapped.c_str());
        y += renderer.getLineHeight(cardFont) + 6;
      }
      if (line.empty()) y += renderer.getLineHeight(cardFont) / 2;
      if (end == std::string::npos) break;
      start = end + 1;
    }
  };
  drawBlock(card.front);

  if (!revealed) {
    renderer.drawCenteredText(UI_10_FONT_ID, height - 60, "Tap to show the answer");
  } else {
    y += 20;
    renderer.drawLine(40, y, width - 40, y);
    y += 30;
    drawBlock(card.back);
    const int top = height - kRatingBarHeight;
    for (int i = 0; i < 4; ++i) {
      const int x = i * width / 4;
      renderer.drawRect(x + 4, top + 8, width / 4 - 8, kRatingBarHeight - 16);
      const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, kLabels[i]);
      renderer.drawText(UI_10_FONT_ID, x + (width / 4 - textWidth) / 2, top + kRatingBarHeight / 2 - 10, kLabels[i]);
    }
  }
  renderer.displayBuffer();
}
