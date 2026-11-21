// What everyone can see of a player: their counts as the state has them,
// each planet their stations touch once, nothing hidden (which resources
// or cards they hold), and the others listed from the next player round
#include <CosmicHex/Handheld/OtherPlayers.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(OtherPlayersTest, AViewHasWhatEveryoneCanSee) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator random(31);
  for (int step = 0; step < 900 && state.winner < 0; ++step)
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, random)
                           : Game::legalActions(state).front());
  for (int p = 0; p < 4; ++p) {
    const PlayerView view = viewOf(state, p);
    const Player &player = state.player[static_cast<std::size_t>(p)];
    EXPECT_EQ(view.points, points(state, p));
    EXPECT_EQ(view.resourceCards,
              std::accumulate(player.resources.begin(), player.resources.end(), 0));
    EXPECT_EQ(view.actionCards, static_cast<int>(player.contracts.size()));
    int stations = 0, colonies = 0;
    for (const Station &s : state.stations)
      if (s.owner == p)
        ++(s.colony ? colonies : stations);
    EXPECT_EQ(view.stations, stations);
    EXPECT_EQ(view.colonies, colonies);
    EXPECT_GE(colonies, 1); // (each starts with a home colony)
    // Every planet with a number, each listed once at most
    for (const PlayerView::Planet &planet : view.planets)
      EXPECT_GT(planet.number, 0);
    EXPECT_LE(view.planets.size(), static_cast<std::size_t>(3 * (stations + colonies)));
  }
  EXPECT_EQ(othersOf(state, 0), (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(othersOf(state, 2), (std::vector<int>{3, 0, 1}));
}

// The badges shown are the ones still held, not all ever earned: every 3
// become a point at once, and that point is in the points shown
TEST(OtherPlayersTest, BadgesShownAreTheOnesHeld) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  Player &player = state.player[1];
  player.badgesEarned = 4;
  player.badgePoints = 1; // (3 of them already a point)
  player.badges = 1;
  const PlayerView view = viewOf(state, 1);
  EXPECT_EQ(view.badges, 1);
  EXPECT_EQ(view.points, points(state, 1));
}
