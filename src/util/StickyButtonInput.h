#pragma once

// The shared Confirm/Power contact belongs to voice until its release.
class StickyButtonInput {
 public:
  enum class Action { None, Confirm, Record };
  static constexpr unsigned long HOLD_MS = 500;

  Action update(const bool down, const bool released, const unsigned long heldMs, const bool recording,
                const bool canRecord) {
    // Early-boot recording and a full recording must consume the same gesture.
    if (recording) holdConsumed = true;
    if (down) {
      if (holdConsumed || heldMs < HOLD_MS) return Action::None;
      holdConsumed = true;
      // File transfer cannot service capture; still suppress its long release.
      return canRecord ? Action::Record : Action::None;
    }
    const bool confirm = released && releasedSinceWake && !holdConsumed && heldMs < HOLD_MS;
    releasedSinceWake = true;
    holdConsumed = false;
    return confirm ? Action::Confirm : Action::None;
  }

 private:
  bool releasedSinceWake = false;
  bool holdConsumed = false;
};
