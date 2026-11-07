#include "NotesActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <Memory.h>

#include "activities/reader/DictionaryDefinitionActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "voice/SecureRequest.h"
#include "voice/VoiceFormat.h"

namespace fui = freeink::ui;

void NotesActivity::onEnter() {
  rows.clear();
  std::string text;
  JsonDocument doc;
  // 50 notes at the API's limits. A missing file just means the first sync has not happened.
  if (voice::readTextFile("/.sticky/transcripts.json", text, 256 * 1024) && !deserializeJson(doc, text)) {
    for (JsonObjectConst note : doc["notes"].as<JsonArrayConst>()) {
      if (rows.size() >= kMaxRows) break;
      const char* transcript = note["text"] | "";
      const char* quote = note["quote"] | "";
      const char* status = note["status"] | "";
      Row row;
      row.title = note["bookTitle"] | "Inbox";
      const int page = note["page"] | -1;
      if (page >= 0) row.title += " - p." + std::to_string(page + 1);
      row.preview = voice::truncateUtf8(*transcript ? transcript : status, 120);
      row.body = *quote ? std::string("\"") + quote + "\"\n\n" : std::string();
      row.body += *transcript ? transcript : "Not transcribed yet. Sync again in a moment.";
      rows.push_back(std::move(row));
    }
  }
  for (size_t i = 0; i < rows.size(); ++i) {
    fui::ListItem item;
    item.label = rows[i].title.c_str();
    item.subtitle = rows[i].preview.c_str();
    item.actionValue = static_cast<int16_t>(i);
    items[i] = item;
  }
  UiListActivity::onEnter();
}

void NotesActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (rows.empty()) {
    screen.centeredText("No transcripts yet. Use Sync Notes.", screen.theme().bodyText);
    return;
  }
  fui::ListProps props;
  props.count = static_cast<uint16_t>(rows.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  props.items = items.data();
  props.itemsWindowFirst = 0;
  props.itemsWindowCount = static_cast<uint16_t>(rows.size());
  screen.list(props);
}

void NotesActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  auto detail =
      makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, rows[index].title, rows[index].body);
  if (!detail) return;
  startActivityForResult(std::move(detail), [this](const ActivityResult&) { requestUpdate(); });
}
