#pragma once

#include <cstdint>
#include <string>

namespace remote {

// A masked client text frame (RFC 6455). Commands are small, so lengths above 64 KB are not needed.
inline std::string encodeTextFrame(const std::string& payload, const uint32_t mask) {
  std::string frame;
  frame.reserve(payload.size() + 8);
  frame.push_back(static_cast<char>(0x81));
  if (payload.size() < 126) {
    frame.push_back(static_cast<char>(0x80 | payload.size()));
  } else {
    frame.push_back(static_cast<char>(0x80 | 126));
    frame.push_back(static_cast<char>(payload.size() >> 8));
    frame.push_back(static_cast<char>(payload.size() & 0xFF));
  }
  const char key[4] = {static_cast<char>(mask >> 24), static_cast<char>(mask >> 16), static_cast<char>(mask >> 8),
                       static_cast<char>(mask)};
  frame.append(key, 4);
  for (size_t i = 0; i < payload.size(); ++i) frame.push_back(static_cast<char>(payload[i] ^ key[i % 4]));
  return frame;
}

// The remote's touch grid: which cell a tap landed on, or -1 outside it.
inline int gridCell(const int x, const int y, const int top, const int width, const int height, const int columns,
                    const int rows) {
  if (x < 0 || y < top || x >= width || y >= top + height) return -1;
  return (y - top) * rows / height * columns + x * columns / width;
}

}  // namespace remote
