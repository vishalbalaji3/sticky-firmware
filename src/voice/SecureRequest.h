#pragma once

#include <SecureClient.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace voice {

// One verified HTTPS exchange. The SDK's HTTP client needs the whole body in memory, but audio can
// be far larger than PSRAM, so this streams both ways. It asks for HTTP/1.0 so responses are never
// chunked. Returns the status, or -1 if the connection or certificate check failed.
struct Request {
  std::string host;
  const char* method = "GET";
  std::string target;
  std::string token;
  const char* contentType = nullptr;
  size_t bodyLength = 0;
  std::function<bool(freeink::SecureClient&)> sendBody;
  std::function<void(const uint8_t*, size_t)> onBody;
};
int secureRequest(const Request& request);

bool writeAll(freeink::SecureClient& client, const uint8_t* data, size_t size);

// Short JSON files only.
bool readTextFile(const std::string& path, std::string& out, size_t maxBytes = 8 * 1024);

// The settings file on the card, created from the PWA's device token.
struct Config {
  std::string host;
  std::string token;
  std::string tvHost;
};
bool loadConfig(Config& config);

}  // namespace voice
