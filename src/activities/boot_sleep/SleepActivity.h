#pragma once
#include <ArduinoJson.h>

#include <string>

#include "activities/Activity.h"

class Bitmap;
class HalFile;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderLockScreen() const;
  void drawLockOverlay(const JsonDocument& doc, bool band) const;
  void drawDateBand(const JsonDocument& doc) const;
  void drawAsleepMarker(bool compact = false) const;
  void drawMoon(int x, int y, int size) const;
  bool renderCustomSleepBmp(const std::string& path) const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, bool preserveBackground = false) const;
  bool renderSleepOverlayFile(HalFile& file, const char* pathForLog) const;
  bool renderTransparentOverlayPng(const std::string& path) const;
  bool renderSleepOverlayPath(const std::string& path) const;
  void renderLastScreenSleepScreen() const;
  void renderTransparentCustomSleepScreen() const;
  void renderBlankSleepScreen() const;

  bool fromTimeout = false;
  // Cover + Transparent: the cover under the overlay, and the same cover while its gray planes are redrawn.
  mutable const Bitmap* overlayCover = nullptr;
  mutable const Bitmap* overlayGrayCover = nullptr;
  // The lock screen's band and marker, drawn by renderBitmapSleepScreen into each pass of the wallpaper's paint.
  // Null outside renderLockScreen.
  mutable const JsonDocument* lockOverlayDoc = nullptr;
  mutable bool lockOverlayBand = false;
};
