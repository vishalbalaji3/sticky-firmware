#pragma once

#include <algorithm>
#include <string>
#include <vector>

// Joins a saved network without any UI, for background work that must not
// show the network picker. Call hasSavedWifi() first: it loads the saved
// credentials from the card.
bool hasSavedWifi();
// One quick scan, then the saved networks in range, strongest first, until
// one connects or timeoutMs runs out. Returns true when connected.
bool joinSavedWifi(unsigned long timeoutMs);
// Already connected, or hasSavedWifi() and joinSavedWifi().
bool connectSavedWifi(unsigned long timeoutMs);
// Turns the radio fully off, to save battery. No-op when already off.
void stopWifi();

struct ScannedNetwork {
  std::string ssid;
  int rssi;
};

// Indexes into scan of the saved networks to try: strongest first, one access point per SSID.
inline std::vector<size_t> savedJoinOrder(const std::vector<ScannedNetwork>& scan,
                                          const std::vector<std::string>& saved) {
  std::vector<size_t> order;
  for (size_t i = 0; i < scan.size(); ++i) {
    if (std::find(saved.begin(), saved.end(), scan[i].ssid) != saved.end()) order.push_back(i);
  }
  std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return scan[a].rssi > scan[b].rssi; });
  std::vector<size_t> unique;
  for (const size_t i : order) {
    const bool seen = std::any_of(unique.begin(), unique.end(), [&](size_t u) { return scan[u].ssid == scan[i].ssid; });
    if (!seen) unique.push_back(i);
  }
  return unique;
}
