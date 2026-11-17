// The dice as drawn (round 87): every face's pips on it (21 in all), the
// glass container filling the screen in front of the camera, and a die
// turned as its quaternion says
#include <CosmicHex/Handheld/DicePieces.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Handheld;

TEST(DicePiecesTest, ADieHasItsPips) {
  const DicePieces pieces;
  // The box's 12 triangles and 21 pips of 2
  EXPECT_EQ(pieces.die.triangleCount(), 12u + 21u * 2u);
}

TEST(DicePiecesTest, TheGlassFillsTheScreen) {
  const Scene::Camera camera{.eye = {0, 9.5f, 9.5f}, .target = {0, 0, 1.2f}, .fieldOfView = 50};
  const float aspect = 400.0f / 240;
  const Scene::Transform glass = glassFrame(camera, aspect, 10, 6);
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  const Scene::Vector3 up = right.cross(forward);
  // Its middle straight ahead, 6 away
  const Scene::Vector3 middle = glass.apply({0, 0, 0}) - camera.eye;
  EXPECT_NEAR(middle.dot(forward), 6, 1e-3);
  EXPECT_NEAR(middle.dot(right), 0, 1e-3);
  // Its corners at the screen's edges (within it), the right on the right
  // and the far edge (-z) at the top
  const float halfTall = std::tan(25 * 3.14159265f / 180);
  for (const float sx : {-1.0f, 1.0f})
    for (const float sz : {-1.0f, 1.0f}) {
      const Scene::Vector3 p = glass.apply({5 * sx, 0, 3 * sz}) - camera.eye;
      const float across = p.dot(right) / p.dot(forward), along = p.dot(up) / p.dot(forward);
      EXPECT_LE(std::abs(across), halfTall * aspect + 1e-3f);
      EXPECT_LE(std::abs(along), halfTall + 1e-3f);
      EXPECT_GT(across * sx, 0);
      EXPECT_LT(along * sz, 0);
    }
  // A die at rest is in front of the floor, towards the camera
  const Scene::Vector3 die = glass.apply({0, 0.5f, 0}) - camera.eye;
  EXPECT_LT(die.dot(forward), 6);
}

TEST(DicePiecesTest, ADieTurnsAsItsQuaternion) {
  const Scene::Transform glass = Scene::Transform::identity();
  // A quarter turn about y (w = cos 45, y = sin 45): x goes to -z
  const float h = std::sqrt(0.5f);
  const Scene::Transform t = dieOnGlass(glass, {1, 2, 3}, {h, 0, h, 0}, 0.5f);
  const Scene::Vector3 x = t.applyToDirection({1, 0, 0});
  EXPECT_NEAR(x.z, -1, 1e-5);
  EXPECT_NEAR(x.x, 0, 1e-5);
  EXPECT_NEAR(t.position().x, 0.5f, 1e-5); // (moved half a unit left by the pull)
  EXPECT_NEAR(t.position().y, 2, 1e-5);
}
