// Whole games played through the app's own shows and touch panel, player 0
// by the scripted player (ScriptedGame), each show logged (ShowLog). Each
// test keeps a cause of shows cut short or skipped, found by these runs,
// from coming back (the README's "Scripted play")
#include <CosmicHex/Handheld/ScriptedGame.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace CosmicHex::Handheld;

namespace {

using Cut = ShowLog::Cut;

// (made where it stays: its hooks hold on to it)
std::unique_ptr<ScriptedGame> played(const ScriptedGameSettings &settings) {
  auto game = std::make_unique<ScriptedGame>(settings);
  game->run();
  return game;
}

// Each show of `kind` cut short or skipped without the player asking, in words
std::string unasked(const ScriptedGame &game, std::optional<ShowKind> kind = std::nullopt) {
  std::string found;
  for (const ShowLog::Entry &entry : game.log().entries())
    if ((!kind || entry.kind == *kind) && entry.cut() != Cut::None && !ShowLog::asked(entry.cut()))
      found += std::string(nameOf(entry.kind)) + " " + std::string(ShowLog::nameOf(entry.cut())) +
               " at " + std::to_string(entry.startedAt) + " (" + entry.note + ")\n";
  return found;
}

} // namespace

// The driver plays a whole game to its end through taps and buttons alone
TEST(ScriptedGameTest, PlaysAWholeGameThroughTheTouchPanel) {
  const auto game = played({.seed = 1});
  EXPECT_TRUE(game->table().over());
  EXPECT_GT(game->inputs(), 50);
  EXPECT_GT(game->log().shows(), 100u);
}

// A home's pay and a discovery came in one state at the start (the ship
// placed turned over an anomaly; the first turn's home die rolled): each
// was timed as it came, the discovery drawn over the die, and the die,
// spinning unseen, appeared half done when the discovery went (the user's
// "the home die just appears"). Shows now take the stage in turn
TEST(ScriptedGameTest, AHomesPayNeverPlaysUnderADiscovery) {
  for (const std::uint64_t seed : {2, 3, 9}) {
    const auto game = played({.seed = seed});
    EXPECT_EQ(unasked(*game, ShowKind::HomePay), "") << "seed " << seed;
    EXPECT_EQ(unasked(*game, ShowKind::Discovery), "") << "seed " << seed;
  }
}

// The dice were thrown with a discovery still to come (it waited for the
// banners), and it then covered them as they rolled: the dice now wait for
// the shows before them, and a show waits for dice that are out
TEST(ScriptedGameTest, TheDiceAreNeverThrownUnderAShow) {
  for (const std::uint64_t seed : {2, 3, 9}) {
    const auto game = played({.seed = seed});
    EXPECT_EQ(unasked(*game, ShowKind::Dice), "") << "seed " << seed;
  }
}

// A card played came while a discovery showed: it played under it, unseen
TEST(ScriptedGameTest, ACardPlayedWaitsForTheStage) {
  const auto game = played({.seed = 9});
  EXPECT_EQ(unasked(*game, ShowKind::CardPlay), "");
}

// The game's clock paused whenever a show held the table, so a banner
// froze half across and sat hidden under the show; and the table, held
// only once a show started, made its next move meanwhile, whose banner
// went under the show or cut the last one short. The table is now held
// from the moment a move brings something to show, banners follow each
// other on one band, and a banner waits for a show that has the stage
TEST(ScriptedGameTest, BannersAreSeenWhole) {
  for (const std::uint64_t seed : {1, 3, 4}) {
    const auto game = played({.seed = seed});
    EXPECT_EQ(unasked(*game, ShowKind::Banner), "") << "seed " << seed;
  }
}

// The table moved every 0.6 s while the pieces took 0.85 s to land; and
// the dealer's draws after your move (a planet a ship reached turned over)
// came as a state of their own: either cut the last move's flight or
// landing short. The table now waits for the board to settle, your move
// waits for the pieces to land, and the draws come with the move
TEST(ScriptedGameTest, ThePiecesLandBeforeTheNextMove) {
  for (const std::uint64_t seed : {1, 4, 5}) {
    const auto game = played({.seed = seed});
    EXPECT_EQ(unasked(*game, ShowKind::Pieces), "") << "seed " << seed;
  }
}

// B jumped the shows' clock past every show's end, so a home's pay
// waiting for the banner B skipped was skipped too, its die never seen.
// Shows still to come now start afresh after a skip
TEST(ScriptedGameTest, SkippingWhatAnimatesLeavesWhatsToCome) {
  const auto game = played({.seed = 1, .impatience = 10});
  EXPECT_EQ(game->log().count(Cut::SkippedQueued), 0u);
  EXPECT_EQ(unasked(*game), "");
}

// Your cards gained showed once a game: the hand's show never let itself
// start again
TEST(ScriptedGameTest, YourCardsGainedShowEveryTime) {
  const auto game = played({.seed = 1});
  std::size_t hands = 0;
  for (const ShowLog::Entry &entry : game->log().entries())
    hands += entry.kind == ShowKind::Hand && entry.seenSeconds > 0 ? 1 : 0;
  EXPECT_GT(hands, 10u);
  EXPECT_EQ(unasked(*game, ShowKind::Hand), "");
}

// Every show seen whole unless the player skipped it, at each pace and on
// a DS's slower frames, playing and watching
TEST(ScriptedGameTest, NoShowIsCutShortUnasked) {
  const ScriptedGameSettings settings[] = {
      {.seed = 6},
      {.seed = 7, .animations = Animations::Quick},
      {.seed = 8, .frameSeconds = 1.0 / 20},
      {.seed = 10, .person = false},
  };
  for (const ScriptedGameSettings &setting : settings) {
    const auto game = played(setting);
    EXPECT_TRUE(game->table().over()) << "seed " << setting.seed;
    EXPECT_EQ(unasked(*game), "") << "seed " << setting.seed;
  }
}
