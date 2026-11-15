// Texture recipes: reading one (and refusing what isn't one), and drawing it:
// exact pixels in the indexed format, smoothed edges in RGBA, both fill
// rules, later shapes over earlier ones, and the example drawing as the
// build turned it into a recipe, the same pixels everywhere.
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace TextureRecipeExamples {
extern const unsigned char kTile[];
extern const std::size_t kTileSize;
} // namespace TextureRecipeExamples

namespace TR = ScratchPadd::TextureRecipe;

namespace {

TR::Recipe readOrFail(std::string_view text) {
  auto recipe = TR::read(text);
  EXPECT_TRUE(recipe.has_value()) << (recipe ? "" : recipe.error());
  return recipe.value_or(TR::Recipe{});
}

// A 10 x 10 drawing: a red square covering its left half
constexpr std::string_view kHalf = "recipe 1\nsize 10 10\ncolour 0 255 0 0 255\nfill 0 nonzero\n"
                                   "contour 0 0 80 0 80 160 0 160\n";

std::uint32_t checksum(const TR::Image &image) {
  std::uint32_t hash = 2166136261u;
  for (std::uint8_t byte : image.pixels)
    hash = (hash ^ byte) * 16777619u;
  return hash;
}

} // namespace

TEST(TextureRecipeTest, Reads) {
  const TR::Recipe recipe = readOrFail(kHalf);
  EXPECT_EQ(recipe.width, 10);
  ASSERT_EQ(recipe.colours.size(), 1u);
  EXPECT_EQ(recipe.colours[0], (TR::Colour{255, 0, 0, 255}));
  ASSERT_EQ(recipe.fills.size(), 1u);
  EXPECT_EQ(recipe.fills[0].contours[0].size(), 4u);
}

TEST(TextureRecipeTest, RefusesWhatIsntARecipe) {
  EXPECT_FALSE(TR::read("hello\n").has_value());
  const auto noColour = TR::read("recipe 1\nsize 4 4\nfill 0 nonzero\n");
  ASSERT_FALSE(noColour.has_value());
  EXPECT_EQ(noColour.error().rfind("line 3", 0), 0u) << noColour.error();
  EXPECT_FALSE(TR::read("recipe 1\nsize 4 4\ncolour 0 1 2 3 4\nfill 0 nonzero\ncontour 0 0 1 1\n")
                   .has_value());
  EXPECT_FALSE(TR::read("recipe 1\nsize 4 4\ncolour 0 300 0 0 255\n").has_value());
}

// Indexed: each pixel's centre decides; 0 is transparent, 1 the first colour
TEST(TextureRecipeTest, IndexedPixelsAreExact) {
  const TR::Image image = TR::draw(readOrFail(kHalf), 8, 8, TR::Format::Indexed);
  ASSERT_EQ(image.palette.size(), 2u);
  EXPECT_EQ(image.palette[0].a, 0);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x)
      EXPECT_EQ(image.pixels[static_cast<std::size_t>(y * 8 + x)], x < 4 ? 1 : 0) << x << "," << y;
  }
}

// RGBA: a pixel the edge cuts in half is half covered
TEST(TextureRecipeTest, RgbaEdgesAreSmoothed) {
  const TR::Image image =
      TR::draw(readOrFail(kHalf), 5, 5, TR::Format::Rgba8); // the edge mid-pixel
  EXPECT_EQ(image.at(0, 2), (TR::Colour{255, 0, 0, 255}));
  EXPECT_EQ(image.at(2, 2).r, 255);
  EXPECT_EQ(image.at(2, 2).a, 127); // 8 of 16 samples
  EXPECT_EQ(image.at(4, 2).a, 0);
}

// Even-odd leaves the inner square a hole; nonzero, with both drawn the
// same way round, fills it
TEST(TextureRecipeTest, FillRules) {
  const std::string squares =
      "contour 0 0 160 0 160 160 0 160\ncontour 48 48 112 48 112 112 48 112\n";
  const std::string head = "recipe 1\nsize 10 10\ncolour 0 0 0 255 255\n";
  const TR::Image evenOdd =
      TR::draw(readOrFail(head + "fill 0 evenodd\n" + squares), 10, 10, TR::Format::Indexed);
  const TR::Image nonZero =
      TR::draw(readOrFail(head + "fill 0 nonzero\n" + squares), 10, 10, TR::Format::Indexed);
  EXPECT_EQ(evenOdd.pixels[5 * 10 + 5], 0);
  EXPECT_EQ(evenOdd.pixels[1 * 10 + 1], 1);
  EXPECT_EQ(nonZero.pixels[5 * 10 + 5], 1);
}

TEST(TextureRecipeTest, LaterShapesPaintOver) {
  const TR::Recipe recipe =
      readOrFail("recipe 1\nsize 10 10\ncolour 0 255 0 0 255\ncolour 1 0 255 0 255\n"
                 "fill 0 nonzero\ncontour 0 0 160 0 160 160 0 160\n"
                 "fill 1 nonzero\ncontour 80 0 160 0 160 160 80 160\n");
  const TR::Image image = TR::draw(recipe, 10, 10, TR::Format::Rgba8);
  EXPECT_EQ(image.at(2, 5), (TR::Colour{255, 0, 0, 255}));
  EXPECT_EQ(image.at(7, 5), (TR::Colour{0, 255, 0, 255}));
}

// The example drawing, as the build converted it (tools/svg_to_recipe.py):
// the background in the corner, the moon in the middle, the ring's band
// above the moon's centre and its hole showing the moon through it
TEST(TextureRecipeTest, TheExampleTile) {
  const TR::Recipe recipe =
      readOrFail(std::string_view(reinterpret_cast<const char *>(TextureRecipeExamples::kTile),
                                  TextureRecipeExamples::kTileSize));
  EXPECT_EQ(recipe.width, 64);
  EXPECT_LE(recipe.colours.size(), 15u); // fits a DS's 16-colour palette
  const TR::Image image = TR::draw(recipe, 64, 64, TR::Format::Indexed);
  const TR::Colour background{0x0b, 0x1a, 0x2e, 255}, moon{0xcf, 0xe8, 0xff, 255},
      ring{0x5f, 0xd3, 0xf3, 255};
  EXPECT_EQ(image.at(1, 1), background);
  EXPECT_EQ(image.at(32, 20), moon);
  EXPECT_EQ(image.at(32, 29), ring);
  EXPECT_EQ(image.at(32, 34), moon); // through the ring's hole
  EXPECT_EQ(image.at(32, 2).a, 255); // the outline's stroke at the top
  const TR::Image rgba = TR::draw(recipe, 64, 64, TR::Format::Rgba8);
  EXPECT_EQ(rgba.at(32, 20), moon);
  // The same pixels on every platform (integer arithmetic): pinned
  EXPECT_EQ(checksum(image), 3646522772u) << std::hex << checksum(image);
  EXPECT_EQ(checksum(rgba), 3316621745u) << std::hex << checksum(rgba);
}
