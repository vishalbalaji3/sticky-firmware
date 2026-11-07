#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"

// Offline flashcard review. Cards come from the last sync, and ratings go back on the next one.
class CardsActivity final : public Activity {
 public:
  CardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Cards", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Card {
    std::string id;
    std::string front;
    std::string back;
    std::string lastReview;  // Kept as raw JSON, because null and a large number must both round-trip.
  };

  void rate(int rating);

  std::vector<Card> cards;
  size_t index = 0;
  bool revealed = false;
};
