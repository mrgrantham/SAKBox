// Every face's recipe reads and draws: the hex in the space colour at the
// square's centre-left edge, the space colour's own, and nothing outside it
#include <CosmicHex/Art.hpp>

#include <ScratchPadd/Scene/Font.hpp>
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

// Every number chip but 7 draws: a round chip (its centre's top cream, its
// square's corner see-through); 7 and numbers off the dice have none
TEST(ArtTest, EveryNumberChipDraws) {
  for (int n = Art::kLowestChip; n <= Art::kHighestChip; ++n) {
    if (n == 7) {
      EXPECT_TRUE(Art::chipRecipeOf(n).empty());
      continue;
    }
    const auto recipe = TR::read(Art::chipRecipeOf(n));
    ASSERT_TRUE(recipe.has_value()) << "chip " << n << ": " << recipe.error();
    const TR::Image image = TR::draw(*recipe, 64, 64, TR::Format::Rgba8);
    const auto top = image.at(32, 8); // (above the digits: the chip)
    EXPECT_EQ(top.r, 0xf2) << "chip " << n;
    EXPECT_EQ(image.at(0, 0).a, 0) << "chip " << n;
  }
  EXPECT_TRUE(Art::chipRecipeOf(1).empty());
  EXPECT_TRUE(Art::chipRecipeOf(13).empty());
}

// Every icon draws: something in its middle, its square's corner
// see-through (the button's own colour shows round it)
TEST(ArtTest, EveryIconDraws) {
  for (int i = 0; i < Art::kIcons; ++i) {
    const auto recipe = TR::read(Art::iconRecipeOf(static_cast<Art::Icon>(i)));
    ASSERT_TRUE(recipe.has_value()) << "icon " << i << ": " << recipe.error();
    const TR::Image image = TR::draw(*recipe, 32, 32, TR::Format::Rgba8);
    EXPECT_EQ(image.at(0, 0).a, 0) << "icon " << i;
    int drawn = 0;
    for (int y = 8; y < 24; ++y)
      for (int x = 8; x < 24; ++x)
        drawn += image.at(x, y).a > 0 ? 1 : 0;
    EXPECT_GT(drawn, 20) << "icon " << i;
  }
}

// A card is 2:3 (the user's true card shape), its areas inside it, its
// icon on its art band
TEST(ArtTest, ACardIsTwoByThreeItsAreasInside) {
  EXPECT_FLOAT_EQ(Art::kCardHeight / Art::kCardWidth, 1.5f);
  for (const Art::CardArea &area :
       {Art::kCardTitle, Art::kCardArt, Art::kCardIcon, Art::kCardWords}) {
    EXPECT_GE(area.x, 0);
    EXPECT_GE(area.y, 0);
    EXPECT_LE(area.x + area.width, Art::kCardWidth);
    EXPECT_LE(area.y + area.height, Art::kCardHeight);
  }
  EXPECT_GE(Art::kCardIcon.x, Art::kCardArt.x);
  EXPECT_GE(Art::kCardIcon.y, Art::kCardArt.y);
  EXPECT_LE(Art::kCardIcon.x + Art::kCardIcon.width, Art::kCardArt.x + Art::kCardArt.width);
  EXPECT_LE(Art::kCardIcon.y + Art::kCardIcon.height, Art::kCardArt.y + Art::kCardArt.height);
}

// Every card's icon draws, the icon's shape (2:1): something in its
// middle, its corner see-through (the art band shows round it)
TEST(ArtTest, EveryCardIconDraws) {
  for (int i = 0; i < Art::kCards; ++i) {
    const auto recipe = TR::read(Art::cardIconRecipeOf(static_cast<Art::Card>(i)));
    ASSERT_TRUE(recipe.has_value()) << "card " << i << ": " << recipe.error();
    const TR::Image image = TR::draw(*recipe, 112, 56, TR::Format::Rgba8);
    EXPECT_EQ(image.at(0, 0).a, 0) << "card " << i;
    int drawn = 0;
    for (int y = 20; y < 36; ++y)
      for (int x = 40; x < 72; ++x)
        drawn += image.at(x, y).a > 0 ? 1 : 0;
    EXPECT_GT(drawn, 20) << "card " << i;
  }
}

