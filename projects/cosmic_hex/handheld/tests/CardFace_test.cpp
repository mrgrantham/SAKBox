// Every card drawn one way, in layers (CardFace): 2:3, its layers' colours,
// the one shapes texture they're cut from, the mesh the top screen draws
// and the pixels the bottom screen draws
#include <CosmicHex/Handheld/CardFace.hpp>

#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <gtest/gtest.h>

#include <cstddef>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {
constexpr Scene::Colour kCream{0xf4, 0xf1, 0xe8, 255}, kArtBand{0x18, 0x22, 0x37, 255};
bool sameColour(Scene::Colour a, Scene::Colour b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
} // namespace

// A card is 2:3 at every size, and the widest that fits a box keeps it
TEST(CardFaceTest, ACardIsTwoByThree) {
  EXPECT_FLOAT_EQ(CardFace::heightOf(44.0f), 66);
  EXPECT_EQ(CardFace::heightOf(48), 72);
  EXPECT_FLOAT_EQ(CardFace::widthToFit(100, 90), 60); // (as tall as fits)
  EXPECT_FLOAT_EQ(CardFace::widthToFit(40, 200), 40); // (as wide as fits)
  EXPECT_LT(CardFace::topHeightOf(48), CardFace::heightOf(48));
}

// Its layers back to front: the edge, the cream body, its own band, the
// dark art band; the title's ink dark on every resource's pale band
TEST(CardFaceTest, EachCardsLayersAreItsColours) {
  for (int c = 0; c < Art::kCards; ++c) {
    const auto card = static_cast<Art::Card>(c);
    const auto layers = CardFace::layersOf(card);
    EXPECT_TRUE(sameColour(layers[1].colour, kCream)) << c;
    EXPECT_TRUE(sameColour(layers[2].colour, CardFace::bandOf(card))) << c;
    EXPECT_TRUE(sameColour(layers[3].colour, kArtBand)) << c;
    EXPECT_EQ(layers[2].shape, CardFace::Shape::TitleBand) << c;
  }
  for (int r = 0; r < kResources; ++r)
    EXPECT_EQ(CardFace::titleInkOf(static_cast<Art::Card>(r)), Palette::kInkOnLightBand) << r;
}

// The shapes texture: each rounded rectangle solid in its middle, clear at
// its corner and in the gutters round it, inside the texture, apart
TEST(CardFaceTest, TheShapesTextureHoldsEachRoundedRectangle) {
  const auto pixels = CardFace::shapesPixels();
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(CardFace::kShapesSize * CardFace::kShapesSize));
  const auto alpha = [&](int x, int y) {
    return pixels[static_cast<std::size_t>(y) * CardFace::kShapesSize + static_cast<std::size_t>(x)]
        .a;
  };
  for (const auto shape :
       {CardFace::Shape::Card, CardFace::Shape::TitleBand, CardFace::Shape::ArtBand}) {
    const CardFace::ShapeArea a = CardFace::areaOf(shape);
    EXPECT_GE(a.x, 1);
    EXPECT_GE(a.y, 1);
    EXPECT_LE(a.x + a.width, CardFace::kShapesSize - 1);
    EXPECT_LE(a.y + a.height, CardFace::kShapesSize - 1);
    EXPECT_EQ(alpha(a.x + a.width / 2, a.y + a.height / 2), 255);
    EXPECT_LT(alpha(a.x, a.y), 255);       // (its rounded corner)
    EXPECT_EQ(alpha(a.x - 1, a.y + 2), 0); // (the gutter)
  }
}

// The mesh: four quads, one a layer, over the unit square, each layer in
// front of the one before and the icon in front of them all
TEST(CardFaceTest, TheLayersMeshIsFourQuadsOverTheUnitSquare) {
  const auto mesh = CardFace::layersMesh(Art::Card::Fuel);
  EXPECT_EQ(mesh.triangleCount(), 2u * CardFace::kLayers);
  const auto data = mesh.data();
  for (const Scene::Vertex &v : data.vertices) {
    EXPECT_GE(v.position.x, 0);
    EXPECT_LE(v.position.x, 1);
    EXPECT_GE(v.position.y, 0);
    EXPECT_LE(v.position.y, 1);
    EXPECT_LT(v.position.z, CardFace::kIconLift);
  }
  EXPECT_LT(CardFace::kIconLift, CardFace::kCardStep);
}

// The bottom screen's card: 2:3, its corner clear, its band its colour,
// its art band dark round the icon, the cream under it, the icon drawn
TEST(CardFaceTest, ACardsPixelsAreItsLayersAndIcon) {
  constexpr int kWidth = 60;
  const auto card = Art::Card::Water;
  const CardFace::PixelArea icon = CardFace::iconPixelsOn(kWidth);
  const auto iconPixels = recipePixels(Art::cardIconRecipeOf(card), icon.width, icon.height);
  const auto pixels = CardFace::pixelsOf(card, kWidth, iconPixels);
  const int height = CardFace::heightOf(kWidth);
  ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kWidth * height));
  const auto at = [&](float x, float y) {
    const float u = static_cast<float>(kWidth) / Art::kCardWidth;
    return pixels[static_cast<std::size_t>(y * u) * kWidth + static_cast<std::size_t>(x * u)];
  };
  EXPECT_EQ(pixels.front().a, 0);
  EXPECT_TRUE(sameColour(at(6, 10), CardFace::bandOf(card)));
  EXPECT_TRUE(sameColour(at(4, 23), kArtBand));
  EXPECT_TRUE(sameColour(at(31.5f, 70), kCream));
  EXPECT_TRUE(
      sameColour(at(22.5f, 36), CardFace::bandOf(card))); // (the icon's disc, its band's colour)
}
