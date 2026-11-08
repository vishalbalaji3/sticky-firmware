#include "CardsActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <ctime>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/reader/AutoProgressSync.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/SavedWifi.h"
#include "network/WifiPowerSaveGuard.h"
#include "voice/SecureRequest.h"
#include "voice/VoiceFormat.h"
#include "voice/VoiceNotes.h"
#include "voice/VoiceUpload.h"

namespace {
constexpr StrId kLabels[] = {StrId::STR_CARDS_AGAIN, StrId::STR_CARDS_HARD, StrId::STR_CARDS_GOOD,
                             StrId::STR_CARDS_EASY};
// Card-specific sizes, measured from the content area so a bezel inset or rotation moves them with it.
constexpr int kRatingBarHeight = 110;
constexpr float kCardFontPointSize = 18.0f;
constexpr int kToggleWidth = 130;
constexpr int kToggleHeight = 50;
constexpr int kToggleInset = 8;
constexpr int kProgressY = 24;
constexpr int kCardTop = 90;
constexpr int kHintFromBottom = 60;
constexpr int kLineGap = 6;

bool inside(const Rect& r, const int x, const int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
}  // namespace

void CardsActivity::onEnter() {
  Activity::onEnter();
  loadCards();
  requestUpdate();
}

Rect CardsActivity::toggleRect() const {
  const Rect content = UITheme::getContentArea(renderer);
  return Rect{content.x + content.width - kToggleWidth - kToggleInset, content.y + kToggleInset, kToggleWidth,
              kToggleHeight};
}

Rect CardsActivity::ratingBarRect() const {
  const Rect content = UITheme::getContentArea(renderer);
  return Rect{content.x, content.y + content.height - kRatingBarHeight, content.width, kRatingBarHeight};
}

void CardsActivity::onExit() {
  // Ratings go up on leaving (Back or sleep), like the reader's exit push. ActivityManager holds the render lock.
  if (Storage.exists(voice::kReviewsPath) && !voice::isRecording()) {
    GUI.drawPopup(renderer, tr(STR_CARDS_SYNCING));
    if (connectSavedWifi(autosync::kJoinTimeoutMs)) {
      WifiPowerSaveGuard psGuard;
      voice::syncCardsAndHome();
    }
  }
  stopWifi();
  Activity::onExit();
}

void CardsActivity::loadCards() {
  cards.clear();
  laidOutIndex = SIZE_MAX;
  index = 0;
  revealed = false;
  furigana = false;
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
        for (JsonObjectConst segment : card["ruby"].as<JsonArrayConst>()) {
          row.ruby.emplace_back(segment["t"] | "", segment["r"] | "");
        }
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
    if (connectSavedWifi(autosync::kJoinTimeoutMs)) {
      WifiPowerSaveGuard psGuard;
      voice::syncCardsAndHome();
      loadCards();
    }
  }
  // Off as soon as it is done, to save battery.
  stopWifi();
  requestUpdate();
}

void CardsActivity::rate(const int rating) {
  const Card& card = cards[index];
  JsonDocument review;
  review["cardId"] = card.id;
  review["rating"] = rating;
  const time_t now = time(nullptr);
  if (now >= voice::kFirstValidUnixTime) review["reviewedAt"] = static_cast<uint64_t>(now) * 1000;
  review["expectedLastReview"] = serialized(card.lastReview);
  std::string line;
  serializeJson(review, line);
  line += '\n';

  HalFile file = Storage.open(voice::kReviewsPath, O_WRONLY | O_CREAT | O_APPEND);
  if (!file.isOpen() || file.write(line.data(), line.size()) != line.size()) {
    LOG_ERR("CARDS", "Could not save a rating");  // The card stays up, so the rating is not silently lost.
    return;
  }
  file.close();
  index++;
  revealed = false;
  furigana = false;
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
  // Sticky exception to FreeInkUI (STICKY.md): the furigana card text has no FUI component.
  int x;
  int y;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  if (!cards[index].ruby.empty() && inside(toggleRect(), x, y)) {
    furigana = !furigana;
    requestUpdate();
    return;
  }
  if (!revealed) {
    revealed = true;
    requestUpdate();
    return;
  }
  // Four equal buttons, Again to Easy.
  const Rect bar = ratingBarRect();
  if (inside(bar, x, y)) rate((x - bar.x) * 4 / bar.width + 1);
}

