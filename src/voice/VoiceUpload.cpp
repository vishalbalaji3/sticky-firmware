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

#include "TrustRoots.h"
#include "VoiceFormat.h"
#include "VoiceNotes.h"

namespace voice {
namespace {

constexpr const char* kConfigPath = "/.sticky/config.json";
constexpr uint32_t kResponseTimeoutMs = 30000;
constexpr size_t kFileChunk = 4096;
constexpr size_t kMaxResponse = 2048;
constexpr size_t kMaxTextFile = 8 * 1024;  // Metadata holds at most a 4 KB quote plus short fields.
constexpr int kLocalFileError = -2;

struct Config {
  std::string host;
  std::string token;
};

bool readAll(const std::string& path, std::string& out) {
  HalFile file;
  if (!Storage.openFileForRead("VOICE", path, file)) return false;
  if (file.fileSize() > kMaxTextFile) {
    file.close();
    return false;
  }
  out.resize(file.fileSize());
  const bool ok = out.empty() || file.read(out.data(), out.size()) == static_cast<int>(out.size());
  file.close();
  return ok;
}

bool loadConfig(Config& config) {
  std::string text;
  if (!Storage.exists(kConfigPath) || !readAll(kConfigPath, text)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, text)) return false;
  // shortcut: the token sits unencrypted on the card, revoke it in the PWA if the card is lost.
  config.host = doc["host"] | "";
  config.token = doc["token"] | "";
  return !config.host.empty() && !config.token.empty();
}

// The SDK's HTTP client needs the whole body in memory. Audio can be far larger than PSRAM, so this
// streams over the verified TLS client directly.
int request(const Config& config, const std::string& target, const char* contentType, size_t length,
            const std::function<bool(freeink::SecureClient&)>& sendBody, std::string* responseBody) {
  freeink::SecureClient client;
  client.setCACert(kTrustRoots);
  if (!client.connect(config.host.c_str(), 443)) return -1;

  std::string head = "POST " + target + " HTTP/1.1\r\nHost: " + config.host + "\r\nAuthorization: Bearer " +
                     config.token + "\r\nContent-Type: " + contentType +
                     "\r\nContent-Length: " + std::to_string(length) + "\r\nConnection: close\r\n\r\n";
  if (client.write(reinterpret_cast<const uint8_t*>(head.data()), head.size()) != head.size() || !sendBody(client)) {
    client.stop();
    return -1;
  }

  std::string response;
  uint8_t buffer[256];
  const uint32_t started = millis();
  while (millis() - started < kResponseTimeoutMs) {
    if (client.available() > 0) {
      const int got = client.read(buffer, sizeof(buffer));
      if (got > 0 && response.size() < kMaxResponse) response.append(reinterpret_cast<char*>(buffer), got);
    } else if (!client.connected()) {
      break;
    } else {
      delay(5);
    }
  }
  client.stop();

  const int status = parseHttpStatus(response);
  const size_t body = response.find("\r\n\r\n");
  if (responseBody && body != std::string::npos) *responseBody = response.substr(body + 4);
  return status;
}

bool writeAll(freeink::SecureClient& client, const uint8_t* data, size_t size) {
  while (size > 0) {
    const size_t written = client.write(data, size);
    if (written == 0) return false;
    data += written;
    size -= written;
  }
  return true;
}

// Returns the HTTP status of the first step that did not succeed, or 200.
int uploadNote(const Config& config, const std::string& id) {
  const std::string metaPath = std::string(kNotesDir) + "/" + id + ".json";
  const std::string wavPath = std::string(kNotesDir) + "/" + id + ".wav";

  std::string meta;
  if (!readAll(metaPath, meta)) return kLocalFileError;
  std::string response;
  const int metaStatus = request(
      config, "/api/v1/notes", "application/json", meta.size(),
      [&meta](freeink::SecureClient& c) {
        return writeAll(c, reinterpret_cast<const uint8_t*>(meta.data()), meta.size());
      },
      &response);
  if (metaStatus != 200) return metaStatus;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return -1;
  // false means an earlier attempt already delivered the audio and only its response was lost.
  if (!(doc["needsAudio"] | true)) return 200;

  HalFile audio;
  if (!Storage.openFileForRead("VOICE", wavPath, audio)) return kLocalFileError;
  const size_t size = audio.fileSize();
  const std::unique_ptr<uint8_t[]> chunk(new (std::nothrow) uint8_t[kFileChunk]);
  if (!chunk) {
    audio.close();
    return -1;
  }
  const int audioStatus = request(
      config, "/api/v1/notes/audio?clientId=" + id, "audio/wav", size,
      [&audio, &chunk](freeink::SecureClient& c) {
        int got;
        while ((got = audio.read(chunk.get(), kFileChunk)) > 0) {
          if (!writeAll(c, chunk.get(), got)) return false;
        }
        return got == 0;
      },
      nullptr);
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

}  // namespace voice
