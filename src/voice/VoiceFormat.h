#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Pure helpers for voice notes, with no hardware dependencies so they run in host tests.
namespace voice {

constexpr uint32_t kSampleRate = 16000;
constexpr size_t kWavHeaderBytes = 44;
// The cloud API refuses more than 25 MB. Stopping at 12 minutes (23 MB) leaves a margin.
constexpr uint32_t kMaxDataBytes = kSampleRate * 2 * 60 * 12;
constexpr uint32_t kKeepSentSeconds = 30 * 24 * 60 * 60;
constexpr uint32_t kFirstValidUnixTime = 1700000000;  // Anything earlier means the clock was never set.

inline void put32(uint8_t* p, uint32_t v) {
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

inline void put16(uint8_t* p, uint16_t v) {
  p[0] = v;
  p[1] = v >> 8;
}

// A canonical 16-bit mono PCM WAV header.
inline void writeWavHeader(uint8_t* out, uint32_t dataBytes, uint32_t sampleRate = kSampleRate) {
  memcpy(out, "RIFF", 4);
  put32(out + 4, 36 + dataBytes);
  memcpy(out + 8, "WAVEfmt ", 8);
  put32(out + 16, 16);
  put16(out + 20, 1);
  put16(out + 22, 1);
  put32(out + 24, sampleRate);
  put32(out + 28, sampleRate * 2);
  put16(out + 32, 2);
  put16(out + 34, 16);
  memcpy(out + 36, "data", 4);
  put32(out + 40, dataBytes);
}

// Stable across firmware builds and file moves, unlike std::hash of a path.
inline std::string bookIdFor(const std::string& title, const std::string& author) {
  uint64_t hash = 14695981039346656037ULL;
  const auto mix = [&hash](const std::string& text) {
    for (const unsigned char c : text) {
      hash ^= c;
      hash *= 1099511628211ULL;
    }
    hash ^= 0xFF;
    hash *= 1099511628211ULL;
  };
  mix(title);
  mix(author);
  char out[20];
  snprintf(out, sizeof(out), "b%016llx", static_cast<unsigned long long>(hash));
  return out;
}

// Cuts text to at most maxBytes without splitting a UTF-8 character, because the API refuses longer fields.
inline std::string truncateUtf8(const std::string& text, size_t maxBytes) {
  if (text.size() <= maxBytes) return text;
  size_t end = maxBytes;
  while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) --end;
  return text.substr(0, end);
}

// The API needs a title, but some EPUBs have none. The file name is the best stable stand-in.
inline std::string titleOrFileName(const std::string& title, const std::string& path) {
  if (!title.empty()) return title;
  const size_t slash = path.rfind('/');
  const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  return name.empty() ? "Untitled" : name;
}

// Sent audio is kept for a while as "<unix seconds>-<clientId>.wav". The time is in the name because
// the card's file timestamps are unreliable on a device without a battery-backed clock.
inline std::string sentFileName(uint32_t sentAt, const std::string& clientId) {
  return std::to_string(sentAt) + "-" + clientId + ".wav";
}

inline bool sentFileExpired(const char* name, uint32_t now) {
  uint32_t sentAt = 0;
  const char* p = name;
  for (; *p >= '0' && *p <= '9'; ++p) sentAt = sentAt * 10 + (*p - '0');
  if (p == name || *p != '-' || now < kFirstValidUnixTime) return false;
  return now > sentAt && now - sentAt > kKeepSentSeconds;
}

// Open-Meteo's WMO weather codes, in as few words as the lock screen has room for.
inline const char* weatherLabel(const int code) {
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67 || (code >= 80 && code <= 82)) return "Rain";
  if (code <= 77 || code == 85 || code == 86) return "Snow";
  if (code >= 95) return "Storms";
  return "";
}

// Parses "HTTP/1.1 200 OK" and returns the status, or -1.
inline int parseHttpStatus(const std::string& head) {
  if (head.rfind("HTTP/", 0) != 0) return -1;
  const size_t space = head.find(' ');
  if (space == std::string::npos || space + 4 > head.size()) return -1;
  int status = 0;
  for (size_t i = space + 1; i < space + 4; ++i) {
    if (head[i] < '0' || head[i] > '9') return -1;
    status = status * 10 + (head[i] - '0');
  }
  return status;
}

// The reviews upload body from the offline ratings file, one JSON object per line. A line cut short by a power
// cut during its write is left out, or it would make every later upload invalid JSON.
inline std::string reviewsBody(const std::string& lines) {
  std::string body = "{\"reviews\":[";
  bool first = true;
  for (size_t start = 0; start < lines.size();) {
    size_t end = lines.find('\n', start);
    if (end == std::string::npos) end = lines.size();
    if (end > start && lines[start] == '{' && lines[end - 1] == '}') {
      if (!first) body += ',';
      body.append(lines, start, end - start);
      first = false;
    }
    start = end + 1;
  }
  return body + "]}";
}

