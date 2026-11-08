#include <gtest/gtest.h>

#include "activities/reader/PictureViewport.h"

using picture_viewport::layout;
using picture_viewport::panForZoom;
using picture_viewport::shouldRotate;
using picture_viewport::tapZoom;

TEST(PictureViewport, RotatesOnlyLandscapePicturesOnPortraitScreens) {
  EXPECT_TRUE(shouldRotate(1200, 800, 480, 800));
  EXPECT_FALSE(shouldRotate(800, 1200, 480, 800));
  EXPECT_FALSE(shouldRotate(1000, 1000, 480, 800));
  EXPECT_FALSE(shouldRotate(1200, 800, 800, 480));
}

TEST(PictureViewport, FitsAndCentersAtOneX) {
  int panX = 50;
  int panY = 50;
  const auto view = layout(1600, 800, 800, 480, 1, panX, panY);
  EXPECT_EQ(view.width, 800);
  EXPECT_EQ(view.height, 400);
  EXPECT_EQ(view.x, 0);
  EXPECT_EQ(view.y, 40);
  EXPECT_EQ(panX, 0);
  EXPECT_EQ(panY, 0);
}

TEST(PictureViewport, UpscalesSmallPicturesToFit) {
  int panX = 0;
  int panY = 0;
  const auto view = layout(100, 200, 480, 800, 1, panX, panY);
  EXPECT_EQ(view.width, 400);
  EXPECT_EQ(view.height, 800);
  EXPECT_EQ(view.x, 40);
}

TEST(PictureViewport, ClampsPanAtZoom) {
  int panX = 5000;
  int panY = -30;
  const auto view = layout(1600, 800, 800, 480, 3, panX, panY);
  EXPECT_EQ(view.width, 2400);
  EXPECT_EQ(view.height, 1200);
  EXPECT_EQ(panX, 1600);
  EXPECT_EQ(panY, 0);
  EXPECT_EQ(view.x, -1600);
  EXPECT_EQ(view.y, 0);
}

TEST(PictureViewport, ZoomKeepsScreenCenter) {
  int panX = 0;
  int panY = 0;
  // 1x: 800x400 centered at y=40. Center (400, 240) is image point (400, 200).
  auto view = layout(1600, 800, 800, 480, 1, panX, panY);
  panX = panForZoom(view.x, 400, 800, 1, 2);
  panY = panForZoom(view.y, 240, 480, 1, 2);
  view = layout(1600, 800, 800, 480, 2, panX, panY);
  EXPECT_EQ(panX, 400);  // image point 800 of 1600 at screen x 400
  EXPECT_EQ(panY, 160);  // image point 400 of 800 at screen y 240
  // And back out to 1x recenters.
  panX = panForZoom(view.x, 400, 800, 2, 1);
  panY = panForZoom(view.y, 240, 480, 2, 1);
  view = layout(1600, 800, 800, 480, 1, panX, panY);
  EXPECT_EQ(view.x, 0);
  EXPECT_EQ(view.y, 40);
}

TEST(PictureViewport, TapZoomCentersTappedPoint) {
  int panX = 0;
  int panY = 0;
  // 1x: 800x400 at y=40. Tap (200, 140) is image point (200, 100); at 2x that is (400, 200).
  auto view = layout(1600, 800, 800, 480, 1, panX, panY);
  panX = panForZoom(view.x, 200, 800, 1, 2);
  panY = panForZoom(view.y, 140, 480, 1, 2);
  view = layout(1600, 800, 800, 480, 2, panX, panY);
  EXPECT_EQ(panX, 0);  // 400 - 400
  EXPECT_EQ(panY, 0);  // 200 - 240, clamped
  // A tap near the far corner clamps so no margin opens.
  panX = 800;
  panY = 320;
  view = layout(1600, 800, 800, 480, 2, panX, panY);
  panX = panForZoom(view.x, 790, 800, 2, 3);
  panY = panForZoom(view.y, 470, 480, 2, 3);
  view = layout(1600, 800, 800, 480, 3, panX, panY);
  EXPECT_EQ(panX, 1600);
  EXPECT_EQ(panY, 720);
}

TEST(PictureViewport, TapZoomStepsThenResets) {
  EXPECT_EQ(tapZoom(1), 2);
  EXPECT_EQ(tapZoom(2), 3);
  EXPECT_EQ(tapZoom(3), 1);
}
