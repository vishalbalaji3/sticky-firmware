#include <gtest/gtest.h>

#include "remote/WsFrame.h"

using namespace remote;

// The worked example from RFC 6455 section 5.7.
TEST(WsFrame, MatchesTheRfcExample) {
  const std::string frame = encodeTextFrame("Hello", 0x37FA213D);
  const std::string expected("\x81\x85\x37\xfa\x21\x3d\x7f\x9f\x4d\x51\x58", 11);
  EXPECT_EQ(expected, frame);
}

TEST(WsFrame, LongPayloadUsesTwoLengthBytes) {
  const std::string frame = encodeTextFrame(std::string(300, 'a'), 0);
  EXPECT_EQ(static_cast<char>(0x80 | 126), frame[1]);
  EXPECT_EQ(1, frame[2]);
  EXPECT_EQ(44, frame[3]);
  EXPECT_EQ(300u + 8u, frame.size());
}

TEST(Grid, MapsTapsToCells) {
  EXPECT_EQ(0, gridCell(0, 100, 100, 480, 600, 3, 5));
  EXPECT_EQ(2, gridCell(479, 100, 100, 480, 600, 3, 5));
  EXPECT_EQ(3, gridCell(0, 220, 100, 480, 600, 3, 5));
  EXPECT_EQ(14, gridCell(479, 699, 100, 480, 600, 3, 5));
  EXPECT_EQ(-1, gridCell(10, 99, 100, 480, 600, 3, 5));
  EXPECT_EQ(-1, gridCell(10, 700, 100, 480, 600, 3, 5));
}
