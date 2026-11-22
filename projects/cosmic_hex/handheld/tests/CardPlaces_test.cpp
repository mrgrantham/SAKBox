// Where the top screen's cards fly: every card to or from another player
// lands on that player's own row of the points bars (the user saw cards
// fly to an empty spot right of the bars), for 2, 3 and 4 players, on a
// DS's screen and a 3DS's, playing and watching; yours go to your hand
// below, the bank's to the ROUND hexagon
#include <CosmicHex/Handheld/CardPlaces.hpp>
#include <CosmicHex/Handheld/Hud.hpp>
#include <CosmicHex/Handheld/PointsBars.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {

struct Screen {
  float width, height;
};
constexpr Screen kDs{256, 192}, k3ds{400, 240};

State playersOf(int players) {
  State state;
  state.players = players;
  state.turn = 0;
  return state;
}

// The dashes PointsBars draws for `player`'s row (each player's dashes
// follow the outline's four sides, in order)
std::vector<PointsBars::Bar> dashesOf(const State &state, float width, int player) {
  const std::vector<PointsBars::Bar> bars = PointsBars::of(state, static_cast<int>(width));
  const auto target = static_cast<std::size_t>(state.settings.winPoints);
  const auto first = bars.begin() + 4 + static_cast<std::ptrdiff_t>(player * target);
  return {first, first + static_cast<std::ptrdiff_t>(target)};
}

} // namespace

TEST(CardPlacesTest, EveryFlightToAnotherPlayerEndsOnTheirRow) {
  for (const Screen screen : {kDs, k3ds})
    for (const int players : {2, 3, 4})
      for (const int you : {-1, 0}) { // (watching, playing)
        const State state = playersOf(players);
        for (int p = 0; p < players; ++p) {
          if (p == you)
            continue;
          const CardPlaces::Point to = CardPlaces::of(state, screen.width, screen.height, p, you);
          const std::vector<PointsBars::Bar> dashes = dashesOf(state, screen.width, p);
          const float left = dashes.front().x, right = dashes.back().x + dashes.back().width;
          const float top = dashes.front().y, bottom = top + dashes.front().height;
          EXPECT_GE(to.x, left) << screen.width << " wide, " << players << " players, " << p;
          EXPECT_LE(to.x, right) << screen.width << " wide, " << players << " players, " << p;
          EXPECT_GE(to.y, top) << screen.width << " wide, " << players << " players, " << p;
          EXPECT_LE(to.y, bottom) << screen.width << " wide, " << players << " players, " << p;
        }
      }
}

TEST(CardPlacesTest, YoursGoBelowTheScreenAndTheBanksToTheRoundHexagon) {
  for (const Screen screen : {kDs, k3ds}) {
    const State state = playersOf(4);
    EXPECT_GT(CardPlaces::of(state, screen.width, screen.height, 0, 0).y, screen.height);
    const CardPlaces::Point bank =
        CardPlaces::of(state, screen.width, screen.height, CardPlaces::kBank, 0);
    const float k = screen.width / 256;
    EXPECT_FLOAT_EQ(bank.x, kRoundHexagonX * k);
    EXPECT_FLOAT_EQ(bank.y, kRoundHexagonY * k);
  }
}

// What a player pays to the bank leaves from their home: the home planet,
// or the one exhausted that was theirs; nothing when they've none
TEST(CardPlacesTest, APlayersHomeOrFormerHome) {
  State state = playersOf(2);
  state.planets.resize(3);
  state.planets[0].hex = Hex{1, 0};
  state.planets[0].formerHomeOf = 0;
  state.planets[1].hex = Hex{2, 0};
  state.planets[1].homeOf = 1;
  state.planets[2].hex = Hex{3, 0};
  EXPECT_EQ(CardPlaces::homeOf(state, 0), std::optional(Hex{1, 0}));
  EXPECT_EQ(CardPlaces::homeOf(state, 1), std::optional(Hex{2, 0}));
  EXPECT_FALSE(CardPlaces::homeOf(state, 2).has_value());
}
