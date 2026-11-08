#pragma once

#include <array>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Recent voice notes with their transcripts. Opening it syncs over the saved network.
class NotesActivity final : public UiListActivity {
 public:
  NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Notes", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool preventAutoSleep() override { return sync != Sync::Idle; }

 private:
  struct Row {
    std::string title;
    std::string preview;
    std::string body;
  };
  // Sync runs in loop() after the cached list is on screen; Recheck waits for fresh transcripts.
  enum class Sync : uint8_t { Start, Recheck, Idle };

  int listCount() const override { return static_cast<int>(rows.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void drawChrome() override;
  // Returns how many notes are still being transcribed.
  int loadRows();
  void syncNow();
  void recheckTranscripts();
  void finishSync();

  static constexpr size_t kMaxRows = 50;  // Matches what the API returns.
  std::vector<Row> rows;
  std::array<freeink::ui::ListItem, kMaxRows> items;
  Sync sync = Sync::Start;
  const char* status = "Syncing...";
  int rechecks = 0;
  unsigned long recheckAt = 0;
};
