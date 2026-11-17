// Every face's recipe reads and draws: the hex in the space colour at the
// square's centre-left edge, the space colour's own, and nothing outside it
#include <CosmicHex/Art.hpp>

#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
namespace TR = ScratchPadd::TextureRecipe;

TEST(ArtTest, EveryFaceDraws) {
  for (int i = 0; i < Art::kFaces; ++i) {
    const auto face = static_cast<Art::Face>(i);
    const auto recipe = TR::read(Art::recipeOf(face));
    ASSERT_TRUE(recipe.has_value()) << "face " << i << ": " << recipe.error();
    const TR::Image image = TR::draw(*recipe, 64, 64, TR::Format::Rgba8);
    ASSERT_EQ(image.width, 64);
    // Inside the hex near its left edge: the space colour (24, 34, 55)
    const auto edge = image.at(8, 32);
    EXPECT_EQ(edge.r, 24) << "face " << i;
    EXPECT_EQ(edge.b, 55) << "face " << i;
    // The square's corner, outside the hex: see-through
    EXPECT_EQ(image.at(0, 0).a, 0) << "face " << i;
  }
}
