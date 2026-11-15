#pragma once

#include <Epub.h>
#include <Epub/PageLink.h>
#include <Logging.h>

#include <optional>
#include <vector>

#include "ProgressFile.h"
#include "ReadingSyncState.h"

namespace EpubReaderUtils {

inline readingsync::State loadSyncState(const Epub& epub) {
  uint8_t progress[10];
  uint8_t data[readingsync::RECORD_SIZE];
  size_t progressSize = 0;
  {
    HalFile file;
    if (!Storage.openFileForRead("SYNC", epub.getCachePath() + "/progress.bin", file)) return {};
    progressSize = file.read(progress, sizeof(progress));
  }
  if (progressSize != 4 && progressSize != 6 && progressSize != 10) return {};
  HalFile file;
  if (!Storage.openFileForRead("SYNC", epub.getCachePath() + "/sync-state.bin", file)) return {};
  const size_t size = file.read(data, sizeof(data));
  return readingsync::decode(data, size, progress, progressSize);
}

inline bool saveSyncState(const Epub& epub, const readingsync::State& state) {
  uint8_t progress[10];
  size_t size = 0;
  {
    HalFile file;
    if (!Storage.openFileForRead("SYNC", epub.getCachePath() + "/progress.bin", file)) return false;
    size = file.read(progress, sizeof(progress));
  }
  if (size != 4 && size != 6 && size != 10) return false;
  uint8_t data[readingsync::RECORD_SIZE];
  readingsync::encode(state, progress, size, data);
  return ProgressFile::writeAtomic(epub.getCachePath(), data, sizeof(data), "sync-state.bin");
}

// Persists reader progress for an EPUB to its cache directory. Returns true on success.
inline bool saveProgress(const Epub& epub, int spineIndex, int pageNumber, int pageCount,
                         std::optional<uint32_t> visibleTextOffset = std::nullopt,
                         const readingsync::State* syncState = nullptr) {
  if (spineIndex < 0 || spineIndex > 0xFFFF || pageNumber < 0 || pageNumber > 0xFFFF || pageCount < 0 ||
      pageCount > 0xFFFF) {
    LOG_ERR("ERS", "Progress values out of range: spine=%d page=%d count=%d", spineIndex, pageNumber, pageCount);
    return false;
  }
  uint8_t data[10];
  data[0] = spineIndex & 0xFF;
  data[1] = (spineIndex >> 8) & 0xFF;
  data[2] = pageNumber & 0xFF;
  data[3] = (pageNumber >> 8) & 0xFF;
  data[4] = pageCount & 0xFF;
  data[5] = (pageCount >> 8) & 0xFF;
  size_t dataSize = 6;
  if (visibleTextOffset.has_value()) {
    data[6] = *visibleTextOffset & 0xFF;
    data[7] = (*visibleTextOffset >> 8) & 0xFF;
    data[8] = (*visibleTextOffset >> 16) & 0xFF;
    data[9] = (*visibleTextOffset >> 24) & 0xFF;
    dataSize = sizeof(data);
  }
  if (!ProgressFile::writeAtomic(epub.getCachePath(), data, dataSize)) {
    return false;
  }
  if (syncState && !saveSyncState(epub, *syncState)) return false;
  LOG_DBG("ERS", "Progress saved: spine=%d offset=%u page=%d", spineIndex, visibleTextOffset.value_or(0), pageNumber);
  return true;
}

inline const PageLink* linkAtPoint(const std::vector<PageLink>& links, const int x, const int y, const int marginLeft,
                                   const int marginTop) {
  // Finger slop, plus a floor on the target width: a note marker is often a single superscript
  // digit only a few pixels wide. The box is never grown vertically beyond its own line, so
  // taps on the lines above and below still reach the page-turn zones.
  constexpr int TOUCH_SLOP = 6;
  constexpr int MIN_TOUCH_WIDTH = 28;
  const int pageX = x - marginLeft;
  const int pageY = y - marginTop;
  for (const auto& link : links) {
    if (link.contains(pageX, pageY, TOUCH_SLOP, MIN_TOUCH_WIDTH)) {
      return &link;
    }
  }
  return nullptr;
}

}  // namespace EpubReaderUtils
