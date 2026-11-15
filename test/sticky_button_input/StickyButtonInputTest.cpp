#include <gtest/gtest.h>

#include "src/util/StickyButtonInput.h"

using Action = StickyButtonInput::Action;

TEST(StickyButtonInput, WakeClickIsConsumedThenNewClickConfirms) {
  StickyButtonInput input;
  EXPECT_EQ(input.update(false, true, 50, false, true), Action::None);
  EXPECT_EQ(input.update(true, false, 0, false, true), Action::None);
  EXPECT_EQ(input.update(false, true, 50, false, true), Action::Confirm);
  EXPECT_EQ(input.update(false, false, 50, false, true), Action::None);
}

TEST(StickyButtonInput, HoldRecordsOnceAndNeverConfirmsOnRelease) {
  StickyButtonInput input;
  input.update(false, false, 0, false, true);
  EXPECT_EQ(input.update(true, false, 499, false, true), Action::None);
  EXPECT_EQ(input.update(true, false, 500, false, true), Action::Record);
  EXPECT_EQ(input.update(true, false, 600, true, true), Action::None);
  EXPECT_EQ(input.update(false, true, 650, true, true), Action::None);
  EXPECT_EQ(input.update(false, true, 50, false, true), Action::Confirm);
}

TEST(StickyButtonInput, FailedOrFullRecordingDoesNotRestartWhileHeld) {
  for (const bool started : {false, true}) {
    StickyButtonInput input;
    input.update(false, false, 0, false, true);
    EXPECT_EQ(input.update(true, false, 500, false, true), Action::Record);
    input.update(true, false, 600, started, true);
    EXPECT_EQ(input.update(true, false, 1000, false, true), Action::None);
    EXPECT_EQ(input.update(false, true, 1100, false, true), Action::None);
  }
}

TEST(StickyButtonInput, EarlyBootCaptureOwnsHoldThroughAutomaticStop) {
  StickyButtonInput input;
  EXPECT_EQ(input.update(true, false, 100, true, true), Action::None);
  EXPECT_EQ(input.update(true, false, 1000, false, true), Action::None);
  EXPECT_EQ(input.update(false, true, 1100, false, true), Action::None);
}

TEST(StickyButtonInput, BlockedCaptureStillConsumesHold) {
  StickyButtonInput input;
  input.update(false, false, 0, false, true);
  EXPECT_EQ(input.update(true, false, 500, false, false), Action::None);
  EXPECT_EQ(input.update(true, false, 600, false, true), Action::None);
  EXPECT_EQ(input.update(false, true, 650, false, true), Action::None);
}

TEST(StickyButtonInput, ReleaseAfterBlockedLoopDoesNotBecomeClick) {
  StickyButtonInput input;
  input.update(false, false, 0, false, true);
  EXPECT_EQ(input.update(false, true, 500, false, true), Action::None);
  EXPECT_EQ(input.update(false, true, 499, false, true), Action::Confirm);
}
