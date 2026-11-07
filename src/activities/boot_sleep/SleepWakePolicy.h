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

}  // namespace sleepwake
