#include <gtest/gtest.h>

#include <numeric>
#include <vector>

#include "voice/VoiceFormat.h"

using namespace voice;

TEST(Wav, HeaderDescribes16BitMonoAtSampleRate) {
  uint8_t h[kWavHeaderBytes];
  writeWavHeader(h, 32000);
  EXPECT_EQ(0, memcmp(h, "RIFF", 4));
  EXPECT_EQ(0, memcmp(h + 8, "WAVEfmt ", 8));
  EXPECT_EQ(0, memcmp(h + 36, "data", 4));
  const auto u32 = [&](int at) { return h[at] | h[at + 1] << 8 | h[at + 2] << 16 | uint32_t(h[at + 3]) << 24; };
  EXPECT_EQ(32036u, u32(4));
  EXPECT_EQ(16000u, u32(24));
  EXPECT_EQ(32000u, u32(28));
  EXPECT_EQ(32000u, u32(40));
  EXPECT_EQ(1, h[22]);
  EXPECT_EQ(16, h[34]);
}

TEST(Wav, TwelveMinutesStaysUnderTheApiLimit) { EXPECT_LT(kMaxDataBytes + kWavHeaderBytes, 25u * 1024 * 1024); }

TEST(BookId, IsStableAndKeepsFieldsApart) {
  EXPECT_EQ(bookIdFor("Kokoro", "Soseki"), bookIdFor("Kokoro", "Soseki"));
  EXPECT_NE(bookIdFor("Kokoro", "Soseki"), bookIdFor("Kokoro", "Other"));
  EXPECT_NE(bookIdFor("ab", "c"), bookIdFor("a", "bc"));
  EXPECT_EQ(17u, bookIdFor("こころ", "").size());
}

TEST(Text, TruncateNeverSplitsACharacter) {
  EXPECT_EQ("abc", truncateUtf8("abc", 3));
  EXPECT_EQ("ab", truncateUtf8("abc", 2));
  EXPECT_EQ("あ", truncateUtf8("ああ", 4));
  EXPECT_EQ("", truncateUtf8("あ", 2));
}

TEST(Text, TitleFallsBackToFileName) {
  EXPECT_EQ("Kokoro", titleOrFileName("Kokoro", "/Books/x.epub"));
  EXPECT_EQ("x.epub", titleOrFileName("", "/Books/x.epub"));
  EXPECT_EQ("x.epub", titleOrFileName("", "x.epub"));
  EXPECT_EQ("Untitled", titleOrFileName("", "/Books/"));
}

TEST(Sent, ExpiresOnlyAfterThirtyDaysWithAKnownClock) {
  const uint32_t sent = 1800000000;
  const std::string name = sentFileName(sent, "abc");
  EXPECT_EQ("1800000000-abc.wav", name);
  EXPECT_FALSE(sentFileExpired(name.c_str(), sent + kKeepSentSeconds));
  EXPECT_TRUE(sentFileExpired(name.c_str(), sent + kKeepSentSeconds + 1));
  EXPECT_FALSE(sentFileExpired(name.c_str(), 1000));
  EXPECT_FALSE(sentFileExpired("notes.wav", sent * 2));
  EXPECT_FALSE(sentFileExpired("-abc.wav", sent * 2));
}

TEST(Weather, CoversEveryKnownRange) {
  EXPECT_STREQ("Clear", weatherLabel(0));
  EXPECT_STREQ("Cloudy", weatherLabel(3));
  EXPECT_STREQ("Fog", weatherLabel(45));
  EXPECT_STREQ("Rain", weatherLabel(61));
  EXPECT_STREQ("Rain", weatherLabel(81));
  EXPECT_STREQ("Snow", weatherLabel(75));
  EXPECT_STREQ("Snow", weatherLabel(86));
  EXPECT_STREQ("Storms", weatherLabel(95));
  EXPECT_STREQ("", weatherLabel(90));
}

TEST(Http, ParsesStatusLines) {
  EXPECT_EQ(200, parseHttpStatus("HTTP/1.1 200 OK\r\n"));
  EXPECT_EQ(413, parseHttpStatus("HTTP/2 413 \r\n"));
  EXPECT_EQ(-1, parseHttpStatus("<html>"));
  EXPECT_EQ(-1, parseHttpStatus("HTTP/1.1 2"));
  EXPECT_EQ(-1, parseHttpStatus("HTTP/1.1 abc"));
}

TEST(Ring, WrapsWithoutLosingOrder) {
  std::vector<int16_t> storage(8);
  SampleRing ring;
  ring.attach(storage.data(), storage.size());
  std::vector<int16_t> sent(1000), got;
  std::iota(sent.begin(), sent.end(), 0);
  size_t at = 0;
  while (got.size() < sent.size()) {
    if (at < sent.size()) {
      const size_t n = std::min<size_t>(5, sent.size() - at);
      ring.write(sent.data() + at, n);
      at += n;
    }
    const int16_t* p;
    while (const size_t n = ring.peek(&p, 3)) {
      got.insert(got.end(), p, p + n);
      ring.consume(n);
    }
  }
  EXPECT_EQ(sent, got);
  EXPECT_EQ(0u, ring.dropped());
}

TEST(Ring, CountsOverflowAndKeepsOldestSamples) {
  std::vector<int16_t> storage(4);
  SampleRing ring;
  ring.attach(storage.data(), storage.size());
  const int16_t samples[] = {1, 2, 3, 4, 5, 6};
  ring.write(samples, 6);
  EXPECT_EQ(2u, ring.dropped());
  const int16_t* p;
  ASSERT_EQ(4u, ring.peek(&p, 10));
  EXPECT_EQ(1, p[0]);
  EXPECT_EQ(4, p[3]);
}
