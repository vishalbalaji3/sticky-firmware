#include "SavedWifi.h"

#include <Logging.h>
#include <TrustedTime.h>
#include <WiFi.h>

#include <cstring>

#include "WifiCredentialStore.h"

namespace {
// Active scan per channel: about 1.5 s over all channels, the cost when no saved network is near.
constexpr uint32_t kScanMsPerChannel = 120;
// A join with DHCP usually takes 1-3 s. The last candidate gets whatever budget is left.
constexpr unsigned long kAttemptMs = 4000;

struct Candidate {
  std::string ssid;
  uint8_t bssid[6];
  int32_t channel;
};

// shortcut: hidden SSIDs never show in the scan, so a hidden saved network is not joined here;
// add a direct begin() of the last-used SSID if one is ever saved.
std::vector<Candidate> scanSaved() {
  std::vector<std::string> saved;
  for (size_t i = 0; i < WIFI_STORE.getCredentialCount(); ++i) {
    if (auto ssid = WIFI_STORE.getSsidAt(i)) saved.push_back(std::move(*ssid));
  }
  const int16_t found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false, /*passive=*/false, kScanMsPerChannel);
  std::vector<ScannedNetwork> scan;
  for (int16_t i = 0; i < found; ++i) scan.push_back({WiFi.SSID(i).c_str(), WiFi.RSSI(i)});
  std::vector<Candidate> candidates;
  for (const size_t i : savedJoinOrder(scan, saved)) {
    Candidate c{scan[i].ssid, {}, WiFi.channel(i)};
    memcpy(c.bssid, WiFi.BSSID(i), sizeof(c.bssid));
    candidates.push_back(std::move(c));
  }
  WiFi.scanDelete();
  return candidates;
}
}  // namespace

bool hasSavedWifi() {
  WIFI_STORE.loadFromFile();
  return WIFI_STORE.getCredentialCount() > 0;
}

bool joinSavedWifi(const unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  const unsigned long start = millis();
  WiFi.mode(WIFI_STA);
  const std::vector<Candidate> candidates = scanSaved();
  for (size_t i = 0; i < candidates.size(); ++i) {
    const unsigned long elapsed = millis() - start;
    const auto cred = WIFI_STORE.findCredential(candidates[i].ssid);
    if (!cred || elapsed >= timeoutMs) continue;
    const bool last = i + 1 == candidates.size();
    const unsigned long attemptMs = last ? timeoutMs - elapsed : std::min(kAttemptMs, timeoutMs - elapsed);
    LOG_DBG("WIFI", "Background join: %s", cred->ssid.c_str());
    WiFi.disconnect(false);
    // The scanned channel and BSSID skip the driver's own scan. Only CONNECTED is trusted:
    // the core updates the status from async events, so a failure code can be stale.
    WiFi.begin(cred->ssid.c_str(), cred->password.c_str(), candidates[i].channel, candidates[i].bssid);
    const unsigned long attemptStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - attemptStart < attemptMs) delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      trustedtime::startSync();  // snap the clock floor while the network is up
      return true;
    }
  }
  return false;
}

void stopWifi() {
  WiFi.disconnect(false);
  delay(30);
  WiFi.mode(WIFI_OFF);
}
