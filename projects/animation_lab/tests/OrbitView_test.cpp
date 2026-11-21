// OrbitView: turning, tilting, panning and zooming as Blender's viewport
// does, the numpad's views, framing, glides, and a drag undone
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
constexpr float kPi = OrbitView::kPi;
} // namespace

// The numpad's views: front from +z, right from +x, top from above
TEST(OrbitViewTest, TheNumpadsViewsLookSquareOn) {
  OrbitView view;
  view.pose.distance = 5;
  view.pose = view.looking(OrbitView::Side::Front);
  expectNear(view.camera().eye, {0, 0, 5});
  view.pose = view.looking(OrbitView::Side::Right);
  expectNear(view.camera().eye, {5, 0, 0});
  view.pose = view.looking(OrbitView::Side::Left);
  expectNear(view.camera().eye, {-5, 0, 0});
  view.pose = view.looking(OrbitView::Side::Top);
  EXPECT_NEAR(view.camera().eye.y, 5, 1e-2f);
  // (looking down, screen up is away, -z: the camera's up stays well defined)
  expectNear(view.camera().up, {0, 0, -1}, 1e-2f);
}

// Turning keeps the horizon level and the distance; tilting stops short of
// straight down
TEST(OrbitViewTest, OrbitingIsATurntable) {
  OrbitView view;
  view.pose.distance = 4;
  view.orbit(300, 0);
  EXPECT_NEAR(view.right().y, 0, 1e-6f); // (level)
  EXPECT_NEAR((view.camera().eye - view.pose.target).length(), 4, 1e-4f);
  view.orbit(0, 100000);
  EXPECT_LT(view.pose.pitch, kPi / 2);
  EXPECT_GT(view.up().length(), 0.99f);
}

// Panning moves the target across the view, a pixel a pixel's worth at its
// depth: dragging right moves the scene right (the target left)
TEST(OrbitViewTest, PanningKeepsWhatsUnderTheCursor) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  view.pose.distance = 10;
  view.pan(100, 0, 500);
  const float perPixel = 2 * 10 * std::tan(view.fieldOfView * kPi / 360) / 500;
  expectNear(view.pose.target, {-100 * perPixel, 0, 0});
}

// Zooming toward the cursor: the point under it stays where it was on the
// screen; at the middle, only nearer
TEST(OrbitViewTest, ZoomingGoesTowardTheCursor) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  view.pose.distance = 10;
  view.zoomAt(1, 400, 300, 800, 600); // (the middle)
  expectNear(view.pose.target, {0, 0, 0});
  EXPECT_NEAR(view.pose.distance, 10 / OrbitView::kZoomStep, 1e-4f);
  OrbitView side = view;
  side.zoomAt(1, 800, 300, 800, 600); // (the right edge)
  EXPECT_GT(side.pose.target.x, 0);
  view.zoomAt(-1000, 400, 300, 800, 600);
  EXPECT_EQ(view.pose.distance, OrbitView::kFarthest);
}

// Framing fits a box; a drag cancelled (Esc) puts the view back
TEST(OrbitViewTest, FramingAndCancelling) {
  OrbitView view;
  view.pose = view.framing({-1, -1, -1}, {1, 1, 1}, 16.0f / 9);
  expectNear(view.pose.target, {0, 0, 0});
  const float distance = view.pose.distance;
  EXPECT_GT(distance, std::sqrt(3.0f));
  view.begin();
  view.orbit(50, 50);
  view.pan(10, 10, 400);
  view.cancel();
  EXPECT_FALSE(view.dragging());
  expectNear(view.pose.target, {0, 0, 0});
  EXPECT_EQ(view.pose.distance, distance);
}

// A glide eases from where it was to where it's going, and ends exactly
// there; halfway, it's halfway
TEST(OrbitViewTest, AGlideEndsWhereItWasSent) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  const OrbitView::Pose to = view.looking(OrbitView::Side::Right);
  view.glideTo(to, 0.4f);
  EXPECT_TRUE(view.gliding());
  view.advance(0.2f);
  EXPECT_NEAR(view.pose.yaw, kPi / 4, 1e-4f); // (eased: halfway at half time)
  view.advance(0.05f);
  EXPECT_GT(view.pose.yaw, kPi / 4);
  view.advance(1);
  EXPECT_FALSE(view.gliding());
  EXPECT_TRUE(OrbitView::same(view.pose, to));
}

// A glide goes the short way round: from just left of behind to just right
// of it is a small turn, not nearly a whole one
TEST(OrbitViewTest, AGlideTurnsTheShortWayRound) {
  OrbitView::Pose a, b;
  a.yaw = kPi - 0.1f;
  b.yaw = -kPi + 0.1f;
  const OrbitView::Pose half = OrbitView::between(a, b, 0.5f);
  EXPECT_NEAR(std::cos(half.yaw), -1, 1e-4f); // (straight behind)
}

// Distance glides evenly in ratio: halfway from 1 to 100 is 10
TEST(OrbitViewTest, DistanceGlidesInRatio) {
  OrbitView::Pose a, b;
  a.distance = 1;
  b.distance = 100;
  EXPECT_NEAR(OrbitView::between(a, b, 0.5f).distance, 10, 1e-3f);
}

// Taking hold of the view (a drag, a pan, a zoom) stops a glide where it is
TEST(OrbitViewTest, TheHandTakesOverFromAGlide) {
  OrbitView view;
  view.glideTo(view.looking(OrbitView::Side::Back));
  view.advance(0.1f);
  view.orbit(5, 0);
  EXPECT_FALSE(view.gliding());
  view.glideTo(view.looking(OrbitView::Side::Back));
  view.begin();
  EXPECT_FALSE(view.gliding());
  view.end();
  view.glideTo(view.looking(OrbitView::Side::Back));
  EXPECT_TRUE(OrbitView::same(view.destination(), view.looking(OrbitView::Side::Back)));
}

// Facing a way: a side square on; an edge between two faces; straight down
// keeps the nearest square turn rather than spinning to the front
TEST(OrbitViewTest, FacingAWay) {
  OrbitView view;
  view.pose.yaw = 0;
  view.pose = view.facing({1, 0, 0});
  EXPECT_NEAR(view.pose.yaw, kPi / 2, 1e-5f);
  EXPECT_NEAR(view.pose.pitch, 0, 1e-5f);
  view.pose = view.facing({1, 1, 0});
  EXPECT_NEAR(view.pose.pitch, kPi / 4, 1e-5f);
  view.pose = view.facing({1, 1, 1}); // (a corner)
  EXPECT_NEAR(view.pose.pitch, std::atan2(1.0f, std::sqrt(2.0f)), 1e-5f);
  view.pose.yaw = kPi - 0.2f;
  view.pose = view.facing({0, 1, 0});
  EXPECT_NEAR(view.pose.yaw, kPi, 1e-5f);
  EXPECT_NEAR(view.pose.pitch, OrbitView::kMostPitch, 1e-5f);
}
