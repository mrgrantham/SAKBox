// The corner widgets: the ViewCube's cells and what's under the cursor,
// its snapping and compass; Blender's axis gizmo's balls and clicks
#include <AnimationLab/AxisGizmo.hpp>
#include <AnimationLab/ViewCube.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using AnimationLab::AxisGizmo;
using AnimationLab::OrbitView;
using AnimationLab::ScreenPoint;
using AnimationLab::ViewCube;

namespace {
constexpr ScreenPoint kCentre{100, 100};
constexpr float kHalf = 40;
} // namespace

// Square on to the front, only the front's 9 cells face you; its middle is
// the front, its right-hand strip the front-right edge, its top-right
// corner the front-top-right corner
TEST(ViewCubeTest, FrontOnShowsTheFrontsCells) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  const auto cells = ViewCube::cells(view, kCentre, kHalf);
  EXPECT_EQ(cells.size(), 9u);
  EXPECT_EQ(ViewCube::pick(cells, kCentre), (ViewCube::Way{0, 0, 1}));
  EXPECT_EQ(ViewCube::pick(cells, {kCentre.x + kHalf * 0.8f, kCentre.y}), (ViewCube::Way{1, 0, 1}));
  EXPECT_EQ(ViewCube::pick(cells, {kCentre.x + kHalf * 0.8f, kCentre.y - kHalf * 0.8f}),
            (ViewCube::Way{1, 1, 1}));
  EXPECT_FALSE(ViewCube::pick(cells, {kCentre.x + kHalf * 1.5f, kCentre.y}).has_value());
  ASSERT_TRUE(ViewCube::faceMiddle(view, kCentre, kHalf, 4).has_value());  // (FRONT)
  EXPECT_FALSE(ViewCube::faceMiddle(view, kCentre, kHalf, 5).has_value()); // (BACK)
}

// From a corner-ish view, three faces' cells face you, and every way picked
// is one of the 26
TEST(ViewCubeTest, AtAnAngleThreeFacesShow) {
  OrbitView view; // (the lab's starting turn: a little right and above)
  const auto cells = ViewCube::cells(view, kCentre, kHalf);
  EXPECT_EQ(cells.size(), 27u);
  for (const auto &cell : cells)
    EXPECT_FALSE(cell.way[0] == 0 && cell.way[1] == 0 && cell.way[2] == 0);
}

// A face's name is written on it in 3D: square on to the front, its right
// is the screen's right and its down the screen's down, a unit of the cube
// `half` pixels; from the right, the right face's right is the screen's;
// the top seen from straight above reads with the front at the bottom
TEST(ViewCubeTest, FaceNamesLieOnTheirFaces) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  const auto front = ViewCube::faceAxes(view, kCentre, kHalf, 4);
  EXPECT_NEAR(front.middle.x, kCentre.x, 1e-3f);
  EXPECT_NEAR(front.right.x, kHalf, 1e-3f);
  EXPECT_NEAR(front.right.y, 0, 1e-3f);
  EXPECT_NEAR(front.down.y, kHalf, 1e-3f);
  view.pose = view.looking(OrbitView::Side::Right);
  const auto right = ViewCube::faceAxes(view, kCentre, kHalf, 0);
  EXPECT_NEAR(right.right.x, kHalf, 1e-3f);
  view.pose = view.looking(OrbitView::Side::Top);
  const auto top = ViewCube::faceAxes(view, kCentre, kHalf, 2);
  EXPECT_NEAR(top.right.x, kHalf, 1e-2f);
  EXPECT_NEAR(top.down.y, kHalf, 1e-2f); // (toward the front: down the screen)
}

// Clicking a cell and facing its way looks from there: picking the
// front-right edge turns the view to 45 degrees between them
TEST(ViewCubeTest, ACellsWayIsItsView) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  const auto cells = ViewCube::cells(view, kCentre, kHalf);
  const auto way = ViewCube::pick(cells, {kCentre.x + kHalf * 0.8f, kCentre.y});
  ASSERT_TRUE(way.has_value());
  view.pose = view.facing(ViewCube::directionOf(*way));
  EXPECT_NEAR(view.pose.yaw, OrbitView::kPi / 4, 1e-5f);
  EXPECT_NEAR(view.pose.pitch, 0, 1e-5f);
}

// Letting go of a drag near a view settles on it; far from all, nothing
TEST(ViewCubeTest, SnapsWithinTenDegrees) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Right);
  view.pose.yaw += 0.1f; // (about 6 degrees off)
  EXPECT_EQ(ViewCube::nearest(view), (ViewCube::Way{1, 0, 0}));
  view.pose.yaw = OrbitView::kPi / 2 - OrbitView::kPi / 8; // (22.5: halfway to an edge)
  EXPECT_FALSE(ViewCube::nearest(view).has_value());
}

// The compass ring sits under the cube; its N is away from the front: seen
// from the front, at the top of the ring
TEST(ViewCubeTest, TheCompassUnderTheCube) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  view.pose.pitch = 0.5f;
  const auto letters = ViewCube::compassLetters(view, kCentre, kHalf);
  EXPECT_LT(letters[0].y, letters[2].y); // (N above S on the screen)
  EXPECT_GT(letters[1].x, letters[3].x); // (E right of W)
  const auto ring = ViewCube::compass(view, kCentre, kHalf);
  EXPECT_TRUE(ViewCube::onCompass(view, kCentre, kHalf, ring[5]));
  EXPECT_FALSE(ViewCube::onCompass(view, kCentre, kHalf, kCentre));
}

// The gizmo: seen from the front, X points right, Y up, Z at you (nearest,
// drawn last); clicking X looks from +x, and clicking it again from there
// looks from -x (Blender's way)
TEST(AxisGizmoTest, BallsAndClicks) {
  OrbitView view;
  view.pose = view.looking(OrbitView::Side::Front);
  view.pose.yaw = 0.01f; // (a hair off, so the depths differ)
  const auto balls = AxisGizmo::balls(view, kCentre, kHalf);
  EXPECT_EQ(balls.back().axis, 2); // (+z, toward you)
  EXPECT_EQ(balls.back().sign, 1);
  const auto x = AxisGizmo::pick(balls, {kCentre.x + kHalf, kCentre.y}, 8);
  ASSERT_TRUE(x.has_value());
  EXPECT_EQ(x->axis, 0);
  EXPECT_EQ(x->sign, 1);
  const auto y = AxisGizmo::pick(balls, {kCentre.x, kCentre.y - kHalf}, 8);
  ASSERT_TRUE(y.has_value());
  EXPECT_EQ(y->axis, 1);
  EXPECT_FLOAT_EQ(AxisGizmo::lookFrom(view, *x).x, 1);
  view.pose = view.facing({1, 0, 0});
  EXPECT_FLOAT_EQ(AxisGizmo::lookFrom(view, *x).x, -1);
}
