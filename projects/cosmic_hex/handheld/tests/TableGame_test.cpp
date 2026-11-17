// The game at the table: bots and the dealer play a whole game; a person's
// move waits for them, and only a legal one is taken
#include <CosmicHex/Handheld/TableGame.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(TableGameTest, FourBotsPlayAGame) {
  TableGame game({.botIterations = 10, .seed = 3});
  for (int moves = 0; !game.over() && moves < 20000; ++moves) {
    ASSERT_FALSE(game.waitingForPerson());
    ASSERT_TRUE(game.step().has_value());
  }
  EXPECT_TRUE(game.over());
  EXPECT_EQ(game.history().size(), game.log().actions.size());
  // The log replays to the same end (as the lab would)
  EXPECT_TRUE(ActionLog::replay(game.log()).ok());
}

TEST(TableGameTest, APersonsMoveWaitsForThem) {
  TableGame game(
      {.seats = {Seat::Person, Seat::Person, Seat::Person, Seat::Person}, .botIterations = 10});
  // The dealer goes on by itself until it's someone's move
  while (!game.waitingForPerson() && !game.over())
    ASSERT_TRUE(game.step().has_value());
  ASSERT_TRUE(game.waitingForPerson());
  EXPECT_FALSE(game.step().has_value());               // (theirs to make)
  EXPECT_FALSE(game.play({Action::Kind::CashBadges})); // (not legal now)
  const std::vector<Action> legal = Game::legalActions(game.state());
  ASSERT_FALSE(legal.empty());
  EXPECT_TRUE(game.play(legal.front()));
  EXPECT_FALSE(game.history().empty());
}

// A person playing a whole game against three bots, as the consoles' move
// list offers: whenever it's their move there's at least one, any of them
// is taken, and the game goes on to its end (no phase leaves a person stuck)
TEST(TableGameTest, APersonCanPlayAWholeGame) {
  for (std::uint64_t seed : {11u, 12u, 13u}) {
    TableGame game({.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot},
                    .botIterations = 8,
                    .seed = seed});
    SeededRandom::Generator choose(seed);
    int personMoves = 0;
    for (int moves = 0; !game.over() && moves < 40000; ++moves) {
      if (game.waitingForPerson()) {
        const std::vector<Action> legal = Game::legalActions(game.state());
        ASSERT_FALSE(legal.empty()) << "a person with no move, seed " << seed;
        // (mostly ending the turn early would stall a game: pick at random)
        ASSERT_TRUE(game.play(legal[choose.below(static_cast<std::uint32_t>(legal.size()))]));
        ++personMoves;
      } else {
        ASSERT_TRUE(game.step().has_value()) << "stuck, seed " << seed;
      }
    }
    EXPECT_TRUE(game.over()) << "seed " << seed;
    EXPECT_GT(personMoves, 10);
  }
}
