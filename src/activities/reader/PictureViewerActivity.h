#pragma once

#include <string>

#include "activities/Activity.h"

// Full-screen view of one book picture: 1x/2x/3x zoom on Up/Down or a tap, finger drag to pan,
// Back or the corner X to return.
// Adds no voice place, so a recording made here keeps the reader's place below it on the stack.
class PictureViewerActivity final : public Activity {
 public:
  PictureViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string imagePath)
      : Activity("PictureViewer", renderer, mappedInput), imagePath(std::move(imagePath)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool drawPicture();
  void drawBadge();

  std::string imagePath;
  GfxRenderer::Orientation readerOrientation = GfxRenderer::Portrait;
  int imageWidth = 0;
  int imageHeight = 0;
  int zoom = 1;
  bool showHint = true;  // until the first zoom or pan
  int panX = 0;
  int panY = 0;
  bool dragging = false;
  int dragStartX = 0;
  int dragStartY = 0;
  int dragLastX = 0;
  int dragLastY = 0;
};
