#include "PictureViewerActivity.h"

#include <Epub/converters/ImageDecoderFactory.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>

#include <algorithm>
#include <cstdlib>

#include "MappedInputManager.h"
#include "PictureViewport.h"
#include "activities/RenderLock.h"
#include "fontIds.h"

namespace {
constexpr int DRAG_SLOP = 12;
}

void PictureViewerActivity::onEnter() {
  Activity::onEnter();
  readerOrientation = renderer.getOrientation();
  ImageDimensions dims{0, 0};
  auto* decoder = (FsHelpers::hasJpgExtension(imagePath) || FsHelpers::hasPngExtension(imagePath))
                      ? ImageDecoderFactory::getDecoder(imagePath)
                      : nullptr;
  // getDimensions() rejects pictures over the decoders' 8 MP cap; render() then shows a message.
  if (decoder && decoder->getDimensions(imagePath, dims) && dims.width > 0 && dims.height > 0) {
    imageWidth = dims.width;
    imageHeight = dims.height;
    if (picture_viewport::shouldRotate(imageWidth, imageHeight, renderer.getScreenWidth(),
                                       renderer.getScreenHeight())) {
      // One quarter turn: Portrait -> LandscapeClockwise, PortraitInverted -> LandscapeCounterClockwise.
      renderer.setOrientation(static_cast<GfxRenderer::Orientation>((readerOrientation + 1) % 4));
    }
  }
  requestUpdate();
}

void PictureViewerActivity::onExit() {
  Activity::onExit();
  renderer.setOrientation(readerOrientation);
  // Clear the gray picture so the reader's next fast refresh starts from a clean panel.
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}

void PictureViewerActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (imageWidth == 0) return;

  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const bool zoomIn = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool zoomOut = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const int newZoom = zoomIn ? std::min(zoom + 1, picture_viewport::MAX_ZOOM) : zoomOut ? std::max(zoom - 1, 1) : zoom;
  if (newZoom != zoom) {
    {
      RenderLock lock;  // every plane of one render must see the same viewport
      const auto view = picture_viewport::layout(imageWidth, imageHeight, screenW, screenH, zoom, panX, panY);
      panX = picture_viewport::panForZoom(view.x, screenW, zoom, newZoom);
      panY = picture_viewport::panForZoom(view.y, screenH, zoom, newZoom);
      zoom = newZoom;
    }
    requestUpdate();
    return;
  }

  // E-paper is too slow to follow the finger, so the picture moves once when the drag ends.
  int x = 0;
  int y = 0;
  if (mappedInput.isScreenTouchHeld(x, y)) {
    if (!dragging) {
      dragging = true;
      dragStartX = x;
      dragStartY = y;
    }
    dragLastX = x;
    dragLastY = y;
    return;
  }
  if (!dragging) return;
  dragging = false;
  const int dx = dragLastX - dragStartX;
  const int dy = dragLastY - dragStartY;
  if (zoom == 1 || (std::abs(dx) < DRAG_SLOP && std::abs(dy) < DRAG_SLOP)) return;
  bool moved;
  {
    RenderLock lock;
    const int oldPanX = panX;
    const int oldPanY = panY;
    panX -= dx;
    panY -= dy;
    picture_viewport::layout(imageWidth, imageHeight, screenW, screenH, zoom, panX, panY);
    moved = panX != oldPanX || panY != oldPanY;
  }
  if (moved) requestUpdate();
}

bool PictureViewerActivity::drawPicture() {
  auto* decoder = ImageDecoderFactory::getDecoder(imagePath);
  if (!decoder) return false;
  const auto view = picture_viewport::layout(imageWidth, imageHeight, renderer.getScreenWidth(),
                                             renderer.getScreenHeight(), zoom, panX, panY);
  RenderConfig config{view.x, view.y, view.width, view.height};
  config.useExactDimensions = true;
  config.useDithering = false;  // the panel's 4 grays keep fine lines sharp
  // shortcut: every plane re-decodes the whole picture (slow at 3x on big files); cache it if that hurts.
  return decoder->decodeToFramebuffer(imagePath, renderer, config);
}

// Same base + two gray planes + BW rebuild sequence as BmpViewerActivity::renderImage().
void PictureViewerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (imageWidth == 0 || !drawPicture()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_FILE_OPEN_FAILED));
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    return;
  }

  const bool absolute = renderer.grayscaleCapabilities(HalDisplay::GrayscaleMode::Absolute).supported();
  if (absolute && !renderer.displayGrayscaleBase(HalDisplay::GrayscaleMode::Absolute)) return;
  if (!absolute) renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  bool ready = true;
  for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
    renderer.clearScreen(absolute ? 0xFF : 0x00);
    renderer.setRenderMode(mode);
    if (!drawPicture()) {
      ready = false;
      break;
    }
    if (mode == GfxRenderer::GRAYSCALE_LSB) {
      renderer.copyGrayscaleLsbBuffers();
    } else {
      renderer.copyGrayscaleMsbBuffers();
    }
  }
  if (ready) renderer.displayGrayBuffer();

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  drawPicture();
  renderer.cleanupGrayscaleWithFrameBuffer();
}
