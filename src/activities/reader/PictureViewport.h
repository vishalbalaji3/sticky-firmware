#pragma once

#include <algorithm>
#include <cstdint>

// Fit, zoom and pan math for the full-screen picture viewer.
namespace picture_viewport {

constexpr int MAX_ZOOM = 3;

// Image rectangle on screen. x/y go negative once a zoomed side is wider than the screen.
struct View {
  int x;
  int y;
  int width;
  int height;
};

// A landscape picture on a portrait screen is shown rotated to use the long edge.
inline bool shouldRotate(const int imageW, const int imageH, const int screenW, const int screenH) {
  return imageW > imageH && screenW < screenH;
}

// Fits the image to the screen, scales it by zoom, and clamps pan (the screen's offset into the
// zoomed image) so no blank margin opens on a side larger than the screen.
inline View layout(const int imageW, const int imageH, const int screenW, const int screenH, const int zoom, int& panX,
                   int& panY) {
  int fitW = screenW;
  int fitH = screenH;
  if (static_cast<int64_t>(imageW) * screenH >= static_cast<int64_t>(imageH) * screenW) {
    fitH = std::max(1, static_cast<int>(static_cast<int64_t>(imageH) * screenW / imageW));
  } else {
    fitW = std::max(1, static_cast<int>(static_cast<int64_t>(imageW) * screenH / imageH));
  }
  const int width = fitW * zoom;
  const int height = fitH * zoom;
  panX = std::clamp(panX, 0, std::max(0, width - screenW));
  panY = std::clamp(panY, 0, std::max(0, height - screenH));
  return {width <= screenW ? (screenW - width) / 2 : -panX, height <= screenH ? (screenH - height) / 2 : -panY, width,
          height};
}

// Pan for a new zoom level that keeps the image point under the screen center in place.
// viewX is that axis' View offset at the old zoom; layout() clamps the result.
inline int panForZoom(const int viewX, const int screenSize, const int oldZoom, const int newZoom) {
  return (screenSize / 2 - viewX) * newZoom / oldZoom - screenSize / 2;
}

}  // namespace picture_viewport
