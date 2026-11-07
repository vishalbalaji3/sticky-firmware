#pragma once

#include <array>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Recent voice notes with their transcripts, as of the last sync.
class NotesActivity final : public UiListActivity {
 public:
  NotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Notes", renderer, mappedInput) {}
  void onEnter() override;

 private:
  struct Row {
    std::string title;
    std::string preview;
    std::string body;
  };

  int listCount() const override { return static_cast<int>(rows.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Notes"; }

  static constexpr size_t kMaxRows = 50;  // Matches what the API returns.
  std::vector<Row> rows;
  std::array<freeink::ui::ListItem, kMaxRows> items;
};
