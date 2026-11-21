#include "CaptiveSignin.h"

#include <Logging.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_random.h>

#include <cstring>

namespace captivesignin {
namespace {
constexpr const char* kSsid = "Sticky-signin";
char password[9] = {};  // 8 chars + NUL, fresh for every sign-in
// A subnet airports rarely hand out, so the hotspot can't shadow the portal's own.
const IPAddress kApIp(192, 168, 47, 1);
}  // namespace

bool behindPortal() {
  WiFiClient client;
  client.setTimeout(4000);  // ms (Stream)
  if (!client.connect("connectivitycheck.gstatic.com", 80, 4000)) return false;
  client.print("GET /generate_204 HTTP/1.1\r\nHost: connectivitycheck.gstatic.com\r\nConnection: close\r\n\r\n");
  char line[32] = {};
  const size_t n = client.readBytesUntil('\n', line, sizeof(line) - 1);
  client.stop();
  // "HTTP/1.1 204 No Content": the status code starts at column 9.
  if (n < 12 || std::strncmp(line, "HTTP/", 5) != 0) return false;
  const bool portal = std::strncmp(line + 9, "204", 3) != 0;
  LOG_DBG("PORTAL", "Connectivity check: %.12s -> %s", line, portal ? "portal" : "online");
  return portal;
}

bool start(const char*& ssid, const char*& pass) {
  static constexpr char kChars[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (size_t i = 0; i + 1 < sizeof(password); ++i) password[i] = kChars[esp_random() % (sizeof(kChars) - 1)];

  const IPAddress upstreamDns = WiFi.dnsIP(0);
  // AP_STA keeps the station link; the AP must sit on the station's channel.
  if (!WiFi.mode(WIFI_AP_STA) ||
      !WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0), IPAddress(192, 168, 47, 2), upstreamDns) ||
      !WiFi.softAP(kSsid, password, WiFi.channel(), false, 1) || !WiFi.AP.enableNAPT(true)) {
    LOG_ERR("PORTAL", "Could not start the sign-in hotspot");
    stop();
    return false;
  }
  ssid = kSsid;
  pass = password;
  return true;
}

void stop() {
  if (!WiFi.AP.started()) return;
  WiFi.AP.enableNAPT(false);
  WiFi.softAPdisconnect(true);  // drops the AP half only; STA stays joined
}

}  // namespace captivesignin
