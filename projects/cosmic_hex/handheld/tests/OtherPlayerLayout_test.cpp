// Another player's panel laid out on each handheld's top screen in its own
// font, and on screens of other shapes: for every hand from none to far
// more than show, and every count of planets, no two parts overlap and
// all stay on the screen
#include <CosmicHex/Handheld/TopViews.hpp>

#include <ScratchPadd/Scene/Font.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace Scene = ScratchPadd::Scene;

namespace {

float silkscreenLine(float scale) { return Scene::Font::silkscreen().lineHeight(scale); }
float silkscreenWidth(std::string_view text, float scale) {
  return Scene::Font::silkscreen().width(text, scale);
}
float orbitronLine(float scale) { return Scene::Font::orbitron().lineHeight(scale); }
float orbitronWidth(std::string_view text, float scale) {
  return Scene::Font::orbitron().width(text, scale);
}

// A screen and the font its overlay draws in
struct Screen {
  std::string_view name;
  float width, height;
  TextMeasure measure;
};
const TextMeasure kSilkscreen{silkscreenLine, silkscreenWidth};
const TextMeasure kOrbitron{orbitronLine, orbitronWidth};
const Screen kDs{"Ds", 256, 192, kSilkscreen};
const Screen k3ds{"N3ds", 400, 240, kOrbitron};

// A player as the panel shows them: `cards` of each kind, `planets`
// planets, every piece past four (a count beside them), three Charters
PlayerView playerWith(int cards, int planets) {
  PlayerView view;
  view.player = 1;
  view.points = 10;
  view.resourceCards = view.actionCards = cards;
  view.badges = 2;
  view.ships = view.stations = view.colonies = 12;
  for (int i = 0; i < planets; ++i)
    view.planets.push_back({PlanetKind::GasGiant, 2 + i % 11, i % 3 == 0});
  view.charters = {Art::Card::CharterThreeOfAKind, Art::Card::CharterOccupier,
                   Art::Card::CharterDeepSpaceRelay};
  return view;
}
constexpr std::string_view kLongestName = "MMMMMMM"; // (PlayerNames::kShortLetters wide ones)

class OtherPlayerLayoutTest : public testing::TestWithParam<Screen> {
protected:
  OtherPlayerLayout layOut(const PlayerView &view) const {
    const Screen &s = GetParam();
    return otherPlayerLayout(view, kLongestName, s.width, s.height, s.measure);
  }
  float widthOf(std::string_view text, float scale) const {
    return GetParam().measure.width(text, scale);
  }
};

const std::vector<int> kHands{0, 1, 2, 5, 6, 7, 11, 12, 13, 40, 1000};
const std::vector<int> kPlanets{0, 1, 7, 8, 9, 16, 17, 40};

// Down the panel: the header, the cards, the pieces, the planets, the
// Charters, each under the last and all between the panel's lines. On a
// 3DS, two rows of cards pushed the planets off the bottom (they were
// placed at a DS's offsets scaled by the width, so the panel was a
// quarter too tall for the 3DS's screen)
TEST_P(OtherPlayerLayoutTest, ItsPartsStackDownWithoutOverlapping) {
  const Screen &screen = GetParam();
  for (const int cards : kHands)
    for (const int planets : kPlanets) {
      SCOPED_TRACE(std::to_string(cards) + " cards, " + std::to_string(planets) + " planets");
      const OtherPlayerLayout l = layOut(playerWith(cards, planets));
      EXPECT_GE(l.frameTop, 0);
      EXPECT_LE(l.frameBottom, screen.height - 1);
      EXPECT_GT(l.name.top, l.frameTop);
      EXPECT_LE(l.name.bottom, l.points.top);
      EXPECT_GT(l.cardLabels.top, l.frameTop);
      EXPECT_LE(l.cardLabels.bottom, l.cardsTop);
      EXPECT_LE(l.cardsBottom(), l.piecesMiddle - l.pieceHalf);
      EXPECT_LE(l.points.bottom, l.piecesMiddle - l.pieceHalf);
      EXPECT_LE(l.pieceCounts.bottom, l.pieceNames.top);
      EXPECT_LE(l.piecesMiddle + l.pieceHalf, l.pieceNames.top);
      EXPECT_LE(l.pieceNames.bottom, l.planetsLabel.top);
      if (planets > 0) {
        EXPECT_LE(l.pieceNames.bottom, l.planetsY - l.planetRadius);
      }
      EXPECT_LE(l.planetsBottom(), l.charters.top);
      EXPECT_LE(l.charters.bottom, l.frameBottom);
    }
}

// Across: the name left of the cards, the two columns of cards apart, a
// full row's fan inside its column, the labels and the Charters fitting,
// every planet's hexagon inside the panel and right of its label
TEST_P(OtherPlayerLayoutTest, ItsPartsFitAcross) {
  const Screen &screen = GetParam();
  for (const int cards : kHands)
    for (const int planets : kPlanets) {
      SCOPED_TRACE(std::to_string(cards) + " cards, " + std::to_string(planets) + " planets");
      const PlayerView view = playerWith(cards, planets);
      const OtherPlayerLayout l = layOut(view);
      EXPECT_LE(l.left + widthOf(kLongestName, l.nameScale), l.resourcesX);
      EXPECT_LE(l.left + widthOf("10 points", l.textScale), l.resourcesX);
      const int row = std::min(cards, OtherPlayerLayout::kCardsARow);
      const float fanned = static_cast<float>(std::max(0, row - 1)) * l.fanStep(row) + l.cardWidth;
      EXPECT_LE(l.resourcesX + fanned, l.actionsX);
      EXPECT_LE(l.actionsX + fanned, screen.width - l.margin);
      EXPECT_LE(widthOf(l.resourcesLabel, l.labelScale), l.columnWidth);
      EXPECT_LE(widthOf(l.actionsLabel, l.labelScale), l.columnWidth);
      EXPECT_NE(l.resourcesLabel.find(std::to_string(cards)), std::string::npos);
      for (const std::string &line : l.chartersLines)
        EXPECT_LE(l.left + widthOf(line, l.chartersScale), screen.width - l.margin) << line;
      EXPECT_FALSE(l.chartersLines.empty()); // (every Charter named, on one line or more)
      EXPECT_NE(l.chartersLines.back().find("Relay"), std::string::npos);
      EXPECT_LE(l.piecesX[3] + widthOf("colonies", l.textScale), screen.width - l.margin);
      const float halfWide = 0.8660254f * l.planetRadius;
      EXPECT_LE(l.left + widthOf("PLANETS", l.textScale), l.planetsX - halfWide);
      const float lastX = l.planetsX + static_cast<float>(l.planetsARow - 1) * l.planetStep;
      EXPECT_LE(lastX + halfWide, screen.width - l.margin);
      // (a planet's number inside its hexagon)
      EXPECT_LE(widthOf("12", l.textScale), 2 * halfWide);
    }
}

// The cards as big as fits: a DS's 36 wide with a row of them (the user's),
// and at least half that with two rows on every screen, the planets under
// them
TEST_P(OtherPlayerLayoutTest, TheCardsTakeTheRoomLeft) {
  const OtherPlayerLayout oneRow = layOut(playerWith(6, 16));
  const OtherPlayerLayout twoRows = layOut(playerWith(12, 16));
  EXPECT_EQ(oneRow.cardRows, 1);
  EXPECT_EQ(twoRows.cardRows, 2);
  EXPECT_GE(twoRows.cardWidth, 18 * twoRows.unit);
  EXPECT_LE(twoRows.cardWidth, oneRow.cardWidth);
  if (GetParam().name == "Ds") {
    EXPECT_FLOAT_EQ(oneRow.cardWidth, 36);
  }
}

// A short row fans out, closing up as it fills; a full one fits its column
TEST_P(OtherPlayerLayoutTest, ARowClosesUpAsItFills) {
  const OtherPlayerLayout l = layOut(playerWith(6, 0));
  float last = l.fanStep(2);
  EXPECT_GT(last, 0);
  for (int inRow = 3; inRow <= OtherPlayerLayout::kCardsARow; ++inRow) {
    EXPECT_LE(l.fanStep(inRow), last);
    last = l.fanStep(inRow);
  }
  EXPECT_LE(l.cardWidth + 5 * l.fanStep(6), l.columnWidth + 0.01f);
  EXPECT_FLOAT_EQ(l.fanStep(1), 0);
}

// More planets than two rows hold: the last place says how many more
TEST_P(OtherPlayerLayoutTest, PlanetsPastTwoRowsAreCounted) {
  const OtherPlayerLayout l = layOut(playerWith(0, 40));
  EXPECT_EQ(l.planetsShown, OtherPlayerLayout::kPlanetRows * l.planetsARow);
  EXPECT_EQ(layOut(playerWith(0, 3)).planetsShown, 3);
}

// Each revealed Charter by its own name: every Charter's short title is
// "Charter", so the line read "Charters: Charter Charter"
TEST(ChartersLineTest, NamesEachCharter) {
  PlayerView view;
  EXPECT_EQ(chartersLine(view), "");
  view.charters = {Art::Card::CharterOccupier};
  EXPECT_EQ(chartersLine(view), "Charters: Occupier");
  view.charters = {Art::Card::CharterThreeOfAKind, Art::Card::CharterOccupier,
                   Art::Card::CharterDeepSpaceRelay};
  EXPECT_EQ(chartersLine(view), "Charters: Three of a Kind, Occupier, Deep-space Relay");
}

INSTANTIATE_TEST_SUITE_P(OnEachHandheld, OtherPlayerLayoutTest,
                         testing::Values(kDs, k3ds,
                                         // (other shapes: a 3DS's bottom screen,
                                         // a wide phone's, a square one)
                                         Screen{"Wide320", 320, 240, kOrbitron},
                                         Screen{"Wide854", 854, 480, kOrbitron},
                                         Screen{"Square", 240, 240, kSilkscreen}),
                         [](const testing::TestParamInfo<Screen> &info) {
                           return std::string(info.param.name);
                         });

} // namespace