// Every card's title band has its colour (cards.txt): the resources' as
// their drawings had them, never the body's cream
TEST(ArtTest, EveryCardHasItsBandsColour) {
  EXPECT_EQ(Art::bandOf(Art::Card::Minerals), (Art::Rgb{0xaa, 0x96, 0x82}));
  EXPECT_EQ(Art::bandOf(Art::Card::Food), (Art::Rgb{0x6e, 0xc8, 0x6e}));
  EXPECT_EQ(Art::bandOf(Art::Card::Fuel), (Art::Rgb{0xeb, 0x96, 0x50}));
  EXPECT_EQ(Art::bandOf(Art::Card::Water), (Art::Rgb{0xaa, 0xd7, 0xf5}));
  EXPECT_EQ(Art::bandOf(Art::Card::Crystals), (Art::Rgb{0xb9, 0x82, 0xe6}));
  for (int i = 0; i < Art::kCards; ++i) {
    const Art::Rgb band = Art::bandOf(static_cast<Art::Card>(i));
    EXPECT_NE(band, (Art::Rgb{0, 0, 0})) << "card " << i;
    EXPECT_NE(band, Art::kCardBody) << "card " << i;
  }
}

// Every card's words (cards/cards.txt) are there and fit, in each console's
// font (the DS's Silkscreen, the 3DS's Orbitron): the short title on a
// small card (46 of a DS's pixels; on the 3DS, as small as its 320-wide
// bottom screen draws it, 1.25 times as wide: Orbitron's letters spaced
// apart, Font::tracking, need it), the words on the cards screen's big card
TEST(ArtTest, EveryCardsWordsFit) {
  namespace Scene = ScratchPadd::Scene;
  constexpr float kBigCard = 160, kMargin = 4;
  const float wordsWidth = Art::kCardWords.width * kBigCard / Art::kCardHeight - kMargin;
  constexpr float kSmallCardTitle = 46;
  for (const Scene::Font *font : {&Scene::Font::silkscreen(), &Scene::Font::orbitron()}) {
    const float titleRoom =
        font == &Scene::Font::silkscreen() ? kSmallCardTitle : kSmallCardTitle * 320 / 256;
    const auto width = [font](std::string_view text) { return font->width(text); };
    for (int i = 0; i < Art::kCards; ++i) {
      const auto card = static_cast<Art::Card>(i);
      const Art::CardWords words = Art::wordsOf(card);
      EXPECT_FALSE(words.title.empty()) << Art::fileOf(card);
      EXPECT_FALSE(words.words.empty()) << Art::fileOf(card);
      // (its letters' own pixels: Silkscreen's advance counts a clear
      // column each side)
      EXPECT_LE(font->ink(words.shortTitle).width, titleRoom) << Art::fileOf(card);
      EXPECT_LE(Scene::wrapText(words.words, wordsWidth, width).size(), 7u) << Art::fileOf(card);
    }
  }
}

// Both card backs draw, 2:3 as the faces: dark down to their foot, their
// rounded corners see-through
TEST(ArtTest, TheCardBacksDraw) {
  for (Art::CardBack design : {Art::CardBack::Rings, Art::CardBack::Planets}) {
    const auto recipe = TR::read(Art::cardBackRecipe(design));
    ASSERT_TRUE(recipe.has_value()) << recipe.error();
    const TR::Image image = TR::draw(*recipe, 64, 96, TR::Format::Rgba8);
    EXPECT_EQ(image.at(0, 0).a, 0);
    EXPECT_EQ(image.at(0, 95).a, 0);
    EXPECT_EQ(image.at(20, 92).g, 0x1c); // (the dark ground, inside the edge, clear of stars)
  }
}
