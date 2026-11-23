// The game as the screens get it (snapshotOf): the moves played since the
// last sent, held or not, your move and its moves only when the table waits
// for you, and the dice's seed, the same from the same seed and history
#include <CosmicHex/Handheld/TableSnapshot.hpp>

#include "TestTables.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {
constexpr std::uint64_t kSeed = 5;
} // namespace

// The moves since the first `playedFrom`, as made, and the last eight moves
// in words, the newest last
TEST(TableSnapshotTest, CarriesTheMovesPlayedSinceTheLastSent) {
  TableGame game(TestTables::yourTable(kSeed));
  ASSERT_TRUE(
      TestTables::playedOnUntil(game, [](const TableGame &g) { return g.played().size() >= 10; }));
  const TableSnapshot snapshot = snapshotOf(game, 4, false, kSeed);
  ASSERT_EQ(snapshot.played.size(), game.played().size() - 4);
  for (std::size_t i = 0; i < snapshot.played.size(); ++i) {
    EXPECT_EQ(snapshot.played[i].player, game.played()[4 + i].player);
    EXPECT_EQ(snapshot.played[i].action, game.played()[4 + i].action);
  }
  EXPECT_TRUE(snapshotOf(game, game.played().size() + 3, false, kSeed).played.empty());
  ASSERT_EQ(snapshot.lastMoves.size(), std::min<std::size_t>(game.history().size(), 8));
  EXPECT_EQ(snapshot.lastMoves.back(), game.history().back());
  EXPECT_EQ(snapshot.state, game.state());
  EXPECT_EQ(snapshot.firstRoll.first, game.firstRoll().first);
}

// Held as the table was when it sent it
TEST(TableSnapshotTest, SaysWhetherTheTableIsHeld) {
  const TableGame game(TestTables::yourTable(kSeed));
  EXPECT_TRUE(snapshotOf(game, 0, true, kSeed).paused);
  EXPECT_FALSE(snapshotOf(game, 0, false, kSeed).paused);
}

// Your move, with the moves you can make and each in words, only while the
// table waits for you
TEST(TableSnapshotTest, YourMovesOnlyOnYourMove) {
  TableGame game(TestTables::yourTable(kSeed));
  ASSERT_TRUE(
      TestTables::playedOnUntil(game, [](const TableGame &g) { return g.waitingForPerson(); }));
  const TableSnapshot yours = snapshotOf(game, 0, false, kSeed);
  EXPECT_TRUE(yours.yourMove);
  EXPECT_EQ(yours.moves, game.personMoves());
  ASSERT_EQ(yours.moveText.size(), yours.moves.size());
  EXPECT_TRUE(std::none_of(yours.moveText.begin(), yours.moveText.end(),
                           [](const std::string &words) { return words.empty(); }));
  ASSERT_TRUE(TestTables::playedOnUntil(
      game, [](const TableGame &g) { return !g.waitingForPerson() && !g.waitingForDice(); }));
  const TableSnapshot theirs = snapshotOf(game, 0, false, kSeed);
  EXPECT_FALSE(theirs.yourMove);
  EXPECT_TRUE(theirs.moves.empty());
  EXPECT_TRUE(theirs.moveText.empty());
}

// The dice: waited for with a seed from the table's seed and how far the
// game has got (the same game throws the same; each roll a new one); none
// while they don't wait
TEST(TableSnapshotTest, TheDiceSeedIsRepeatable) {
  TableGame game(TestTables::yourTable(kSeed)), again(TestTables::yourTable(kSeed));
  const auto dice = [](const TableGame &g) { return g.waitingForDice(); };
  ASSERT_TRUE(TestTables::playedOnUntil(game, dice));
  ASSERT_TRUE(TestTables::playedOnUntil(again, dice));
  const TableSnapshot first = snapshotOf(game, 0, false, kSeed);
  EXPECT_TRUE(first.waitingForDice);
  EXPECT_NE(first.diceSeed, 0u);
  EXPECT_EQ(snapshotOf(again, 0, false, kSeed).diceSeed, first.diceSeed);
  EXPECT_NE(snapshotOf(game, 0, false, kSeed + 1).diceSeed, first.diceSeed);
  ASSERT_TRUE(game.rollDice(2, 4));
  const TableSnapshot after = snapshotOf(game, 0, false, kSeed);
  if (!after.waitingForDice) {
    EXPECT_EQ(after.diceSeed, 0u);
  }
  ASSERT_TRUE(TestTables::playedOnUntil(game, dice));
  EXPECT_NE(snapshotOf(game, 0, false, kSeed).diceSeed, first.diceSeed); // (the next roll's own)
}
