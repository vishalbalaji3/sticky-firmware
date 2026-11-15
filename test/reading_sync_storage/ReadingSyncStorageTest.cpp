#include <gtest/gtest.h>

#include "activities/reader/EpubReaderUtils.h"

class ReadingSyncStorage : public testing::Test {
 protected:
  Epub book;
  void SetUp() override {
    fake::files.clear();
    fake::failWrite.clear();
  }
};

TEST_F(ReadingSyncStorage, LegacyProgressNeedsAnExplicitBaseline) {
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 2, 80, 400));
  EXPECT_FALSE(EpubReaderUtils::loadSyncState(book).known);
}

TEST_F(ReadingSyncStorage, LayoutSavePreservesBaselineAndDoesNotInventAnEdit) {
  readingsync::State clean{42, 3, true, false};
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 2, 80, 400, &clean));
  auto reopened = EpubReaderUtils::loadSyncState(book);
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 4, 160, 400, &reopened));
  reopened = EpubReaderUtils::loadSyncState(book);
  EXPECT_EQ(readingsync::reconcile(reopened, 42, 4), readingsync::Decision::Download);
  EXPECT_FALSE(reopened.dirty);
}

TEST_F(ReadingSyncStorage, OfflineRewindSurvivesReopenAndConflictsWithPhoneEdit) {
  readingsync::State state{42, 3, true, false};
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 20, 80, 4000, &state));
  state.dirty = true;  // Successful deliberate navigation; offline upload fails.
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 1, 2, 20, 100, &state));
  const auto reopened = EpubReaderUtils::loadSyncState(book);
  EXPECT_EQ(readingsync::reconcile(reopened, 42, 3), readingsync::Decision::Upload);
  EXPECT_EQ(readingsync::reconcile(reopened, 42, 4), readingsync::Decision::Choose);
}

TEST_F(ReadingSyncStorage, FailedCompanionWriteCannotMakeChangedProgressClean) {
  readingsync::State state{42, 3, true, false};
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 20, 80, 4000, &state));
  state.dirty = true;
  fake::failWrite = "/book/sync-state.bin.tmp";
  EXPECT_FALSE(EpubReaderUtils::saveProgress(book, 1, 2, 20, 100, &state));
  EXPECT_FALSE(EpubReaderUtils::loadSyncState(book).known);
}

TEST_F(ReadingSyncStorage, AcknowledgedUploadClearsDirtyOnlyAtNewRevision) {
  readingsync::State dirty{42, 3, true, true};
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 1, 2, 20, 100, &dirty));
  ASSERT_TRUE(EpubReaderUtils::saveSyncState(book, {42, 4, true, false}));
  EXPECT_EQ(readingsync::reconcile(EpubReaderUtils::loadSyncState(book), 42, 4), readingsync::Decision::Keep);
}

TEST_F(ReadingSyncStorage, CacheRebuildRestoresCurrentPositionAndBaseline) {
  readingsync::State dirty{42, 3, true, true};
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 20, 80, 4000, &dirty));
  auto state = EpubReaderUtils::loadSyncState(book);
  fake::files.clear();
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 20, 80, std::nullopt, &state));
  // The section-less exit retains the restored page and count.
  ASSERT_TRUE(EpubReaderUtils::saveProgress(book, 7, 20, 80, std::nullopt, &state));
  EXPECT_EQ(fake::files["/book/progress.bin"], (std::vector<uint8_t>{7, 0, 20, 0, 80, 0}));
  EXPECT_EQ(readingsync::reconcile(EpubReaderUtils::loadSyncState(book), 42, 3), readingsync::Decision::Upload);
}
