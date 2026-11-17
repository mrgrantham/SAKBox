// The dice as drawn (round 87): every face's pips on it (21 in all), the
// glass container filling the screen in front of the camera, and a die
// turned as its quaternion says
#include <CosmicHex/Handheld/DicePieces.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Handheld;

TEST(DicePiecesTest, ADieHasItsPipsAndRoundedCorners) {
  const DicePieces pieces(1.2f);
  // The chamfered cube's 44 triangles and 21 pips of 2
  EXPECT_EQ(pieces.die.triangleCount(), 44u + 21u * 2u);
  // Every triangle faces out (its winding's normal away from the middle),
  // and nothing reaches past the die's half-size
  const Scene::MeshData data = pieces.die.data();
  for (std::size_t i = 0; i + 2 < data.indices.size(); i += 3) {
    const Scene::Vector3 a = data.vertices[data.indices[i]].position,
                         b = data.vertices[data.indices[i + 1]].position,
                         c = data.vertices[data.indices[i + 2]].position;
    const Scene::Vector3 normal = (b - a).cross(c - a);
    const Scene::Vector3 middle = (a + b + c) * (1.0f / 3);
    EXPECT_GT(normal.dot(middle), 0) << "triangle " << i / 3;
    for (const Scene::Vector3 &p : {a, b, c}) {
      EXPECT_LE(std::abs(p.x), 0.61f);
      EXPECT_LE(std::abs(p.y), 0.61f);
      EXPECT_LE(std::abs(p.z), 0.61f);
    }
  }
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

// Round 89: where the dice show on the screen, for the roll's words: the
// glass's middle in the screen's middle, its right edge to the right,
// nothing behind the camera
TEST(DicePiecesTest, PointsOnTheScreen) {
  const Scene::Camera camera{.eye = {0, 9.5f, 9.5f}, .target = {0, 0, 1.2f}, .fieldOfView = 50};
  const Scene::Transform glass = glassFrame(camera, 400.0f / 240, 10, 6);
  // (off the screen if missing: the checks below then fail)
  constexpr std::array<float, 2> kMissing{-1000, -1000};
  const auto middle = onScreen(camera, 400, 240, glass.apply({0, 0, 0})).value_or(kMissing);
  EXPECT_NEAR(middle[0], 200, 0.5f);
  EXPECT_NEAR(middle[1], 120, 0.5f);
  const auto right = onScreen(camera, 400, 240, glass.apply({5, 0, 0})).value_or(kMissing);
  EXPECT_NEAR(right[0], 200 + 0.82f * 200, 2); // (the glass's floor: 82% of the width)
  const auto top = onScreen(camera, 400, 240, glass.apply({0, 0, -3})).value_or(kMissing);
  EXPECT_LT(top[1], 120);
  EXPECT_GT(top[1], 0);
  EXPECT_FALSE(onScreen(camera, 400, 240, camera.eye * 2 - camera.target).has_value());
}
