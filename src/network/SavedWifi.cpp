#include "SavedWifi.h"

#include <TrustedTime.h>
#include <WiFi.h>

#include "WifiCredentialStore.h"

bool hasLastSavedWifi() {
  WIFI_STORE.loadFromFile();
  return WIFI_STORE.findCredential(WIFI_STORE.getLastConnectedSsid()).has_value();
}

bool joinLastSavedWifi(const unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  const auto cred = WIFI_STORE.findCredential(WIFI_STORE.getLastConnectedSsid());
  if (!cred) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
  const unsigned long deadline = millis() + timeoutMs;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  trustedtime::startSync();  // snap the clock floor while the network is up
  return true;
}

void stopWifi() {
  WiFi.disconnect(false);
  delay(30);
  WiFi.mode(WIFI_OFF);
}
