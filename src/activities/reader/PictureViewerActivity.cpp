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
#include "components/HeaderBackTapTarget.h"
#include "fontIds.h"

namespace {
constexpr int DRAG_SLOP = 12;
constexpr int CLOSE_SIZE = 44;  // finger-sized close box
constexpr int BADGE_PAD = 10;
}  // namespace

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
  int focusX = screenW / 2;
  int focusY = screenH / 2;
  int newZoom = zoom;
  // Sticky exception to FreeInkUI (STICKY.md): zoom around the tapped point and drag-to-pan have no FUI component.
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    newZoom = std::min(zoom + 1, picture_viewport::MAX_ZOOM);
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    newZoom = std::max(zoom - 1, 1);
  } else if (mappedInput.wasScreenTapped(focusX, focusY)) {
    dragging = false;
    newZoom = picture_viewport::tapZoom(zoom);
  }
  if (newZoom != zoom) {
    {
      RenderLock lock;  // every plane of one render must see the same viewport
      const auto view = picture_viewport::layout(imageWidth, imageHeight, screenW, screenH, zoom, panX, panY);
      panX = picture_viewport::panForZoom(view.x, focusX, screenW, zoom, newZoom);
      panY = picture_viewport::panForZoom(view.y, focusY, screenH, zoom, newZoom);
      zoom = newZoom;
      showHint = false;
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
    if (moved) showHint = false;
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
  if (!decoder->decodeToFramebuffer(imagePath, renderer, config)) return false;
  drawBadge();
  return true;
}

// Top-left box: a close X (tapped as Back), the zoom level and, until the first change, the hint.
// Absolute gray planes and BW take the same white box and black ink (UI_10 is a 1-bit font, which
// paints with its state on every plane); overlay gray masks are cleared so the BW base shows there.
void PictureViewerActivity::drawBadge() {
  char zoomText[4];
  snprintf(zoomText, sizeof(zoomText), "%dx", zoom);
  const char* hint = tr(STR_PICTURE_VIEWER_HINT);
  const int zoomWidth = renderer.getTextWidth(UI_10_FONT_ID, zoomText);
  const int hintWidth = showHint ? renderer.getTextWidth(UI_10_FONT_ID, hint) + BADGE_PAD : 0;
  const int width = std::min(CLOSE_SIZE + zoomWidth + 2 * BADGE_PAD + hintWidth, renderer.getScreenWidth() - 1);
  HeaderBackTapTarget::set(0, 0, CLOSE_SIZE, CLOSE_SIZE);

  if (renderer.getRenderMode() != GfxRenderer::BW && !renderer.grayPlanesAreAbsolute()) {
    renderer.fillRect(0, 0, width + 1, CLOSE_SIZE + 1, true);
    return;
  }
  constexpr int CROSS_INSET = 14;
  const int textY = (CLOSE_SIZE - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.fillRect(0, 0, width, CLOSE_SIZE, false);
  renderer.drawLine(0, CLOSE_SIZE, width, CLOSE_SIZE, true);
  renderer.drawLine(width, 0, width, CLOSE_SIZE, true);
  renderer.drawLine(CLOSE_SIZE, 0, CLOSE_SIZE, CLOSE_SIZE, true);
  renderer.drawLine(CROSS_INSET, CROSS_INSET, CLOSE_SIZE - CROSS_INSET, CLOSE_SIZE - CROSS_INSET, 2, true);
  renderer.drawLine(CLOSE_SIZE - CROSS_INSET, CROSS_INSET, CROSS_INSET, CLOSE_SIZE - CROSS_INSET, 2, true);
  renderer.drawText(UI_10_FONT_ID, CLOSE_SIZE + BADGE_PAD, textY, zoomText);
  if (showHint) renderer.drawText(UI_10_FONT_ID, CLOSE_SIZE + zoomWidth + 2 * BADGE_PAD, textY, hint);
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
