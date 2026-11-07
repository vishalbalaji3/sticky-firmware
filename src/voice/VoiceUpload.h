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

// Deletes sent audio past its retention period.
void pruneSentNotes();

}  // namespace voice
