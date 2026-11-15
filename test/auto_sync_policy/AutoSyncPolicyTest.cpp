#include <gtest/gtest.h>

#include "activities/reader/AutoSyncPolicy.h"
#include "network/SavedWifi.h"

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

TEST(AutoSyncPolicy, NoPullWhenReturningFromSync) { EXPECT_FALSE(shouldPullOnOpen(true, Status{}, kBook, true, kNow)); }

TEST(AutoSyncPolicy, ReopeningChecksForImmediatePhoneChanges) {
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, kNow - 30), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, kNow - 120), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(bookKey("/other.epub"), kNow - 30), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, Status{kBook, Result::Failed, kNow - 30}, kBook, false, kNow));
  // Unknown clock or a clock that went backwards: pull rather than trust the age.
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, 0), kBook, false, kNow));
  EXPECT_TRUE(shouldPullOnOpen(true, ok(kBook, kNow + 30), kBook, false, kNow));
}

// Leave on this device (push), read on in another app, reopen: the pull must run.
TEST(AutoSyncPolicy, ExitPushDoesNotSkipPull) {
  EXPECT_TRUE(shouldPullOnOpen(true, Status{kBook, Result::Pushed, kNow - 30}, kBook, false, kNow));
  EXPECT_EQ("Synced just now", statusText(true, Status{kBook, Result::Pushed, kNow - 30}, kBook, kNow));
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

// Offline reading here, then Readest read further, then the book is left online: keep Readest's position.
TEST(AutoSyncPolicy, ExitPushNeverOverwritesFurtherRemote) {
  EXPECT_FALSE(shouldPushOverRemote(true, true, 0.60f, 0.45f));
  EXPECT_TRUE(shouldPushOverRemote(true, true, 0.40f, 0.45f));  // ours is further
  EXPECT_TRUE(shouldPushOverRemote(true, true, 0.45f, 0.45f));
  EXPECT_TRUE(shouldPushOverRemote(true, false, 0.0f, 0.45f));    // nothing stored yet
  EXPECT_FALSE(shouldPushOverRemote(false, false, 0.0f, 0.45f));  // remote unknown
}

TEST(SavedWifiOrder, SavedInRangeStrongestFirstOncePerSsid) {
  const std::vector<ScannedNetwork> scan = {
      {"cafe", -40}, {"home", -70}, {"phone", -55}, {"home", -60}, {"neighbour", -30}};
  EXPECT_EQ((std::vector<size_t>{2, 3}), savedJoinOrder(scan, {"home", "phone", "office"}));
  EXPECT_TRUE(savedJoinOrder(scan, {"office"}).empty());
  EXPECT_TRUE(savedJoinOrder({}, {"home"}).empty());
}

#include "activities/reader/ReadingSyncState.h"

TEST(ReadingReconciliation, PhoneRewindDownloadsWhenLocalHasNoEdits) {
  const readingsync::State baseline{42, 1, true, false};
  EXPECT_EQ(readingsync::reconcile(baseline, 42, 2), readingsync::Decision::Download);
}

TEST(ReadingReconciliation, SamePositionsNeedDifferentDecisionsForDifferentAncestry) {
  // Local 2%, remote 72%: a local rewind is safe only against its shared baseline.
  const readingsync::State localRewind{42, 1, true, true};
  EXPECT_EQ(readingsync::reconcile(localRewind, 42, 1), readingsync::Decision::Upload);
  EXPECT_EQ(readingsync::reconcile(localRewind, 42, 2), readingsync::Decision::Choose);
}

TEST(ReadingReconciliation, UnknownOrChangedAccountNeverOverwritesAutomatically) {
  EXPECT_EQ(readingsync::reconcile({42, 3, true, false}, 42, 1), readingsync::Decision::Choose);
  EXPECT_EQ(readingsync::reconcile({}, 42, 1), readingsync::Decision::Choose);
  EXPECT_EQ(readingsync::reconcile({42, 1, true, false}, 43, 1), readingsync::Decision::Choose);
  EXPECT_EQ(readingsync::reconcile({42, 1, true, true}, 42, std::nullopt), readingsync::Decision::Choose);
  EXPECT_EQ(readingsync::reconcile({42, 1, true, false}, 42, 1), readingsync::Decision::Keep);
}

TEST(ReadingReconciliation, OfflineEditsSurviveReopenButTornAssociationsAreUnknown) {
  const uint8_t progress[] = {1, 0, 5, 0, 10, 0};
  uint8_t record[readingsync::RECORD_SIZE];
  readingsync::encode({42, 3, true, true}, progress, sizeof(progress), record);
  auto restored = readingsync::decode(record, sizeof(record), progress, sizeof(progress));
  EXPECT_EQ(readingsync::reconcile(restored, 42, 3), readingsync::Decision::Upload);
  EXPECT_EQ(readingsync::reconcile(restored, 42, 4), readingsync::Decision::Choose);
  const uint8_t different[] = {1, 0, 6, 0, 10, 0};
  EXPECT_FALSE(readingsync::decode(record, sizeof(record), different, sizeof(different)).known);
  record[5] = 0;
  EXPECT_FALSE(readingsync::decode(record, sizeof(record), progress, sizeof(progress)).known);
  EXPECT_FALSE(readingsync::decode(record, 12, progress, sizeof(progress)).known);
}

TEST(AutoSyncPolicy, CompletedBookWithoutSectionStillPullsPhoneRewinds) {
  EXPECT_TRUE(canPullAfterRender(-1, false, true, true));
  EXPECT_TRUE(canPullAfterRender(-1, true, false, true));
  EXPECT_FALSE(canPullAfterRender(-1, false, false, true));
  EXPECT_FALSE(canPullAfterRender(-1, false, true, false));
  EXPECT_FALSE(canPullAfterRender(12, false, true, true));
}
