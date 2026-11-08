#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

// Gathers small writes into one caller-owned block, so the SD card sees a few large
// writes instead of one per TLS record. With no block, writes pass straight through.
class WriteBatcher {
 public:
  using Sink = std::function<bool(const uint8_t* data, size_t len)>;

  WriteBatcher(uint8_t* block, size_t capacity, Sink sink)
      : block_(block), capacity_(block ? capacity : 0), sink_(std::move(sink)) {}

  bool write(const uint8_t* data, size_t len) {
    if (capacity_ == 0) return sink_(data, len);
    while (len > 0) {
      const size_t n = len < capacity_ - used_ ? len : capacity_ - used_;
      memcpy(block_ + used_, data, n);
      used_ += n;
      data += n;
      len -= n;
      if (used_ == capacity_ && !flush()) return false;
    }
    return true;
  }

  // Writes out what is held. Call before closing the file.
  bool flush() {
    if (used_ == 0) return true;
    const size_t n = used_;
    used_ = 0;
    return sink_(block_, n);
  }

  // Drops what is held, for a body that restarts from byte 0.
  void discard() { used_ = 0; }

 private:
  uint8_t* block_;
  size_t capacity_;
  size_t used_ = 0;
  Sink sink_;
};
