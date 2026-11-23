// Every card drawn one way, in layers (CardFace): one shape, 2:3, its
// layers' colours, a resource's art band down to its foot, the one shapes
// texture they're cut from, the mesh the top screen draws, the pixels the
// bottom screen draws, and a card turning over
#include <CosmicHex/Handheld/CardFace.hpp>

#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Handheld/RecipePixels.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {
constexpr Scene::Colour kCream{0xf4, 0xf1, 0xe8, 255}, kArtBand{0x18, 0x22, 0x37, 255};
bool sameColour(Scene::Colour a, Scene::Colour b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
} // namespace

// A card is 2:3 at every size (kHeightPerWidth, the one source of its
// shape: the card's units and the shapes texture's card keep to it), and
// the widest that fits a box keeps it
TEST(CardFaceTest, ACardIsTwoByThree) {
  EXPECT_FLOAT_EQ(CardFace::kHeightPerWidth, 1.5f);
  EXPECT_FLOAT_EQ(Art::kCardHeight / Art::kCardWidth, CardFace::kHeightPerWidth);
  EXPECT_FLOAT_EQ(CardFace::heightOf(44.0f), 66);
  EXPECT_EQ(CardFace::heightOf(48), 72);
  EXPECT_FLOAT_EQ(CardFace::widthToFit(100, 90), 60); // (as tall as fits)
  EXPECT_FLOAT_EQ(CardFace::widthToFit(40, 200), 40); // (as wide as fits)
  const CardFace::ShapeArea card = CardFace::areaOf(CardFace::Shape::Card);
  EXPECT_EQ(card.height, CardFace::heightOf(card.width));
}

