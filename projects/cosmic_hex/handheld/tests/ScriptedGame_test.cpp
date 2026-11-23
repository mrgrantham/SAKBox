// Short whole games played through the app's own shows and touch panel,
// player 0 by the scripted player (ScriptedGame), each show logged
// (ShowLog): many seeds a run, each game under the agreed rules but for a
// home exhausted at 4 points and the win at 8 (ScriptedRuns' shortGame).
// Each test keeps a cause of shows cut short or skipped, found by these
// runs, from coming back (the README's "Scripted play"). The other paces
// are ScriptedGamePaces_test's, the full-length games
// ScriptedGameLong_test's (run by hand)
#include "ScriptedRuns.hpp"

#include <CosmicHex/Rules.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace CosmicHex::Handheld;
using namespace CosmicHex::Handheld::Testing;

namespace {

using Cut = ShowLog::Cut;

// The games: player 0 playing, at the Full pace, played once for all the
// tests
class ShortGamesTest : public testing::Test {
protected:
  static constexpr std::uint64_t kFirstSeed = 1;
  static constexpr int kSeeds = 32;

  static void SetUpTestSuite() { runs_ = playAll(shortGames(kFirstSeed, kSeeds)); }
  static void TearDownTestSuite() { runs_.clear(); }

  // No show of `kind` cut short or skipped unasked, in any game
  static void expectNoneUnasked(ShowKind kind) {
    for (const PlayedGame &run : runs_)
      EXPECT_EQ(unasked(*run.game, kind), "") << nameOf(run.settings);
  }

  static std::vector<PlayedGame> runs_;
};

std::vector<PlayedGame> ShortGamesTest::runs_;

} // namespace

// Every game ends as a game does, by the rules' own win at the short
// games' points, through taps and buttons alone
TEST_F(ShortGamesTest, EachGameEndsByTheWin) {
  for (const PlayedGame &run : runs_) {
    EXPECT_EQ(endedByTheWin(*run.game), "") << nameOf(run.settings);
    EXPECT_EQ(run.game->table().state().settings.winPoints, kShortWinPoints);
    EXPECT_GT(run.game->inputs(), 20) << nameOf(run.settings);
  }
}

// The short games' other setting is played: homes are exhausted on the way
TEST_F(ShortGamesTest, HomesAreExhaustedOnTheWay) {
  int exhausted = 0;
  for (const PlayedGame &run : runs_)
    for (const CosmicHex::Planet &planet : run.game->table().state().planets)
      exhausted += planet.formerHomeOf >= 0 ? 1 : 0;
  EXPECT_GT(exhausted, kSeeds);
}

// A home's pay and a discovery came in one state at the start (the ship
// placed turned over an anomaly; the first turn's home die rolled): each
// was timed as it came, the discovery drawn over the die, and the die,
// spinning unseen, appeared half done when the discovery went (the user's
// "the home die just appears"). Shows now take the stage in turn
TEST_F(ShortGamesTest, AHomesPayNeverPlaysUnderADiscovery) {
  expectNoneUnasked(ShowKind::HomePay);
  expectNoneUnasked(ShowKind::Discovery);
}

// The dice were thrown with a discovery still to come (it waited for the
// banners), and it then covered them as they rolled: the dice now wait for
// the shows before them, and a show waits for dice that are out
TEST_F(ShortGamesTest, TheDiceAreNeverThrownUnderAShow) { expectNoneUnasked(ShowKind::Dice); }

// A card played came while a discovery showed: it played under it, unseen
TEST_F(ShortGamesTest, ACardPlayedWaitsForTheStage) { expectNoneUnasked(ShowKind::CardPlay); }

// The game's clock paused whenever a show held the table, so a banner
// froze half across and sat hidden under the show; and the table, held
// only once a show started, made its next move meanwhile, whose banner
// went under the show or cut the last one short. The table is now held
// from the moment a move brings something to show, banners follow each
// other on one band, and a banner waits for a show that has the stage
TEST_F(ShortGamesTest, BannersAreSeenWhole) { expectNoneUnasked(ShowKind::Banner); }

