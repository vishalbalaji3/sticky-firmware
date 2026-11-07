#include <gtest/gtest.h>

#include "src/activities/boot_sleep/SleepWakePolicy.h"

using sleepwake::StickyScreen;

TEST(SleepWakePolicy, TimeoutKeepsTheFrameAndLockShowsTheDashboard) {
  EXPECT_EQ(sleepwake::stickyScreenFor(true), StickyScreen::KeepFrame);
  EXPECT_EQ(sleepwake::stickyScreenFor(false), StickyScreen::Dashboard);
}

TEST(SleepWakePolicy, WakeReopensTheBookItSleptIn) {
  EXPECT_TRUE(sleepwake::wakeReopensBook(true, true, false, false));
}

TEST(SleepWakePolicy, WakeGoesHomeOtherwise) {
  EXPECT_FALSE(sleepwake::wakeReopensBook(false, true, false, false));  // no book open
  EXPECT_FALSE(sleepwake::wakeReopensBook(true, false, false, false));  // slept outside the reader
  EXPECT_FALSE(sleepwake::wakeReopensBook(true, true, true, false));    // Back held on wake
  EXPECT_FALSE(sleepwake::wakeReopensBook(true, true, false, true));    // last reader open crashed
}

TEST(SleepWakePolicy, WakeHoldRecordsAndClickDoesNot) {
  using sleepwake::WakeVoice;
  EXPECT_EQ(sleepwake::wakeVoiceFor(true, sleepwake::kWakeVoiceHoldMs), WakeVoice::Record);
  EXPECT_EQ(sleepwake::wakeVoiceFor(true, sleepwake::kWakeVoiceHoldMs - 1), WakeVoice::Wait);
  EXPECT_EQ(sleepwake::wakeVoiceFor(false, 50), WakeVoice::No);  // released early: a click
  EXPECT_EQ(sleepwake::wakeVoiceFor(false, 5000), WakeVoice::No);
}
