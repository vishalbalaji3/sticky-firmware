#include "VoiceNotes.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Microphone.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <ctime>

#include "VoiceFormat.h"

namespace voice {
namespace {

// 32 seconds at 16 kHz, far longer than any e-paper refresh or card stall.
constexpr size_t kRingSamples = 512 * 1024;
constexpr size_t kMicBlockSamples = 512;
constexpr size_t kPumpMaxSamples = 8192;

Microphone mic;
SampleRing ring;
int16_t* ringBuffer = nullptr;
HalFile audioFile;
std::string audioPath;
std::string clientId;
uint32_t dataBytes = 0;
bool recording = false;
volatile bool micRunning = false;
SemaphoreHandle_t micStopped = nullptr;

void micTask(void*) {
  static int16_t block[kMicBlockSamples];
  while (micRunning) {
    const int got = mic.read(block, kMicBlockSamples, 100);
    if (got > 0) ring.write(block, got);
  }
  xSemaphoreGive(micStopped);
  vTaskDelete(nullptr);
}

std::string newClientId() {
  char out[24];
  snprintf(out, sizeof(out), "%08lx%08lx", static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  return out;
}

std::string partPath(const std::string& id) { return std::string(kNotesDir) + "/" + id + ".part"; }

bool writeMeta(const std::string& id, const Place& place) {
  JsonDocument doc;
  doc["clientId"] = id;
  const time_t now = time(nullptr);
  // The API uses the upload time when the clock has never been set.
  if (now >= kFirstValidUnixTime) doc["capturedAt"] = static_cast<uint64_t>(now) * 1000ULL;
  if (place.bookId.empty()) {
    doc["kind"] = "inbox";
  } else {
    doc["kind"] = place.quote.empty() ? "page" : "sentence";
    doc["bookId"] = place.bookId;
    // Limits match parseNoteMeta in the cloud API. A longer field would strand the note on the card.
    doc["bookTitle"] = truncateUtf8(place.bookTitle, 256);
    if (!place.bookAuthor.empty()) doc["bookAuthor"] = truncateUtf8(place.bookAuthor, 256);
    if (!place.chapter.empty()) doc["chapter"] = truncateUtf8(place.chapter, 256);
    if (place.spineIndex >= 0) doc["spineIndex"] = place.spineIndex;
    if (place.page >= 0) doc["page"] = place.page;
    if (place.startOffset != UINT32_MAX) doc["startOffset"] = place.startOffset;
    if (place.endOffset != UINT32_MAX) doc["endOffset"] = place.endOffset;
    if (!place.quote.empty()) doc["quote"] = place.quote;
  }
  HalFile file;
  if (!Storage.openFileForWrite("VOICE", std::string(kNotesDir) + "/" + id + ".json", file)) return false;
  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool writeHeader(HalFile& file, const uint32_t bytes) {
  uint8_t header[kWavHeaderBytes];
  writeWavHeader(header, bytes);
  return file.seek(0) && file.write(header, sizeof(header)) == sizeof(header);
}

void releaseCapture() {
  if (micRunning) {
    micRunning = false;
    xSemaphoreTake(micStopped, portMAX_DELAY);
  }
  mic.end();
}

}  // namespace

bool startRecording(const Place& place) {
  if (recording) return true;
  if (!Storage.ensureDirectoryExists(kSentDir)) return false;  // Creates the notes folder too.
  if (!ringBuffer) {
    ringBuffer = static_cast<int16_t*>(heap_caps_malloc(kRingSamples * sizeof(int16_t), MALLOC_CAP_SPIRAM));
    if (!ringBuffer) {
      LOG_ERR("VOICE", "No memory for the audio buffer");
      return false;
    }
  }
  if (!micStopped) micStopped = xSemaphoreCreateBinary();

  clientId = newClientId();
  audioPath = partPath(clientId);
  // The quote and place are saved before any audio, so a power cut still leaves a complete note.
  if (!writeMeta(clientId, place) || !Storage.openFileForWrite("VOICE", audioPath, audioFile) ||
      !writeHeader(audioFile, 0)) {
    LOG_ERR("VOICE", "Could not create the note on the card");
    audioFile.close();
    Storage.remove(audioPath.c_str());
    Storage.remove((std::string(kNotesDir) + "/" + clientId + ".json").c_str());
    return false;
  }

  dataBytes = 0;
  ring.attach(ringBuffer, kRingSamples);
  if (!mic.begin(kSampleRate)) {
    LOG_ERR("VOICE", "Microphone did not start");
    audioFile.close();
    Storage.remove(audioPath.c_str());
    Storage.remove((std::string(kNotesDir) + "/" + clientId + ".json").c_str());
    return false;
  }
  micRunning = true;
  if (xTaskCreatePinnedToCore(micTask, "voiceMic", 4096, nullptr, 5, nullptr, 0) != pdPASS) {
    micRunning = false;
    mic.end();
    audioFile.close();
    Storage.remove(audioPath.c_str());
    Storage.remove((std::string(kNotesDir) + "/" + clientId + ".json").c_str());
    return false;
  }
  recording = true;
  LOG_INF("VOICE", "Recording %s", clientId.c_str());
  return true;
}

bool isRecording() { return recording; }

bool updatePlace(const Place& place) { return recording && writeMeta(clientId, place); }

void pumpRecording() {
  if (!recording) return;
  const int16_t* samples;
  while (const size_t count = ring.peek(&samples, kPumpMaxSamples)) {
    const size_t bytes = count * sizeof(int16_t);
    const size_t written = audioFile.write(samples, bytes);
    // Count only whole samples, so a short write cannot leave the rest of the audio misaligned.
    ring.consume(written / sizeof(int16_t));
    dataBytes += written & ~static_cast<size_t>(1);
    if (written != bytes) {
      LOG_ERR("VOICE", "Card write failed after %lu bytes", static_cast<unsigned long>(dataBytes));
      return;
    }
  }
}

bool recordingFull() { return recording && dataBytes >= kMaxDataBytes; }

bool stopRecording() {
  if (!recording) return false;
  releaseCapture();
  pumpRecording();
  recording = false;
  if (dataBytes < kSampleRate * sizeof(int16_t)) {  // Under a second: an accidental hold, which transcribes as ".".
    audioFile.close();
    Storage.remove(audioPath.c_str());
    Storage.remove((std::string(kNotesDir) + "/" + clientId + ".json").c_str());
    LOG_INF("VOICE", "Discarded %s: under a second", clientId.c_str());
    return false;
  }
  const bool headerOk = writeHeader(audioFile, dataBytes);
  audioFile.close();
  // Kept as .part on failure, so recoverInterruptedNotes() can try again.
  if (headerOk) {
    Storage.rename(audioPath.c_str(), (std::string(kNotesDir) + "/" + clientId + ".wav").c_str());
  }
  LOG_INF("VOICE", "Saved %s: %lu bytes, %u samples dropped", clientId.c_str(), static_cast<unsigned long>(dataBytes),
          static_cast<unsigned>(ring.dropped()));
  return true;
}

void recoverInterruptedNotes() {
  if (recording) return;
  for (const String& name : Storage.listFiles(kNotesDir)) {
    if (!name.endsWith(".part")) continue;
    const std::string id = name.substring(0, name.length() - 5).c_str();
    const std::string path = partPath(id);
    HalFile file = Storage.open(path.c_str(), O_RDWR);
    if (!file.isOpen()) continue;
    const size_t size = file.fileSize();
    const bool ok = size > kWavHeaderBytes && writeHeader(file, size - kWavHeaderBytes);
    file.close();
    if (ok) {
      Storage.rename(path.c_str(), (std::string(kNotesDir) + "/" + id + ".wav").c_str());
    } else if (size <= kWavHeaderBytes) {
      Storage.remove(path.c_str());
      Storage.remove((std::string(kNotesDir) + "/" + id + ".json").c_str());
    }
  }
}

}  // namespace voice
