#pragma once

#include <cstdint>
#include <string>

// Hold-to-talk recording. Every note is written to the card first, so nothing depends on the network.
namespace voice {

constexpr const char* kNotesDir = "/.sticky/notes";
constexpr const char* kSentDir = "/.sticky/notes/sent";

// What a note attaches to. An empty bookId means the inbox.
struct Place {
  std::string bookId;
  std::string bookTitle;
  std::string bookAuthor;
  std::string chapter;
  int spineIndex = -1;
  int page = -1;
  uint32_t startOffset = UINT32_MAX;
  uint32_t endOffset = UINT32_MAX;
  std::string quote;
};

// Starts capture. Returns false if the mic, memory or card is unavailable; nothing is left behind.
bool startRecording(const Place& place);
bool isRecording();
// Moves captured audio to the card. Call often from the main loop while recording.
void pumpRecording();
// True once a note reaches the size the cloud API accepts.
bool recordingFull();
// Finishes the file and makes the note ready to upload.
// Returns false when the note was too short to keep (e.g. the hold that woke the device) and was deleted.
bool stopRecording();

// Repairs notes interrupted by power loss, so their audio can still upload.
void recoverInterruptedNotes();

}  // namespace voice
