#include "AutoProgressSync.h"

#include <HalStorage.h>
#include <Logging.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <esp_attr.h>

#include "KOReaderCredentialStore.h"
#include "KOReaderDocumentId.h"
#include "network/SavedWifi.h"
#include "network/WifiPowerSaveGuard.h"

namespace autosync {
namespace {
constexpr uint32_t STATUS_MAGIC = 0x4b53594eu;  // "KSYN"
// RTC_NOINIT survives the manual sync's silent reboot and deep sleep, not power loss.
RTC_NOINIT_ATTR uint32_t statusMagic;
RTC_NOINIT_ATTR Status lastStatus;
uint32_t skipPullKey = 0;

Status currentStatus() { return statusMagic == STATUS_MAGIC ? lastStatus : Status{}; }
}  // namespace

bool shouldPullOnOpen(const std::string& path) {
  const uint32_t key = bookKey(path);
  const bool returningFromSync = skipPullKey == key;
  skipPullKey = 0;
  return autosync::shouldPullOnOpen(KOREADER_STORE.hasCredentials(), currentStatus(), key, returningFromSync,
                                    trustedtime::trustedNow());
}

void skipNextPull(const std::string& path) { skipPullKey = bookKey(path); }

void record(const std::string& path, const bool ok, const bool pushOnly) {
  const Result result = !ok ? Result::Failed : pushOnly ? Result::Pushed : Result::Ok;
  lastStatus = Status{bookKey(path), result, trustedtime::trustedNow()};
  statusMagic = STATUS_MAGIC;
}

std::string statusText(const std::string& path) {
  return autosync::statusText(KOREADER_STORE.hasCredentials(), currentStatus(), bookKey(path),
                              trustedtime::trustedNow());
}

KOReaderProgress buildUploadProgress(const std::string& documentHash, const SavedProgressPosition& localProgress,
                                     const CrossPointPosition& localPosition, Epub* epub, const std::string& epubPath) {
  KOReaderProgress progress;
  progress.document = documentHash;
  progress.progress = localProgress.xpath;
  progress.percentage = localProgress.percentage;

  // Rich position for server profiles that explicitly support the CrossPoint extension.
  // The HTTP client enforces the same boundary before serializing the extension.
  if (KOREADER_STORE.supportsRichProgress()) {
    KOReaderRichPosition pos;
    const float pct = localProgress.percentage < 0.0f   ? 0.0f
                      : localProgress.percentage > 1.0f ? 1.0f
                                                        : localProgress.percentage;
    pos.pctQ = static_cast<uint32_t>(pct * 1000000.0f + 0.5f);
    pos.spineIndex = static_cast<uint16_t>(localPosition.spineIndex);
    pos.pageNumber = static_cast<uint16_t>(localPosition.pageNumber);
    pos.totalPages = static_cast<uint16_t>(localPosition.totalPages > 0 ? localPosition.totalPages : 1);
    if (localPosition.hasParagraphIndex) {
      pos.paragraphIndex = localPosition.paragraphIndex;
    }
    pos.xpath = localProgress.xpath;
    progress.position = std::move(pos);
  }

  // Optionally include document metadata (KOReader PR #15306)
  if (KOREADER_STORE.getSendMetadata()) {
    KOReaderMetadata meta;
    const auto lastSlash = epubPath.rfind('/');
    meta.filename = (lastSlash != std::string::npos) ? epubPath.substr(lastSlash + 1) : epubPath;
    if (epub) {
      meta.title = epub->getTitle();
      meta.authors = epub->getAuthor();
      if (KOREADER_STORE.supportsExtendedMetadata()) {
        Epub::SyncMetadata syncMetadata;
        if (epub->loadSyncMetadata(syncMetadata)) {
          meta.isbn = std::move(syncMetadata.isbn);
          meta.asin = std::move(syncMetadata.asin);
          meta.series = std::move(syncMetadata.series);
          meta.seriesIndex = syncMetadata.seriesIndex;
        } else {
          LOG_DBG("KOSync", "Could not read extended EPUB metadata; sending core metadata only");
        }
      }
    } else {
      LOG_ERR("KOSync", "Epub unavailable for metadata; sending filename only");
    }
    // Plugin sidecar fields ("<book>.meta.json", written at download time via
    // the catalog sidecar mechanism or /api/plugin-fs) ride along so a custom
    // sync server can route progress by a service book id.
    // A missing sidecar (the common case) leaves extraJson empty.
    Storage.readFileToString("KOSync", epubPath + ".meta.json", 2 * 1024, meta.extraJson);
    progress.metadata = std::move(meta);
  }
  return progress;
}

bool pushProgress(const std::shared_ptr<Epub>& epub, const CrossPointPosition& position) {
  const std::string& path = epub->getPath();
  const std::string hash = KOREADER_STORE.getMatchMethod() == DocumentMatchMethod::FILENAME
                               ? KOReaderDocumentId::calculateFromFilename(path)
                               : KOReaderDocumentId::calculate(path);
  bool ok = false;
  if (!hash.empty()) {
    const KOReaderProgress progress =
        buildUploadProgress(hash, ProgressMapper::toSavedProgress(epub, position), position, epub.get(), path);
    const bool wasConnected = WiFi.status() == WL_CONNECTED;
    if (wasConnected || (hasSavedWifi() && joinSavedWifi(kJoinTimeoutMs))) {
      WiFi.setSleep(false);
      WifiPowerSaveGuard psGuard;
      KOReaderProgress remote{};
      const auto pulled = KOReaderSyncClient::getProgress(hash, remote);
      if (shouldPushOverRemote(pulled == KOReaderSyncClient::OK || pulled == KOReaderSyncClient::NOT_FOUND,
                               pulled == KOReaderSyncClient::OK, remote.deviceId == KOReaderSyncClient::DEVICE_ID,
                               remote.percentage, progress.percentage)) {
        ok = KOReaderSyncClient::updateProgress(progress) == KOReaderSyncClient::OK;
      } else {
        LOG_DBG("KOSync", "Exit push skipped: remote %.4f from %s, local %.4f", remote.percentage,
                remote.device.c_str(), progress.percentage);
      }
    }
    if (!wasConnected) stopWifi();
  }
  LOG_DBG("KOSync", "Auto push %s: %s", ok ? "ok" : "failed", path.c_str());
  record(path, ok, /*pushOnly=*/true);
  return ok;
}

}  // namespace autosync
