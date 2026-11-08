#pragma once

#include <string>

namespace voice {

struct UploadResult {
  int uploaded = 0;
  int remaining = 0;
  bool unconfigured = false;  // /.sticky/config.json is missing or incomplete.
  bool unauthorized = false;  // The token was refused, so it needs replacing.
  bool unreachable = false;   // The server could not be reached.
  bool needsLogin = false;    // Unreachable because the network shows a sign-in page first.
};

// Sends every waiting note. Requires Wi-Fi. A note leaves the queue only after the server confirms it.
UploadResult uploadQueuedNotes();

constexpr const char* kTranscriptsPath = "/.sticky/transcripts.json";
constexpr const char* kCardsPath = "/.sticky/cards.json";
constexpr const char* kReviewsPath = "/.sticky/reviews.jsonl";
constexpr const char* kHomePath = "/.sticky/home.json";

constexpr const char* kWallpaperPath = "/sleep.bmp";
constexpr const char* kWallpaperIdPath = "/.sticky/wallpaper.id";

// Sends offline ratings, then downloads new cards and lock screen data. Failures leave the old files.
void syncCardsAndHome();

// Requires Wi-Fi: downloads lock screen data, and the wallpaper when the PWA applied a new one.
void syncHome();

// Saves recent transcripts to the card for the Notes app. False keeps the old file.
bool downloadTranscripts();

// Deletes sent audio past its retention period.
void pruneSentNotes();

// Requires Wi-Fi: sets the clock, uploads waiting notes, prunes sent audio, downloads transcripts and lock screen data.
// transcriptsSaved reports whether the transcript file was refreshed.
UploadResult syncNotes(bool& transcriptsSaved);

}  // namespace voice
