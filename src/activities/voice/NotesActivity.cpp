#include "NotesActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>

#include "activities/ActivityManager.h"
#include "activities/reader/AutoProgressSync.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "network/SavedWifi.h"
#include "network/WifiPowerSaveGuard.h"
#include "voice/SecureRequest.h"
#include "voice/VoiceFormat.h"
#include "voice/VoiceNotes.h"
#include "voice/VoiceUpload.h"

namespace fui = freeink::ui;

namespace {
// Pure sync policies return English labels; translate only at the UI boundary.
const char* notesLabel(const char* label) {
  if (!label) return nullptr;
  if (strcmp(label, "Syncing...") == 0) return tr(STR_NOTES_SYNCING);
  if (strcmp(label, "Transcribing...") == 0) return tr(STR_NOTES_TRANSCRIBING);
  if (strcmp(label, "Uploading audio...") == 0) return tr(STR_NOTES_UPLOADING_AUDIO);
  if (strcmp(label, "Transcription failed") == 0) return tr(STR_NOTES_TRANSCRIPTION_FAILED);
  if (strcmp(label, "Not set up") == 0) return tr(STR_NOTES_NOT_CONFIGURED);
  if (strcmp(label, "Token refused") == 0) return tr(STR_NOTES_TOKEN_REFUSED);
  if (strcmp(label, "Not synced") == 0) return tr(STR_NOTES_NOT_SYNCED);
  return label;
}
}  // namespace

int NotesActivity::loadRows() {
  rows.clear();
  int pending = 0;
  std::string text;
  JsonDocument doc;
  // 50 notes at the API's limits. A missing file just means the first sync has not happened.
  if (voice::readTextFile("/.sticky/transcripts.json", text, 256 * 1024) && !deserializeJson(doc, text)) {
    for (JsonObjectConst note : doc["notes"].as<JsonArrayConst>()) {
      if (rows.size() >= kMaxRows) break;
      const char* transcript = note["text"] | "";
      const char* quote = note["quote"] | "";
      const char* noteStatus = note["status"] | "";
      if (voice::transcriptPending(noteStatus)) pending++;
      Row row;
      row.title = note["bookTitle"] | "Inbox";
      const int page = note["page"] | -1;
      if (page >= 0) row.title += " - p." + std::to_string(page + 1);
      row.preview =
          voice::truncateUtf8(*transcript ? transcript : notesLabel(voice::pendingNoteLabel(noteStatus)), 120);
      row.body = *quote ? std::string("\"") + quote + "\"\n\n" : std::string();
      row.body += *transcript ? transcript : tr(STR_NOTES_NO_TEXT);
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
  return pending;
}

void NotesActivity::onEnter() {
  loadRows();
  UiListActivity::onEnter();
}

void NotesActivity::onExit() {
  if (WiFi.getMode() != WIFI_MODE_NULL) stopWifi();
  UiListActivity::onExit();
}

void NotesActivity::loop() {
  // The cached list and "Syncing..." go up before the blocking join.
  if (sync == Sync::Start && !voice::isRecording() && activityManager.currentScreenPainted()) {
    syncNow();
    return;
  }
  if (sync == Sync::Recheck && !voice::isRecording() && static_cast<long>(millis() - recheckAt) >= 0)
    recheckTranscripts();
  UiListActivity::loop();
}

void NotesActivity::syncNow() {
  int pending = 0;
  {
    // The card shares SPI with the display, and the rows change under the render task.
    RenderLock lock(*this);
    voice::Config config;
    if (!voice::loadConfig(config)) {
      status = voice::notesSyncStatus(false, true, false, false, 0);
    } else {
      const bool wasConnected = WiFi.status() == WL_CONNECTED;
      const bool online = wasConnected || (hasSavedWifi() && joinSavedWifi(autosync::kJoinTimeoutMs));
      bool saved = false;
      voice::UploadResult result;
      if (online) {
        WifiPowerSaveGuard psGuard;
        result = voice::syncNotes(saved);
      }
      pending = loadRows();
      status = voice::notesSyncStatus(online, result.unconfigured, result.unauthorized, saved, pending);
      if (!online || !saved) pending = 0;  // nothing to recheck without a working connection
    }
  }
  if (voice::shouldRecheckTranscripts(pending, rechecks)) {
    sync = Sync::Recheck;
    recheckAt = millis() + voice::kTranscriptRecheckMs;
  } else {
    finishSync();
  }
  requestUpdate();
}

void NotesActivity::recheckTranscripts() {
  rechecks++;
  int pending = 0;
  {
    RenderLock lock(*this);
    WifiPowerSaveGuard psGuard;
    const bool saved = voice::downloadTranscripts();
    pending = loadRows();
    status = voice::notesSyncStatus(true, false, false, saved, pending);
    if (!saved) pending = 0;
  }
  if (voice::shouldRecheckTranscripts(pending, rechecks)) {
    recheckAt = millis() + voice::kTranscriptRecheckMs;
  } else {
    finishSync();
  }
  requestUpdate();
}

void NotesActivity::finishSync() {
  sync = Sync::Idle;
  // Off even if it was on before, as Sync Notes does: the recorder needs it off.
  if (WiFi.getMode() != WIFI_MODE_NULL) stopWifi();
}

void NotesActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, "Notes",
                 notesLabel(status));
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
    screen.centeredText(tr(STR_NOTES_EMPTY), screen.theme().bodyText);
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
  if (sync == Sync::Recheck) finishSync();  // this loop pauses while the note is open
  auto detail =
      makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, rows[index].title, rows[index].body);
  if (!detail) return;
  startActivityForResult(std::move(detail), [this](const ActivityResult&) { requestUpdate(); });
}
