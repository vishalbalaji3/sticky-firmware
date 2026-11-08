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
      : block(block), capacity(block ? capacity : 0), sink(std::move(sink)) {}

  bool write(const uint8_t* data, size_t len) {
    if (capacity == 0) return sink(data, len);
    while (len > 0) {
      const size_t n = len < capacity - used ? len : capacity - used;
      memcpy(block + used, data, n);
      used += n;
      data += n;
      len -= n;
      if (used == capacity && !flush()) return false;
    }
    return true;
  }

  // Writes out what is held. Call before closing the file.
  bool flush() {
    if (used == 0) return true;
    const size_t n = used;
    used = 0;
    return sink(block, n);
  }

  // Drops what is held, for a body that restarts from byte 0.
  void discard() { used = 0; }

 private:
  uint8_t* block;
  size_t capacity;
  size_t used = 0;
  Sink sink;
};