// Runs once per card, so a repaint (reveal, furigana) only draws.
void CardsActivity::layoutCard(const int maxWidth) {
  laidOutIndex = index;
  const Card& card = cards[index];
  const auto wrap = [&](const std::string& text, std::vector<std::string>& out) {
    out.clear();
    size_t start = 0;
    while (start <= text.size()) {
      const size_t end = text.find('\n', start);
      const std::string line = text.substr(start, end == std::string::npos ? end : end - start);
      if (line.empty()) {
        out.emplace_back();  // An empty entry is a paragraph gap.
      } else {
        for (std::string& wrapped : renderer.wrappedText(cardFont, line.c_str(), maxWidth, 8)) {
          out.push_back(std::move(wrapped));
        }
      }
      if (end == std::string::npos) break;
      start = end + 1;
    }
  };
  wrap(card.front, frontLines);
  wrap(card.back, backLines);

  // Readings sit above their text. A segment with a reading never splits; plain text breaks per character.
  rubyLines.assign(1, {});
  if (card.ruby.empty()) return;
  int lineWidth = 0;
  const auto add = [&](std::string text, std::string reading) {
    const int w = std::max(renderer.getTextWidth(cardFont, text.c_str()),
                           reading.empty() ? 0 : renderer.getTextWidth(UI_10_FONT_ID, reading.c_str()));
    if (lineWidth + w > maxWidth && !rubyLines.back().empty()) {
      rubyLines.emplace_back();
      lineWidth = 0;
    }
    rubyLines.back().push_back({std::move(text), std::move(reading), w});
    lineWidth += w;
  };
  for (const auto& [text, reading] : card.ruby) {
    if (!reading.empty()) {
      add(text, reading);
      continue;
    }
    for (size_t i = 0; i < text.size();) {
      size_t n = 1;
      while (i + n < text.size() && (static_cast<unsigned char>(text[i + n]) & 0xC0) == 0x80) n++;
      add(text.substr(i, n), "");
      i += n;
    }
  }
}

int CardsActivity::drawLines(const std::vector<std::string>& lines, int y) const {
  for (const std::string& line : lines) {
    if (line.empty()) {
      y += renderer.getLineHeight(cardFont) / 2;
      continue;
    }
    renderer.drawCenteredText(cardFont, y, line.c_str());
    y += renderer.getLineHeight(cardFont) + kLineGap;
  }
  return y;
}

int CardsActivity::drawRubyLines(int y) const {
  const Rect content = UITheme::getContentArea(renderer);
  const int readingHeight = renderer.getLineHeight(UI_10_FONT_ID);
  for (const auto& line : rubyLines) {
    int total = 0;
    for (const Piece& piece : line) total += piece.width;
    int x = content.x + (content.width - total) / 2;
    for (const Piece& piece : line) {
      if (!piece.reading.empty()) {
        renderer.drawText(UI_10_FONT_ID,
                          x + (piece.width - renderer.getTextWidth(UI_10_FONT_ID, piece.reading.c_str())) / 2, y,
                          piece.reading.c_str());
      }
      renderer.drawText(cardFont, x + (piece.width - renderer.getTextWidth(cardFont, piece.text.c_str())) / 2,
                        y + readingHeight, piece.text.c_str());
      x += piece.width;
    }
    y += readingHeight + renderer.getLineHeight(cardFont) + kLineGap;
  }
  return y;
}

void CardsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect content = UITheme::getContentArea(renderer);
  if (index >= cards.size()) {
    renderer.drawCenteredText(
        UI_12_FONT_ID, content.y + content.height / 2,
        cards.empty() ? syncPending ? tr(STR_NOTES_SYNCING) : tr(STR_CARDS_NONE_DUE) : tr(STR_CARDS_ALL_DONE));
    renderer.displayBuffer();
    return;
  }

  char progress[24];
  snprintf(progress, sizeof(progress), "%u / %u", static_cast<unsigned>(index + 1),
           static_cast<unsigned>(cards.size()));
  renderer.drawCenteredText(UI_10_FONT_ID, content.y + kProgressY, progress);

  if (laidOutIndex != index) {
    cardFont = SETTINGS.getReaderFontId();
    const int larger = renderer.resolveFontVariant(cardFont, kCardFontPointSize / SETTINGS.fontPointSize);
    if (larger != 0) cardFont = larger;
    const int sidePadding = UITheme::getInstance().getMetrics().contentSidePadding;
    layoutCard(content.width - 2 * sidePadding);
  }

  const Card& card = cards[index];
  if (!card.ruby.empty()) {
    const Rect toggle = toggleRect();
    if (furigana) {
      renderer.fillRect(toggle.x, toggle.y, toggle.width, toggle.height);
    } else {
      renderer.drawRect(toggle.x, toggle.y, toggle.width, toggle.height);
    }
    const char* label = tr(STR_CARDS_FURIGANA);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
    renderer.drawText(UI_10_FONT_ID, toggle.x + (toggle.width - labelWidth) / 2, toggle.y + toggle.height / 2 - 10,
                      label, !furigana);
  }

  int y = content.y + kCardTop;
  y = furigana && !card.ruby.empty() ? drawRubyLines(y) : drawLines(frontLines, y);

  if (!revealed) {
    renderer.drawCenteredText(UI_10_FONT_ID, content.y + content.height - kHintFromBottom, tr(STR_CARDS_SHOW_ANSWER));
  } else {
    y += 20;
    renderer.drawLine(content.x + 40, y, content.x + content.width - 40, y);
    y += 30;
    drawLines(backLines, y);
    const Rect bar = ratingBarRect();
    for (int i = 0; i < 4; ++i) {
      const int x = bar.x + i * bar.width / 4;
      renderer.drawRect(x + 4, bar.y + 8, bar.width / 4 - 8, bar.height - 16);
      const char* label = I18n::getInstance().get(kLabels[i]);
      const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
      renderer.drawText(UI_10_FONT_ID, x + (bar.width / 4 - textWidth) / 2, bar.y + bar.height / 2 - 10, label);
    }
  }
  renderer.displayBuffer();
}
