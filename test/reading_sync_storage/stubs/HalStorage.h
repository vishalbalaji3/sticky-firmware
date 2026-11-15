#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace fake {
inline std::map<std::string, std::vector<uint8_t>> files;
inline std::string failWrite;
}  // namespace fake
class HalFile {
 public:
  std::string path;
  size_t read(void* dst, size_t size) {
    const auto& bytes = fake::files.at(path);
    size = std::min(size, bytes.size());
    std::memcpy(dst, bytes.data(), size);
    return size;
  }
  size_t write(const void* src, size_t size) {
    if (path == fake::failWrite) return 0;
    const auto* bytes = static_cast<const uint8_t*>(src);
    fake::files[path].assign(bytes, bytes + size);
    return size;
  }
  void flush() {}
};
struct TestStorage {
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    file.path = path;
    return fake::files.count(path) != 0;
  }
  bool openFileForWrite(const char*, const std::string& path, HalFile& file) {
    file.path = path;
    fake::files[path].clear();
    return true;
  }
  void remove(const char* path) { fake::files.erase(path); }
  bool rename(const char* from, const char* to) {
    if (!fake::files.count(from) || fake::files.count(to)) return false;
    fake::files[to] = std::move(fake::files[from]);
    fake::files.erase(from);
    return true;
  }
};
inline TestStorage Storage;
