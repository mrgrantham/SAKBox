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

// Every card draws, as the areas say: its title band in a colour of its
// own, not the card's cream; the art's dark panel; the cream where its words
// go; and its rounded corner see-through
TEST(ArtTest, EveryCardDrawsInItsAreas) {
  const auto at = [](const TR::Image &image, float x, float y) {
    return image.at(static_cast<int>(x / Art::kCardWidth * static_cast<float>(image.width)),
                    static_cast<int>(y / Art::kCardHeight * static_cast<float>(image.height)));
  };
  for (int i = 0; i < Art::kCards; ++i) {
    const auto recipe = TR::read(Art::cardRecipeOf(static_cast<Art::Card>(i)));
    ASSERT_TRUE(recipe.has_value()) << "card " << i << ": " << recipe.error();
    const TR::Image image = TR::draw(*recipe, 126, 176, TR::Format::Rgba8);
    const auto band =
        at(image, Art::kCardTitle.x + 2, Art::kCardTitle.y + Art::kCardTitle.height / 2);
    EXPECT_NE(band.r, 0xf4) << "card " << i;
    const auto art = at(image, Art::kCardArt.x + 2, Art::kCardArt.y + 2);
    EXPECT_EQ(art.r, 0x18) << "card " << i;
    const auto words = at(image, Art::kCardWords.x + 1, Art::kCardWords.y + 1);
    EXPECT_EQ(words.r, 0xf4) << "card " << i;
    EXPECT_EQ(image.at(0, 0).a, 0) << "card " << i;
  }
}

// Every card's words (cards/cards.txt) are there and fit, in each console's
// font (the DS's Silkscreen, the 3DS's Orbitron): the short title on a
// small card (46 pixels), the words on the cards screen's big card
TEST(ArtTest, EveryCardsWordsFit) {
  namespace Scene = ScratchPadd::Scene;
  constexpr float kBigCard = 160, kMargin = 4;
  const float wordsWidth = Art::kCardWords.width * kBigCard / Art::kCardHeight - kMargin;
  for (const Scene::Font *font : {&Scene::Font::silkscreen(), &Scene::Font::orbitron()}) {
    const auto width = [font](std::string_view text) { return font->width(text); };
    for (int i = 0; i < Art::kCards; ++i) {
      const auto card = static_cast<Art::Card>(i);
      const Art::CardWords words = Art::wordsOf(card);
      EXPECT_FALSE(words.title.empty()) << Art::fileOf(card);
      EXPECT_FALSE(words.words.empty()) << Art::fileOf(card);
      // (its letters' own pixels: Silkscreen's advance counts a clear
      // column each side)
      EXPECT_LE(font->ink(words.shortTitle).width, 46) << Art::fileOf(card);
      EXPECT_LE(Scene::wrapText(words.words, wordsWidth, width).size(), 7u) << Art::fileOf(card);
    }
  }
}

// Both card backs draw: dark, their rounded corners see-through
TEST(ArtTest, TheCardBacksDraw) {
  for (Art::CardBack design : {Art::CardBack::Rings, Art::CardBack::Planets}) {
    const auto recipe = TR::read(Art::cardBackRecipe(design));
    ASSERT_TRUE(recipe.has_value()) << recipe.error();
    const TR::Image image = TR::draw(*recipe, 63, 88, TR::Format::Rgba8);
    EXPECT_EQ(image.at(0, 0).a, 0);
    EXPECT_EQ(image.at(20, 80).g, 0x1c); // (the dark ground, inside the edge, clear of stars)
  }
}
