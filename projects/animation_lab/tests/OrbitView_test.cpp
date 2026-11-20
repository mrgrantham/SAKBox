// OrbitView: turning, tilting, panning and zooming as Blender's viewport
// does, the numpad's views, framing, and a drag undone
#include <AnimationLab/OrbitView.hpp>

#include <gtest/gtest.h>

#include <cmath>

using AnimationLab::OrbitView;
using ScratchPadd::Scene::Vector3;

namespace {
void expectNear(const Vector3 &a, const Vector3 &b, float tolerance = 1e-3f) {
  EXPECT_NEAR(a.x, b.x, tolerance);
  EXPECT_NEAR(a.y, b.y, tolerance);
  EXPECT_NEAR(a.z, b.z, tolerance);
}
} // namespace

// The numpad's views: front from +z, right from +x, top from above
TEST(OrbitViewTest, TheNumpadsViewsLookSquareOn) {
  OrbitView view;
  view.distance = 5;
  view.look(OrbitView::Side::Front);
  expectNear(view.camera().eye, {0, 0, 5});
  view.look(OrbitView::Side::Right);
  expectNear(view.camera().eye, {5, 0, 0});
  view.look(OrbitView::Side::Left);
  expectNear(view.camera().eye, {-5, 0, 0});
  view.look(OrbitView::Side::Top);
  EXPECT_NEAR(view.camera().eye.y, 5, 1e-2f);
  // (looking down, screen up is away, -z: the camera's up stays well defined)
  expectNear(view.camera().up, {0, 0, -1}, 1e-2f);
}

// Turning keeps the horizon level and the distance; tilting stops short of
// straight down
TEST(OrbitViewTest, OrbitingIsATurntable) {
  OrbitView view;
  view.distance = 4;
  view.orbit(300, 0);
  EXPECT_NEAR(view.right().y, 0, 1e-6f); // (level)
  EXPECT_NEAR((view.camera().eye - view.target).length(), 4, 1e-4f);
  view.orbit(0, 100000);
  EXPECT_LT(view.pitch, 3.14159265f / 2);
  EXPECT_GT(view.up().length(), 0.99f);
}

// Panning moves the target across the view, a pixel a pixel's worth at its
// depth: dragging right moves the scene right (the target left)
TEST(OrbitViewTest, PanningKeepsWhatsUnderTheCursor) {
  OrbitView view;
  view.look(OrbitView::Side::Front);
  view.distance = 10;
  view.pan(100, 0, 500);
  const float perPixel = 2 * 10 * std::tan(view.fieldOfView * 3.14159265f / 360) / 500;
  expectNear(view.target, {-100 * perPixel, 0, 0});
}

// Zooming toward the cursor: the point under it stays where it was on the
// screen; at the middle, only nearer
TEST(OrbitViewTest, ZoomingGoesTowardTheCursor) {
  OrbitView view;
  view.look(OrbitView::Side::Front);
  view.distance = 10;
  view.zoomAt(1, 400, 300, 800, 600); // (the middle)
  expectNear(view.target, {0, 0, 0});
  EXPECT_NEAR(view.distance, 10 / OrbitView::kZoomStep, 1e-4f);
  OrbitView side = view;
  side.zoomAt(1, 800, 300, 800, 600); // (the right edge)
  EXPECT_GT(side.target.x, 0);
  view.zoomAt(-1000, 400, 300, 800, 600);
  EXPECT_EQ(view.distance, OrbitView::kFarthest);
}

// Framing fits a box; a drag cancelled (Esc) puts the view back
TEST(OrbitViewTest, FramingAndCancelling) {
  OrbitView view;
  view.frame({-1, -1, -1}, {1, 1, 1}, 16.0f / 9);
  expectNear(view.target, {0, 0, 0});
  const float distance = view.distance;
  EXPECT_GT(distance, std::sqrt(3.0f));
  view.begin();
  view.orbit(50, 50);
  view.pan(10, 10, 400);
  view.cancel();
  EXPECT_FALSE(view.dragging());
  expectNear(view.target, {0, 0, 0});
  EXPECT_EQ(view.distance, distance);
}
