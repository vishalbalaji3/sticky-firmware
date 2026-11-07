#include <gtest/gtest.h>

#include "activities/reader/AutoSyncPolicy.h"

using namespace autosync;

namespace {
const uint32_t kBook = bookKey("/books/a.epub");
constexpr int64_t kNow = 1'800'000'000;

Status ok(const uint32_t key, const int64_t at) { return Status{key, Result::Ok, at}; }
}  // namespace

TEST(AutoSyncPolicy, BookKeyDistinguishesPaths) {
  EXPECT_EQ(kBook, bookKey("/books/a.epub"));
  EXPECT_NE(kBook, bookKey("/books/b.epub"));
}

TEST(AutoSyncPolicy, PullNeedsCredentials) {
  EXPECT_FALSE(shouldPullOnOpen(false, Status{}, kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, Status{}, kBook, false, kNow));
}

TEST(AutoSyncPolicy, NoPullWhenReturningFromSync) {
  EXPECT_FALSE(shouldPullOnOpen(true, Status{}, kBook, true, kNow));
}

TEST(AutoSyncPolicy, SkipsPullRightAfterSuccessfulSync) {
  EXPECT_FALSE(shouldPullOnOpen(true, ok(kBook, kNow - 30), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, kNow - kRecentSyncSeconds), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(bookKey("/other.epub"), kNow - 30), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, Status{kBook, Result::Failed, kNow - 30}, kBook, false, kNow));
  // Unknown clock or a clock that went backwards: pull rather than trust the age.
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, 0), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, kNow + 30), kBook, false, kNow));
}

TEST(AutoSyncPolicy, PushOnlyWhenMoved) {
  EXPECT_FALSE(shouldPushOnExit(true, 3, 7, 3, 7));
  EXPECT_TRUE(shouldPushOnExit(true, 3, 7, 3, 8));
  EXPECT_TRUE(shouldPushOnExit(true, 3, 7, 4, 0));
  EXPECT_FALSE(shouldPushOnExit(false, 3, 7, 4, 0));
  EXPECT_FALSE(shouldPushOnExit(true, -1, 0, 4, 0));  // never shown a page
}

TEST(AutoSyncPolicy, StatusText) {
  EXPECT_EQ("Sync off (set up in Settings)", statusText(false, ok(kBook, kNow), kBook, kNow));
  EXPECT_EQ("Not synced", statusText(true, Status{}, kBook, kNow));
  EXPECT_EQ("Not synced", statusText(true, Status{kBook, Result::Failed, kNow}, kBook, kNow));
  EXPECT_EQ("Not synced", statusText(true, ok(bookKey("/other.epub"), kNow), kBook, kNow));
  EXPECT_EQ("Synced", statusText(true, ok(kBook, 0), kBook, kNow));
  EXPECT_EQ("Synced", statusText(true, ok(kBook, kNow), kBook, 0));
  EXPECT_EQ("Synced just now", statusText(true, ok(kBook, kNow - 59), kBook, kNow));
  EXPECT_EQ("Synced just now", statusText(true, ok(kBook, kNow + 5), kBook, kNow));
  EXPECT_EQ("Synced 2 min ago", statusText(true, ok(kBook, kNow - 150), kBook, kNow));
  EXPECT_EQ("Synced 3 h ago", statusText(true, ok(kBook, kNow - 3 * 3600 - 10), kBook, kNow));
  EXPECT_EQ("Synced 2 d ago", statusText(true, ok(kBook, kNow - 2 * 86400), kBook, kNow));
}
