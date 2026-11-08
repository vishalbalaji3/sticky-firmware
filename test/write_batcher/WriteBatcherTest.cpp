#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "src/network/WriteBatcher.h"

namespace {
struct Recorder {
  std::string bytes;
  std::vector<size_t> writes;
  bool ok = true;
  WriteBatcher::Sink sink() {
    return [this](const uint8_t* d, size_t n) {
      bytes.append(reinterpret_cast<const char*>(d), n);
      writes.push_back(n);
      return ok;
    };
  }
};

bool put(WriteBatcher& b, const std::string& s) {
  return b.write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}
}  // namespace

TEST(WriteBatcher, GathersWritesIntoFullBlocks) {
  Recorder r;
  uint8_t block[4];
  WriteBatcher b(block, sizeof(block), r.sink());
  EXPECT_TRUE(put(b, "ab"));
  EXPECT_TRUE(put(b, "cdefghij"));  // spans two blocks
  EXPECT_TRUE(put(b, "k"));
  EXPECT_EQ(r.writes, (std::vector<size_t>{4, 4}));
  EXPECT_TRUE(b.flush());
  EXPECT_EQ(r.bytes, "abcdefghijk");
  EXPECT_EQ(r.writes.back(), 3u);
  EXPECT_TRUE(b.flush());  // nothing held: no empty write
  EXPECT_EQ(r.writes.size(), 3u);
}

TEST(WriteBatcher, DiscardDropsHeldBytes) {
  Recorder r;
  uint8_t block[8];
  WriteBatcher b(block, sizeof(block), r.sink());
  put(b, "old");
  b.discard();
  put(b, "new");
  b.flush();
  EXPECT_EQ(r.bytes, "new");
}

TEST(WriteBatcher, NoBlockPassesWritesThrough) {
  Recorder r;
  WriteBatcher b(nullptr, 4096, r.sink());
  put(b, "ab");
  put(b, "c");
  EXPECT_EQ(r.writes, (std::vector<size_t>{2, 1}));
}

TEST(WriteBatcher, ReportsAFailedWrite) {
  Recorder r;
  r.ok = false;
  uint8_t block[2];
  WriteBatcher b(block, sizeof(block), r.sink());
  EXPECT_FALSE(put(b, "abc"));
}
