#include "SyncNotesActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <ctime>
#include <memory>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/Timezones.h"
#include "voice/VoiceFormat.h"
#include "voice/VoiceUpload.h"

void SyncNotesActivity::onEnter() {
  Activity::onEnter();
  wifiActivated = true;
  if (WiFi.status() == WL_CONNECTED) {
    syncPending = true;
    state = State::Syncing;
    message = "Uploading notes...";
    requestUpdate();
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             state = State::Done;
                             message = "Not connected. Your notes are safe on the card.";
                           } else {
                             WiFi.setSleep(false);
                             syncPending = true;
                             state = State::Syncing;
                             message = "Uploading notes...";
                           }
                           requestUpdate(true);
                         });
}

void SyncNotesActivity::onExit() {
  Activity::onExit();
  if (wifiActivated) {
    WiFi.disconnect(false);
    delay(30);
    WiFi.mode(WIFI_OFF);
  }
}

void SyncNotesActivity::syncNow() {
  // Sets the clock for capture times and for expiring old audio.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 50 && time(nullptr) < voice::kFirstValidUnixTime; ++i) delay(100);
  timezones::applyToClock();  // configTime() replaced the zone the rest of the UI displays in.

  const voice::UploadResult result = voice::uploadQueuedNotes();
  voice::pruneSentNotes();
  // Fresh uploads need a moment to transcribe, so wait briefly for them to appear in the list.
  if (result.uploaded > 0) delay(6000);
  if (!result.unconfigured && !result.unreachable && !result.unauthorized) {
    voice::downloadTranscripts();
    voice::syncCardsAndHome();
  }

  if (result.unconfigured) {
    message = "Add /.sticky/config.json to the card first.";
  } else if (result.unauthorized) {
    message = "Device token was refused. Create a new one in the app.";
  } else if (result.unreachable) {
    message = "Network needs login or is offline. Notes will upload later.";
  } else if (result.remaining > 0) {
    message =
        std::to_string(result.uploaded) + " uploaded, " + std::to_string(result.remaining) + " refused by the server.";
  } else {
    message = result.uploaded == 0 ? "No notes waiting." : std::to_string(result.uploaded) + " uploaded.";
  }
  state = State::Done;
  requestUpdate(true);
}

void SyncNotesActivity::loop() {
  if (syncPending) {
    syncPending = false;
    syncNow();
    return;
  }
  if (state == State::Done && (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
                               mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    activityManager.goHome();
  }
}

void SyncNotesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  renderer.drawCenteredText(UI_12_FONT_ID, renderer.getScreenHeight() / 2 - 20, "Sync Notes", true,
                            EpdFontFamily::BOLD);
  const auto lines = renderer.wrappedText(UI_10_FONT_ID, message.c_str(), renderer.getScreenWidth() - 40, 4);
  int y = renderer.getScreenHeight() / 2 + 20;
  for (const auto& line : lines) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
  renderer.displayBuffer();
}
