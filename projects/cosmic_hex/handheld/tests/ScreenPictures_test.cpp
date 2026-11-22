// Pictures on the top screen: a square placed in front of the
// camera covers exactly the pixels asked for, from the game's own cameras;
// pictures padded to the DS's texture sizes keep their pixels
#include <CosmicHex/Handheld/BoardView.hpp>
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

TEST(ScreenPicturesTest, ASquareCoversItsPixels) {
  for (const Scene::Camera &camera : {gameCamera(0), gameCamera(1.3f), topDownCamera()}) {
    const Scene::Transform at = onScreenAt(camera, 256, 192, 40, 150, 48, 30);
    const auto corner = [&](float u, float v) {
      return onScreen(camera, 256, 192, at.apply({u, v, 0}));
    };
    const auto bottomLeft = corner(0, 0), topRight = corner(1, 1);
    ASSERT_TRUE(bottomLeft && topRight);
    if (bottomLeft && topRight) {
      EXPECT_NEAR((*bottomLeft)[0], 40, 0.05);
      EXPECT_NEAR((*bottomLeft)[1], 180, 0.05);
      EXPECT_NEAR((*topRight)[0], 88, 0.05);
      EXPECT_NEAR((*topRight)[1], 150, 0.05);
    }
  }
}

TEST(ScreenPicturesTest, PaddedToPowersOfTwo) {
  std::vector<Scene::Colour> picture(48 * 39, Scene::Colour{200, 100, 50, 255});
  const PaddedPicture out = padded(picture, 48, 39);
  EXPECT_EQ(out.width, 64);
  EXPECT_EQ(out.height, 64);
  EXPECT_FLOAT_EQ(out.uMax, 48.0f / 64);
  EXPECT_EQ(out.pixels[static_cast<std::size_t>(38 * 64 + 47)].r, 200);
  EXPECT_EQ(out.pixels[static_cast<std::size_t>(38 * 64 + 48)].a, 0); // (the padding: see-through)
  EXPECT_EQ(pictureSquare().triangleCount(), 2u);
}

// A rigid piece between two poses: each at its end, square and unit
// between, and kept at its ends past them
TEST(ScreenPicturesTest, RigidBetweenTwoPoses) {
  const Scene::Transform from = Scene::Transform::identity();
  const Scene::Transform to =
      Scene::Transform::rotation({1, 0, 0}, -1.2f) * Scene::Transform::rotation({0, 1, 0}, 0.7f);
  const auto same = [](const Scene::Transform &a, const Scene::Transform &b) {
    for (const Scene::Vector3 axis : {Scene::Vector3{1, 0, 0}, {0, 1, 0}, {0, 0, 1}})
      EXPECT_NEAR((a.applyToDirection(axis) - b.applyToDirection(axis)).length(), 0, 1e-4);
  };
  same(rigidBetween(from, to, 0, {}), from);
  same(rigidBetween(from, to, 1, {}), to);
  same(rigidBetween(from, to, -0.2f, {}), from);
  same(rigidBetween(from, to, 1.1f, {}), to);
  const Scene::Transform half = rigidBetween(from, to, 0.5f, {1, 2, 3});
  EXPECT_NEAR((half.position() - Scene::Vector3{1, 2, 3}).length(), 0, 1e-5);
  const Scene::Vector3 x = half.applyToDirection({1, 0, 0}), y = half.applyToDirection({0, 1, 0}),
                       z = half.applyToDirection({0, 0, 1});
  EXPECT_NEAR(x.length(), 1, 1e-4);
  EXPECT_NEAR(y.length(), 1, 1e-4);
  EXPECT_NEAR(x.dot(y), 0, 1e-4);
  EXPECT_NEAR(y.dot(z), 0, 1e-4);
  EXPECT_NEAR((x.cross(y) - z).length(), 0, 1e-4); // (right-handed: not mirrored)
}

TEST(ScreenPicturesTest, TheCamerasAxes) {
  const auto [forward, right, up] = viewAxes(gameCamera(0));
  EXPECT_NEAR(forward.dot(right), 0, 1e-5);
  EXPECT_NEAR(forward.dot(up), 0, 1e-5);
  EXPECT_GT(up.y, 0); // (up the screen is up, more or less)
  EXPECT_GT(right.x, 0.99f);
}
