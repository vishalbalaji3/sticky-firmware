#pragma once

#include <Epub.h>

#include <memory>
#include <string>

#include "AutoSyncPolicy.h"
#include "KOReaderSyncClient.h"
#include "ProgressMapper.h"

// Automatic KOReader progress sync: pull when a book opens, push when it is
// left, and the per-book status the reader menu shows.
namespace autosync {

// Short so a missing network costs the reader only a moment.
constexpr unsigned long kJoinTimeoutMs = 5000;

// True when the book should pull now.
bool shouldPullOnOpen(const std::string& path);
// Called when launching the automatic sync: the reader it reopens skips its pull once.
void skipNextPull(const std::string& path);
// pushOnly: the exit push, which must not suppress the next open's pull.
void record(const std::string& path, bool ok, bool pushOnly = false);
std::string statusText(const std::string& path);

// Upload record for the local position, shared with the manual sync.
KOReaderProgress buildUploadProgress(const std::string& documentHash, const SavedProgressPosition& localProgress,
                                     const CrossPointPosition& localPosition, Epub* epub, const std::string& epubPath);

// Blocking: joins the saved network if needed, pushes, and turns WiFi off again if it joined.
bool pushProgress(const std::shared_ptr<Epub>& epub, const CrossPointPosition& position);

}  // namespace autosync
