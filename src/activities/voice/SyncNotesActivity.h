#pragma once

#include <string>

#include "activities/Activity.h"

// Connects to a saved network and uploads waiting voice notes.
class SyncNotesActivity final : public Activity {
 public:
  SyncNotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("SyncNotes", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Syncing; }

 private:
  enum class State : uint8_t { Connecting, Syncing, Done };

  void syncNow();

  State state = State::Connecting;
  std::string message = "Connecting...";
  bool wifiActivated = false;
  bool syncPending = false;
};
