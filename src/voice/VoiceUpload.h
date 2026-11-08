#pragma once

#include <string>

namespace voice {

struct UploadResult {
  int uploaded = 0;
  int remaining = 0;
  bool unconfigured = false;  // /.sticky/config.json is missing or incomplete.
  bool unauthorized = false;  // The token was refused, so it needs replacing.
  bool unreachable = false;   // Often a network with a sign-in page.
};

// Sends every waiting note. Requires Wi-Fi. A note leaves the queue only after the server confirms it.
UploadResult uploadQueuedNotes();

constexpr const char* kCardsPath = "/.sticky/cards.json";
constexpr const char* kReviewsPath = "/.sticky/reviews.jsonl";
constexpr const char* kHomePath = "/.sticky/home.json";

// Sends offline ratings, then downloads new cards and lock screen data. Failures leave the old files.
void syncCardsAndHome();

// Saves recent transcripts to the card for the Notes app. False keeps the old file.
bool downloadTranscripts();

// Deletes sent audio past its retention period.
void pruneSentNotes();

// Requires Wi-Fi: sets the clock, uploads waiting notes, prunes sent audio and downloads transcripts.
// transcriptsSaved reports whether the transcript file was refreshed.
UploadResult syncNotes(bool& transcriptsSaved);

}  // namespace voice
