#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

namespace readingsync {
struct State {
  uint64_t scope = 0;
  uint64_t revision = 0;
  bool known = false;
  bool dirty = false;
};

enum class Decision { Keep, Download, Upload, Choose };

// The revision identifies a shared ancestor, not how far either reader got.
inline Decision reconcile(const State& local, uint64_t scope, std::optional<uint64_t> remoteRevision) {
  if (!local.known || local.scope != scope || !remoteRevision) return Decision::Choose;
  if (*remoteRevision < local.revision) return Decision::Choose;
  if (*remoteRevision != local.revision) return local.dirty ? Decision::Choose : Decision::Download;
  return local.dirty ? Decision::Upload : Decision::Keep;
}

inline uint64_t hash(const uint8_t* bytes, size_t size, uint64_t value = 14695981039346656037ULL) {
  for (size_t i = 0; i < size; ++i) value = (value ^ bytes[i]) * 1099511628211ULL;
  return value;
}

inline uint64_t scope(const std::string& server, const std::string& user, const std::string& document) {
  uint64_t value = 14695981039346656037ULL;
  for (const auto* part : {&server, &user, &document}) {
    value = hash(reinterpret_cast<const uint8_t*>(part->data()), part->size(), value);
    const uint8_t separator = 0;
    value = hash(&separator, 1, value);
  }
  return value;
}

constexpr size_t RECORD_SIZE = 40;
inline void put64(uint8_t* data, uint64_t value) {
  for (size_t i = 0; i < 8; ++i) data[i] = static_cast<uint8_t>(value >> (i * 8));
}
inline uint64_t get64(const uint8_t* data) {
  uint64_t value = 0;
  for (size_t i = 0; i < 8; ++i) value |= static_cast<uint64_t>(data[i]) << (i * 8);
  return value;
}
inline void encode(const State& state, const uint8_t* progress, size_t size, uint8_t* out) {
  out[0] = 'S';
  out[1] = 'Y';
  out[2] = 'N';
  out[3] = 1;
  out[4] = state.known;
  out[5] = state.dirty;
  out[6] = 0;
  out[7] = 0;
  put64(out + 8, state.scope);
  put64(out + 16, state.revision);
  put64(out + 24, hash(progress, size));
  put64(out + 32, hash(out, 32));
}
inline State decode(const uint8_t* data, size_t size, const uint8_t* progress, size_t progressSize) {
  if (size != RECORD_SIZE || data[0] != 'S' || data[1] != 'Y' || data[2] != 'N' || data[3] != 1 || data[4] > 1 ||
      data[5] > 1 || data[6] || data[7] || get64(data + 24) != hash(progress, progressSize) ||
      get64(data + 32) != hash(data, 32))
    return {};
  return {get64(data + 8), get64(data + 16), data[4] != 0, data[5] != 0};
}
}  // namespace readingsync
