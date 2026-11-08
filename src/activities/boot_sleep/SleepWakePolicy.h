#pragma once

#include <string_view>

// Sleep and wake decisions, free of hardware so host tests can cover them.
namespace sleepwake {

// Sticky: an inactivity timeout in a book keeps the page and adds a badge; anywhere else, and Up + Down, locks to
// the dashboard (a wake outside the reader goes home anyway, so the old frame has nothing to resume).
enum class StickyScreen { KeepFrame, Dashboard };

constexpr StickyScreen stickyScreenFor(const bool fromTimeout, const bool inReader) {
  return fromTimeout && inReader ? StickyScreen::KeepFrame : StickyScreen::Dashboard;
}

// A sleep wake reopens the book only if it slept in the reader and the last reader open did not crash.
constexpr bool wakeReopensBook(const bool hasOpenBook, const bool sleptInReader, const bool backHeld,
                               const bool readerCrashed) {
  return hasOpenBook && sleptInReader && !backHeld && !readerCrashed;
}

// A wake outside the reader returns to the app it slept in. Its sub-screens (a settings page, an OPDS feed)
// reopen at that app's top; screens that need a live session (File Transfer, a sync) go home.
enum class WakeApp { Home, Library, Notes, Cards, Settings, FileBrowser };

constexpr WakeApp wakeAppFor(const std::string_view sleptIn) {
  if (sleptIn == "Library" || sleptIn == "OpdsBookBrowser") return WakeApp::Library;
  if (sleptIn == "Notes") return WakeApp::Notes;
  if (sleptIn == "Cards") return WakeApp::Cards;
  if (sleptIn == "Settings" || sleptIn == "SettingsList") return WakeApp::Settings;
  if (sleptIn == "FileBrowser") return WakeApp::FileBrowser;
  return WakeApp::Home;
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
