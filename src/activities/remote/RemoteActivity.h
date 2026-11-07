#pragma once

#include <SecureClient.h>

#include <string>

#include "activities/Activity.h"

// Touch remote for homelab-tv, on the home network. Pairs once with the code shown on the TV.
class RemoteActivity final : public Activity {
 public:
  RemoteActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Remote", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void onWifiReady();
  void requestPairing();
  void completePairing(const std::string& code);
  bool ensureConnected();
  void send(const char* commandJson);
  void showMessage(const std::string& text);

  freeink::SecureClient socket;
  std::string tvHost;
  std::string tvToken;
  std::string pairingId;
  std::string message = "Connecting...";
  uint32_t nextId = 1;
  bool ready = false;
};
