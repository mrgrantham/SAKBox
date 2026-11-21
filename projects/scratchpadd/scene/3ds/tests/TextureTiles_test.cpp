#include <ScratchPadd/Scene/3DS/TextureTiles.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace ScratchPadd::Scene {
namespace {

// The GPU reads a texture's first pixel in memory at its bottom left
// (texture coordinates 0, 0): an image's bottom-left pixel goes there
TEST(TextureTilesTest, TheImagesBottomLeftPixelComesFirst) {
  EXPECT_EQ(tiledIndex(0, 15, 16, 16), 0u);
  EXPECT_EQ(tiledIndex(0, 7, 32, 8), 0u);
}

// So its top row is in the GPU's last row of tiles: v 0 (an image's top,
// as Scene has it) is t 1 (the GPU's top), the right way up
TEST(TextureTilesTest, TheImagesTopRowIsInTheLastRowOfTiles) {
  constexpr int kWidth = 16, kHeight = 32;
  const std::size_t lastRowOfTiles = (kHeight / 8 - 1) * (kWidth / 8);
  for (int x = 0; x < kWidth; ++x)
    EXPECT_EQ(tiledIndex(x, 0, kWidth, kHeight) / 64, lastRowOfTiles + x / 8) << x;
}

// Inside a tile, Morton order from its bottom left: right is 1, up is 2,
// up and right is 3, then the next 2 x 2 block along
TEST(TextureTilesTest, InsideATileItsPixelsAreInMortonOrderFromTheBottom) {
  EXPECT_EQ(tiledIndex(1, 7, 8, 8), 1u);
  EXPECT_EQ(tiledIndex(0, 6, 8, 8), 2u);
  EXPECT_EQ(tiledIndex(1, 6, 8, 8), 3u);
  EXPECT_EQ(tiledIndex(2, 7, 8, 8), 4u);
  EXPECT_EQ(tiledIndex(7, 0, 8, 8), 63u); // (the top right, last)
}

// Every pixel has a place of its own, inside the texture
TEST(TextureTilesTest, EveryPixelHasItsOwnPlace) {
  for (const auto [width, height] : {std::pair{8, 8}, std::pair{32, 16}, std::pair{16, 64}}) {
    std::vector<int> used(static_cast<std::size_t>(width) * height, 0);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const std::size_t at = tiledIndex(x, y, width, height);
        ASSERT_LT(at, used.size());
        ++used[at];
      }
    for (const int count : used)
      EXPECT_EQ(count, 1);
  }
}

} // namespace
} // namespace ScratchPadd::Scene
