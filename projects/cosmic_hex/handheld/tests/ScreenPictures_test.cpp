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