// The table moved every 0.6 s while the pieces took 0.85 s to land; and
// the dealer's draws after your move (a planet a ship reached turned over)
// came as a state of their own: either cut the last move's flight or
// landing short. The table now waits for the board to settle, your move
// waits for the pieces to land, and the draws come with the move
TEST_F(ShortGamesTest, ThePiecesLandBeforeTheNextMove) { expectNoneUnasked(ShowKind::Pieces); }

// Your cards gained showed once a game: the hand's show never let itself
// start again
TEST_F(ShortGamesTest, YourCardsGainedShowEveryTime) {
  for (const PlayedGame &run : runs_) {
    std::size_t hands = 0;
    for (const ShowLog::Entry &entry : run.game->log().entries())
      hands += entry.kind == ShowKind::Hand && entry.seenSeconds > 0 ? 1 : 0;
    EXPECT_GT(hands, 3u) << nameOf(run.settings);
  }
  expectNoneUnasked(ShowKind::Hand);
}

// Every show seen whole unless the player skipped it
TEST_F(ShortGamesTest, NoShowIsCutShortUnasked) {
  for (const PlayedGame &run : runs_)
    EXPECT_EQ(unasked(*run.game), "") << nameOf(run.settings);
}

// Seeds that still have a show cut short unasked (found by
// scripted_play's sweeps of short games: README, "Scripted play"): the
// player's own choices meeting a show still to come. Run them with
// --gtest_also_run_disabled_tests; once they pass, take DISABLED_ off
//   - the star's choice: the home's die, out before a tile turned over
//     earlier had shown, waits for the choice, and the tile takes the stage
//     over it (seeds 34 and 50; the full-length game's seed 50 too)
//   - the choice's banner then starts under that tile (seeds 34, 50, 101)
//   - salvage's second pick: its banner starts while the salvage show
//     pauses between cards, which then flies over it (seeds 104, 112)
//   - at the Quick pace, the star's choice (seeds 65, 67, 155), and a
//     turn's banner under a tile turned over (seed 149)
TEST(ShortGamesKnownTest, DISABLED_ShowsCutShortThatAreStillToFix) {
  std::vector<ScriptedGameSettings> games = shortGames(34, 1);
  for (const std::uint64_t seed : {50, 101, 104, 112})
    games.push_back(shortGames(seed, 1).front());
  for (const std::uint64_t seed : {65, 67, 149, 155})
    games.push_back(shortGames(seed, 1, {.animations = Animations::Quick}).front());
  for (const PlayedGame &run : playAll(games))
    EXPECT_EQ(unasked(*run.game), "") << nameOf(run.settings);
}

// A seed plays the same game every time (a failing seed reproduces): the
// table's moves, the player's inputs and every show's timing
TEST_F(ShortGamesTest, ASeedPlaysTheSameGameAgain) {
  const PlayedGame &first = runs_.front();
  const auto again = played(first.settings);
  EXPECT_EQ(again->table().history(), first.game->table().history());
  EXPECT_EQ(again->inputs(), first.game->inputs());
  EXPECT_EQ(again->seconds(), first.game->seconds());
  ASSERT_EQ(again->log().entries().size(), first.game->log().entries().size());
  for (std::size_t i = 0; i < again->log().entries().size(); ++i) {
    const ShowLog::Entry &a = again->log().entries()[i], &b = first.game->log().entries()[i];
    EXPECT_EQ(a.kind, b.kind) << "show " << i;
    EXPECT_EQ(a.startedAt, b.startedAt) << "show " << i;
    EXPECT_EQ(a.seenSeconds, b.seenSeconds) << "show " << i;
    EXPECT_EQ(a.cut(), b.cut()) << "show " << i;
  }
}
