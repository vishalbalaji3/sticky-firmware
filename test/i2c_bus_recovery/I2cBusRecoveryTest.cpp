#include <gtest/gtest.h>

#include "hal/I2cBusRecovery.h"

namespace {
// A slave that holds SDA low until it has seen `releaseAfter` clocks (-1: never).
struct FakeBus {
  int releaseAfter;
  int clocks = 0;
  int stops = 0;
  bool sdaHigh() const { return releaseAfter >= 0 && clocks >= releaseAfter; }
  void clockScl() { ++clocks; }
  void stop() { ++stops; }
};
}  // namespace

TEST(I2cBusRecovery, IdleBusOnlyGetsStop) {
  FakeBus bus{0};
  EXPECT_EQ(0, clearI2cBus(bus));
  EXPECT_EQ(1, bus.stops);
}

TEST(I2cBusRecovery, ClocksUntilSlaveReleases) {
  FakeBus bus{4};
  EXPECT_EQ(4, clearI2cBus(bus));
  EXPECT_EQ(4, bus.clocks);
  EXPECT_EQ(1, bus.stops);
}

TEST(I2cBusRecovery, NineClocksIsEnough) {
  FakeBus bus{9};
  EXPECT_EQ(9, clearI2cBus(bus));
}

TEST(I2cBusRecovery, GivesUpAfterNineClocks) {
  FakeBus bus{-1};
  EXPECT_EQ(-1, clearI2cBus(bus));
  EXPECT_EQ(9, bus.clocks);
  EXPECT_EQ(0, bus.stops);
}