// A resource card has no words: its art band goes down to its foot (no
// empty cream under it) and its icon sits in the card's middle. An action
// card keeps its art band under its title and its words in the cream
// under that, every line of them inside the card
TEST(CardFaceTest, AResourcesArtFillsTheCardUnderItsTitle) {
  for (int c = 0; c < Art::kCards; ++c) {
    const auto card = static_cast<Art::Card>(c);
    const Art::CardArea art = CardFace::artOf(card), icon = CardFace::iconOf(card);
    EXPECT_GE(icon.y, art.y) << c;
    EXPECT_LE(icon.y + icon.height, art.y + art.height) << c;
    if (CardFace::hasWords(card)) {
      EXPECT_GE(c, kResources);
      EXPECT_LE(art.y + art.height, Art::kCardWords.y) << c;
      continue;
    }
    EXPECT_LT(c, kResources);
    EXPECT_FLOAT_EQ(art.y + art.height, Art::kCardHeight - Art::kCardTitle.y) << c;
    EXPECT_FLOAT_EQ(icon.y + icon.height / 2, Art::kCardHeight / 2) << c;
    EXPECT_EQ(CardFace::layersOf(card)[3].shape, CardFace::Shape::TallArtBand) << c;
  }
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
  for (const auto shape : CardFace::kShapes) {
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
// its art band dark round the icon, the icon drawn; an action card's
// cream under its art band, a resource card's art band down to its foot
TEST(CardFaceTest, ACardsPixelsAreItsLayersAndIcon) {
  constexpr int kWidth = 60;
  const auto card = Art::Card::Enforcer;
  const CardFace::PixelArea icon = CardFace::iconPixelsOn(card, kWidth);
  const auto pixels =
      CardFace::pixelsOf(card, kWidth, CardFace::iconPixels(card, icon.width, icon.height));
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
  EXPECT_NE(pixels[static_cast<std::size_t>(icon.y + icon.height / 2) * kWidth +
                   static_cast<std::size_t>(icon.x + icon.width / 2)]
                .a,
            0); // (the icon drawn)
  const auto water = Art::Card::Water;
  const CardFace::PixelArea waterIcon = CardFace::iconPixelsOn(water, kWidth);
  const auto resource = CardFace::pixelsOf(
      water, kWidth, CardFace::iconPixels(water, waterIcon.width, waterIcon.height));
  const auto resourceAt = [&](float x, float y) {
    const float u = static_cast<float>(kWidth) / Art::kCardWidth;
    return resource[static_cast<std::size_t>(y * u) * kWidth + static_cast<std::size_t>(x * u)];
  };
  EXPECT_TRUE(sameColour(resourceAt(31.5f, 70), kArtBand)); // (no cream under its icon)
  EXPECT_TRUE(sameColour(resourceAt(31.5f, 88), kArtBand));
  EXPECT_GT(waterIcon.y, icon.y); // (its icon lower, in the card's middle)
}

// The icons a little larger than the layered cards first drew them (the
// user's): each drawing, on a card the same height, at least as big as it
// was on the 63 x 88 card before the layers (its height, 24 units, or the
// two wide ones' width, 47), and still whole: every drawn pixel inside the
// part shown, which is inside the art band, kIconMargin in from its edges
TEST(CardFaceTest, IconsAreLargerAndWholeInsideTheirArtBand) {
  constexpr float kOldCardHeight = 88, kOldHeight = 24, kOldWideWidth = 47;
  constexpr float kTexelsPerUnit = 4;
  for (int c = 0; c < Art::kCards; ++c) {
    const auto card = static_cast<Art::Card>(c);
    const Art::CardArea art = CardFace::artOf(card), shown = CardFace::iconOf(card);
    const Art::CardArea drawing = CardFace::iconDrawingOf(card);
    EXPECT_GE(shown.x, art.x + CardFace::kIconMargin - 1e-4f) << c;
    EXPECT_GE(shown.y, art.y + CardFace::kIconMargin - 1e-4f) << c;
    EXPECT_LE(shown.x + shown.width, art.x + art.width - CardFace::kIconMargin + 1e-4f) << c;
    EXPECT_LE(shown.y + shown.height, art.y + art.height - CardFace::kIconMargin + 1e-4f) << c;
    // (the whole drawing, and where its pixels are, in the card's units)
    const int w = static_cast<int>(drawing.width * kTexelsPerUnit),
              h = static_cast<int>(drawing.height * kTexelsPerUnit);
    const auto pixels = recipePixels(Art::cardIconRecipeOf(card), w, h);
    ASSERT_EQ(pixels.size(), static_cast<std::size_t>(w) * static_cast<std::size_t>(h)) << c;
    float left = drawing.x + drawing.width, right = drawing.x, top = drawing.y + drawing.height,
          bottom = drawing.y;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        if (pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                   static_cast<std::size_t>(x)]
                .a > 127) {
          left = std::min(left, drawing.x + static_cast<float>(x) / kTexelsPerUnit);
          right = std::max(right, drawing.x + static_cast<float>(x + 1) / kTexelsPerUnit);
          top = std::min(top, drawing.y + static_cast<float>(y) / kTexelsPerUnit);
          bottom = std::max(bottom, drawing.y + static_cast<float>(y + 1) / kTexelsPerUnit);
        }
    EXPECT_GE(left, shown.x) << Art::fileOf(card);
    EXPECT_LE(right, shown.x + shown.width) << Art::fileOf(card);
    EXPECT_GE(top, shown.y) << Art::fileOf(card);
    EXPECT_LE(bottom, shown.y + shown.height) << Art::fileOf(card);
    const bool wide =
        card == Art::Card::InterstellarCartel || card == Art::Card::CharterThreeOfAKind;
    const float now = (wide ? right - left : bottom - top) / Art::kCardHeight;
    const float before = (wide ? kOldWideWidth : kOldHeight) / kOldCardHeight;
    EXPECT_GE(now, before) << Art::fileOf(card);
  }
}

// The shown part's pixels: the drawing cut to it, its middle the
// drawing's (a resource's mark drawn there), `width` x `height`
TEST(CardFaceTest, AnIconsPixelsAreItsShownPart) {
  const CardFace::IconCut cut = CardFace::iconCutOf(Art::Card::Food);
  EXPECT_GT(cut.left, 0); // (its ground past the band cut off)
  EXPECT_LT(cut.right, 1);
  EXPECT_NEAR(cut.left + cut.right, 1, 1e-4); // (as much off each side)
  const auto pixels = CardFace::iconPixels(Art::Card::Food, 64, 56);
  ASSERT_EQ(pixels.size(), 64u * 56u);
  EXPECT_GT(pixels[28u * 64u + 32u].a, 200); // (its mark in the middle)
  EXPECT_EQ(pixels.front().a, 0);            // (its ground at the corner)
}

// A card turning over: its back, narrowing to its edge at halfway, then
// its face widening to the whole card
TEST(CardFaceTest, ACardTurnsOverFromItsBackToItsFace) {
  EXPECT_FALSE(CardFace::turnOf(0).face);
  EXPECT_FLOAT_EQ(CardFace::turnOf(0).across, 1);
  EXPECT_FALSE(CardFace::turnOf(0.25f).face);
  EXPECT_NEAR(CardFace::turnOf(0.25f).across, 0.7071f, 1e-3);
  EXPECT_NEAR(CardFace::turnOf(0.5f).across, 0, 1e-5);
  EXPECT_TRUE(CardFace::turnOf(0.5f).face);
  EXPECT_TRUE(CardFace::turnOf(1).face);
  EXPECT_FLOAT_EQ(CardFace::turnOf(1).across, 1);
  EXPECT_FLOAT_EQ(CardFace::turnOf(2).across, 1); // (past its end: face up)
}
