#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Offline flashcard review. Cards come from the last sync, and ratings go back on the next one.
class CardsActivity final : public Activity {
 public:
  CardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Cards", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool preventAutoSleep() override { return syncPending; }
  void render(RenderLock&&) override;

 private:
  struct Card {
    std::string id;
    std::string front;
    std::string back;
    std::vector<std::pair<std::string, std::string>> ruby;  // Front as (text, reading) segments, when it has readings.
    std::string lastReview;  // Kept as raw JSON, because null and a large number must both round-trip.
  };

  struct Piece {
    std::string text;
    std::string reading;
    int width;
  };

  Rect toggleRect() const;
  Rect ratingBarRect() const;
  void layoutCard(int maxWidth);
  int drawLines(const std::vector<std::string>& lines, int y) const;
  int drawRubyLines(int y) const;
  void rate(int rating);
  void loadCards();
  void syncNow();

  std::vector<Card> cards;
  size_t index = 0;
  bool revealed = false;
  bool furigana = false;
  // The current card wrapped to the screen, built once per card by layoutCard().
  size_t laidOutIndex = SIZE_MAX;
  int cardFont = 0;
  std::vector<std::string> frontLines;  // An empty entry is a paragraph gap.
  std::vector<std::string> backLines;
  std::vector<std::vector<Piece>> rubyLines;
  bool syncPending = true;  // Opening syncs over a saved network, after the cached cards are on screen.
};
