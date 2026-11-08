#pragma once

#include <cstdint>
#include <string>

// Pure decisions behind automatic KOReader progress sync (host-tested).
namespace autosync {

// Pushed: only the exit push succeeded, so the server may still get newer progress from another device.
enum class Result : uint8_t { None = 0, Ok = 1, Failed = 2, Pushed = 3 };

// Last sync outcome for one book. No member initializers: it lives in RTC
// memory, where a constructor would wipe it at boot.
struct Status {
  uint32_t bookKey;
  Result result;
  int64_t at;  // epoch seconds of the outcome, 0 when the clock was unknown
};

// FNV-1a of the path, so the status can name a book without storing the path.
inline uint32_t bookKey(const std::string& path) {
  uint32_t hash = 2166136261u;
  for (const unsigned char c : path) hash = (hash ^ c) * 16777619u;
  return hash;
}

// A book pulled or manually synced this recently (the manual sync reboots straight back into it)
// needs no second pull. An exit push does not count: the other device may have read on since.
constexpr int64_t kRecentSyncSeconds = 120;

// returningFromSync: the reader reopened by the sync activity itself, which must not pull again.
inline bool shouldPullOnOpen(const bool hasCredentials, const Status& status, const uint32_t key,
                             const bool returningFromSync, const int64_t now) {
  if (!hasCredentials || returningFromSync) return false;
  const bool recentlySynced = status.bookKey == key && status.result == Result::Ok && status.at > 0 &&
                              now >= status.at && now - status.at < kRecentSyncSeconds;
  return !recentlySynced;
}

// Only push a position the reader actually moved to: pushing an unread open
// position would overwrite newer progress from the other device.
inline bool shouldPushOnExit(const bool hasCredentials, const int openSpine, const int openPage, const int spine,
                             const int page) {
  return hasCredentials && openSpine >= 0 && (spine != openSpine || page != openPage);
}

// The server keeps the last write, so the exit push reads the remote position first. Skip only when
// another device's position is further than ours: pushing would lose that reading, and skipping loses
// nothing, because the local position stays on the card and the next open's pull compares the two.
// An unreadable remote also skips, since it may hold such a position.
inline bool shouldPushOverRemote(const bool remoteRead, const bool remoteExists, const bool remoteIsOwn,
                                 const float remotePercentage, const float localPercentage) {
  if (!remoteRead) return false;
  return !remoteExists || remoteIsOwn || remotePercentage <= localPercentage;
}

inline std::string statusText(const bool hasCredentials, const Status& status, const uint32_t key, const int64_t now) {
  if (!hasCredentials) return "Sync off (set up in Settings)";
  if (status.bookKey != key || (status.result != Result::Ok && status.result != Result::Pushed)) return "Not synced";
  if (status.at <= 0 || now <= 0) return "Synced";
  const int64_t age = now > status.at ? now - status.at : 0;
  if (age < 60) return "Synced just now";
  if (age < 3600) return "Synced " + std::to_string(age / 60) + " min ago";
  if (age < 86400) return "Synced " + std::to_string(age / 3600) + " h ago";
  return "Synced " + std::to_string(age / 86400) + " d ago";
}

}  // namespace autosync
