#pragma once

// Sleep and wake decisions, free of hardware so host tests can cover them.
namespace sleepwake {

// Sticky: an inactivity timeout keeps the shown frame and adds a badge; Up + Down locks to the dashboard.
enum class StickyScreen { KeepFrame, Dashboard };

constexpr StickyScreen stickyScreenFor(const bool fromTimeout) {
  return fromTimeout ? StickyScreen::KeepFrame : StickyScreen::Dashboard;
}

// A sleep wake reopens the book only if it slept in the reader and the last reader open did not crash.
constexpr bool wakeReopensBook(const bool hasOpenBook, const bool sleptInReader, const bool backHeld,
                               const bool readerCrashed) {
  return hasOpenBook && sleptInReader && !backHeld && !readerCrashed;
}

// Sticky: a third-button wake still held this long after boot records a voice note; an earlier release is a click.
constexpr unsigned long kWakeVoiceHoldMs = 300;

enum class WakeVoice { No, Wait, Record };

constexpr WakeVoice wakeVoiceFor(const bool held, const unsigned long sinceBootMs) {
  if (!held) return WakeVoice::No;
  return sinceBootMs >= kWakeVoiceHoldMs ? WakeVoice::Record : WakeVoice::Wait;
}

// Sticky: the auto-sleep moon sits beside the battery the screen already shows, on the side away from the edge,
// centred on it.
struct Box {
  int x;
  int y;
  int width;
  int height;
};

constexpr Box moonBesideBattery(const Box battery, const bool moonOnLeft, const int moon, const int gap) {
  return Box{moonOnLeft ? battery.x - gap - moon : battery.x + battery.width + gap,
             battery.y + (battery.height - moon) / 2, moon, moon};
}

}  // namespace sleepwake
