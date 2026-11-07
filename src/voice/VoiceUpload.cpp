#include "VoiceUpload.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SecureClient.h>

#include <ctime>
#include <functional>
#include <memory>
#include <new>
#include <vector>

#include "SecureRequest.h"
#include "VoiceFormat.h"
#include "VoiceNotes.h"

namespace voice {
namespace {

constexpr size_t kFileChunk = 4096;
constexpr size_t kMaxTextFile = 8 * 1024;
constexpr int kLocalFileError = -2;
constexpr const char* kTranscriptsPath = "/.sticky/transcripts.json";

// Returns the HTTP status of the first step that did not succeed, or 200.
int uploadNote(const Config& config, const std::string& id) {
  const std::string metaPath = std::string(kNotesDir) + "/" + id + ".json";
  const std::string wavPath = std::string(kNotesDir) + "/" + id + ".wav";

  std::string meta;
  if (!readTextFile(metaPath, meta, kMaxTextFile)) return kLocalFileError;

  std::string response;
  Request metaRequest;
  metaRequest.host = config.host;
  metaRequest.method = "POST";
  metaRequest.target = "/api/v1/notes";
  metaRequest.token = config.token;
  metaRequest.contentType = "application/json";
  metaRequest.bodyLength = meta.size();
  metaRequest.sendBody = [&meta](freeink::SecureClient& c) {
    return writeAll(c, reinterpret_cast<const uint8_t*>(meta.data()), meta.size());
  };
  metaRequest.onBody = [&response](const uint8_t* data, const size_t size) {
    if (response.size() < 1024) response.append(reinterpret_cast<const char*>(data), size);
  };
  const int metaStatus = secureRequest(metaRequest);
  if (metaStatus != 200) return metaStatus;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return -1;
  // false means an earlier attempt already delivered the audio and only its response was lost.
  if (!(doc["needsAudio"] | true)) return 200;

  HalFile audio;
  if (!Storage.openFileForRead("VOICE", wavPath, audio)) return kLocalFileError;
  const std::unique_ptr<uint8_t[]> chunk(new (std::nothrow) uint8_t[kFileChunk]);
  if (!chunk) {
    audio.close();
    return -1;
  }
  Request audioRequest;
  audioRequest.host = config.host;
  audioRequest.method = "POST";
  audioRequest.target = "/api/v1/notes/audio?clientId=" + id;
  audioRequest.token = config.token;
  audioRequest.contentType = "audio/wav";
  audioRequest.bodyLength = audio.fileSize();
  audioRequest.sendBody = [&audio, &chunk](freeink::SecureClient& c) {
    int got;
    while ((got = audio.read(chunk.get(), kFileChunk)) > 0) {
      if (!writeAll(c, chunk.get(), got)) return false;
    }
    return got == 0;
  };
  const int audioStatus = secureRequest(audioRequest);
  audio.close();
  return audioStatus;
}

}  // namespace

UploadResult uploadQueuedNotes() {
  UploadResult result;
  recoverInterruptedNotes();

  Config config;
  std::vector<std::string> ids;
  for (const String& name : Storage.listFiles(kNotesDir)) {
    if (name.endsWith(".wav")) ids.emplace_back(name.substring(0, name.length() - 4).c_str());
  }
  result.remaining = ids.size();
  if (!loadConfig(config)) {
    result.unconfigured = true;
    return result;
  }

  Storage.ensureDirectoryExists(kSentDir);
  for (const std::string& id : ids) {
    const int status = uploadNote(config, id);
    if (status == 401) {
      result.unauthorized = true;
      return result;
    }
    if (status == kLocalFileError) {
      LOG_ERR("VOICE", "Note %s is unreadable on the card, skipping it", id.c_str());
      continue;
    }
    if (status < 0) {
      result.unreachable = true;
      return result;
    }
    if (status != 200) {
      // Refused for this note alone, such as bad metadata. It stays on the card, and others still go.
      LOG_ERR("VOICE", "Server refused note %s: %d", id.c_str(), status);
      continue;
    }
    const std::string wavPath = std::string(kNotesDir) + "/" + id + ".wav";
    const time_t now = time(nullptr);
    // Sent while the clock is unset, the "0-" name waits for pruneSentNotes() to date it.
    const std::string sentName =
        now >= kFirstValidUnixTime ? sentFileName(static_cast<uint32_t>(now), id) : "0-" + id + ".wav";
    // Metadata goes last. If the move fails the note simply retries, and the server replies that it has the audio.
    if (!Storage.rename(wavPath.c_str(), (std::string(kSentDir) + "/" + sentName).c_str())) continue;
    Storage.remove((std::string(kNotesDir) + "/" + id + ".json").c_str());
    result.uploaded++;
    result.remaining--;
  }
  return result;
}

void pruneSentNotes() {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  if (now < kFirstValidUnixTime) return;
  for (const String& name : Storage.listFiles(kSentDir)) {
    if (name.startsWith("0-")) {
      // Sent while the clock was unset. Its retention starts now that the time is known.
      Storage.rename(
          (String(kSentDir) + "/" + name).c_str(),
          (std::string(kSentDir) + "/" + sentFileName(now, name.substring(2, name.length() - 4).c_str())).c_str());
    } else if (sentFileExpired(name.c_str(), now)) {
      Storage.remove((String(kSentDir) + "/" + name).c_str());
    }
  }
}

namespace {

bool downloadTo(const Config& config, const char* target, const char* path) {
  const std::string temp = std::string(path) + ".tmp";
  HalFile file;
  if (!Storage.openFileForWrite("VOICE", temp, file)) return false;
  Request request;
  request.host = config.host;
  request.target = target;
  request.token = config.token;
  request.onBody = [&file](const uint8_t* data, const size_t size) { file.write(data, size); };
  const int status = secureRequest(request);
  file.close();
  // Only a complete reply replaces the file, so a dropped connection keeps the old one.
  if (status != 200) {
    Storage.remove(temp.c_str());
    return false;
  }
  Storage.remove(path);
  return Storage.rename(temp.c_str(), path);
}

}  // namespace

void downloadTranscripts() {
  Config config;
  if (loadConfig(config)) downloadTo(config, "/api/v1/notes", kTranscriptsPath);
}

void syncCardsAndHome() {
  Config config;
  if (!loadConfig(config)) return;

  std::string pending;
  bool reviewsSent = true;
  if (Storage.exists(kReviewsPath) && readTextFile(kReviewsPath, pending, 256 * 1024) && !pending.empty()) {
    std::string body = "{\"reviews\":[";
    for (size_t start = 0; start < pending.size();) {
      size_t end = pending.find('\n', start);
      if (end == std::string::npos) end = pending.size();
      if (start != 0) body += ',';
      body.append(pending, start, end - start);
      start = end + 1;
    }
    body += "]}";
    Request request;
    request.host = config.host;
    request.method = "POST";
    request.target = "/api/v1/reviews";
    request.token = config.token;
    request.contentType = "application/json";
    request.bodyLength = body.size();
    request.sendBody = [&body](freeink::SecureClient& c) {
      return writeAll(c, reinterpret_cast<const uint8_t*>(body.data()), body.size());
    };
    reviewsSent = secureRequest(request) == 200;
    // Ratings are kept until the server confirms them. A retry is safe, because it drops duplicates.
    if (reviewsSent) Storage.remove(kReviewsPath);
  }
  // New cards wait until ratings land, or the same cards would be asked twice.
  if (reviewsSent) downloadTo(config, "/api/v1/cards", kCardsPath);
  downloadTo(config, "/api/v1/home", kHomePath);
}

}  // namespace voice