// The Content-Length of a response head, or -1 when it has none (a streamed body that ends at close).
inline long parseContentLength(const std::string& head) {
  for (size_t line = head.find("\r\n"); line != std::string::npos; line = head.find("\r\n", line + 2)) {
    static constexpr char kName[] = "content-length:";
    size_t i = 0;
    while (i + 1 < sizeof(kName) && line + 2 + i < head.size() && (head[line + 2 + i] | 0x20) == kName[i]) {
      ++i;
    }
    if (i + 1 < sizeof(kName)) continue;
    size_t at = line + 2 + i;
    while (at < head.size() && head[at] == ' ') ++at;
    long length = -1;
    for (; at < head.size() && head[at] >= '0' && head[at] <= '9'; ++at) {
      length = (length < 0 ? 0 : length * 10) + (head[at] - '0');
    }
    return length;
  }
  return -1;
}

// Single-producer, single-consumer sample buffer. The mic task only writes and the main loop only
// reads, so a slow e-paper refresh delays the card without stopping capture. Overflow drops the
// newest samples and counts them, which is what the recording test needs to measure.
class SampleRing {
 public:
  void attach(int16_t* buffer, size_t capacity) {
    buffer_ = buffer;
    capacity_ = capacity;
    head_.store(0);
    tail_.store(0);
    dropped_.store(0);
  }

  void write(const int16_t* samples, size_t count) {
    const size_t head = head_.load(std::memory_order_relaxed);
    const size_t tail = tail_.load(std::memory_order_acquire);
    const size_t space = capacity_ - (head - tail);
    const size_t accepted = count < space ? count : space;
    const size_t at = head % capacity_;
    const size_t first = accepted < capacity_ - at ? accepted : capacity_ - at;
    memcpy(buffer_ + at, samples, first * sizeof(int16_t));
    memcpy(buffer_, samples + first, (accepted - first) * sizeof(int16_t));
    head_.store(head + accepted, std::memory_order_release);
    if (accepted < count) dropped_.fetch_add(count - accepted, std::memory_order_relaxed);
  }

  // Up to maxSamples contiguous samples, valid until consume().
  size_t peek(const int16_t** out, size_t maxSamples) const {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t available = head_.load(std::memory_order_acquire) - tail;
    const size_t at = tail % capacity_;
    size_t count = available < capacity_ - at ? available : capacity_ - at;
    if (count > maxSamples) count = maxSamples;
    *out = buffer_ + at;
    return count;
  }

  void consume(size_t count) { tail_.store(tail_.load(std::memory_order_relaxed) + count, std::memory_order_release); }
  size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

 private:
  int16_t* buffer_ = nullptr;
  size_t capacity_ = 0;
  std::atomic<size_t> head_{0};
  std::atomic<size_t> tail_{0};
  std::atomic<size_t> dropped_{0};
};

// Notes app labels. The UI turns each into its translated string.
enum class NoteLabel {
  None,
  Syncing,
  Transcribing,
  UploadingAudio,
  TranscriptionFailed,
  NotSetUp,
  TokenRefused,
  NeedsLogin,
  NotSynced
};

// Notes app: the subtitle for a note without text, from the server's status.
inline NoteLabel pendingNoteLabel(const char* status) {
  if (strcmp(status, "transcribing") == 0) return NoteLabel::Transcribing;
  if (strcmp(status, "awaiting_audio") == 0) return NoteLabel::UploadingAudio;
  if (strcmp(status, "failed") == 0) return NoteLabel::TranscriptionFailed;
  return NoteLabel::None;
}

inline bool transcriptPending(const char* status) {
  return strcmp(status, "transcribing") == 0 || strcmp(status, "awaiting_audio") == 0;
}

// Notes app: transcripts are fetched again while some are pending, a few times at most.
constexpr int kTranscriptRechecks = 4;
constexpr unsigned long kTranscriptRecheckMs = 5000;

inline bool shouldRecheckTranscripts(const int pending, const int rechecksDone) {
  return pending > 0 && rechecksDone < kTranscriptRechecks;
}

// Notes app: the header status after a sync attempt. None means up to date.
// needsLogin: the network answered with a sign-in page instead of the internet.
inline NoteLabel notesSyncStatus(const bool online, const bool unconfigured, const bool unauthorized,
                                 const bool needsLogin, const bool transcriptsSaved, const int pending) {
  if (unconfigured) return NoteLabel::NotSetUp;
  if (unauthorized) return NoteLabel::TokenRefused;
  if (needsLogin) return NoteLabel::NeedsLogin;
  if (!online || !transcriptsSaved) return NoteLabel::NotSynced;
  return pending > 0 ? NoteLabel::Transcribing : NoteLabel::None;
}

}  // namespace voice
