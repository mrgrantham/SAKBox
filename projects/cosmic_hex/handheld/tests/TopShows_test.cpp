// The top screen's shows on their own: when each starts, moves and ends,
// and what it asks of the app (hold the game, let it go, redraw the map)
#include <CosmicHex/Handheld/TopShows.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {
// A game, and the same with its first planet that isn't a home turned over
std::pair<State, State> aTurnOver(int planets = 1) {
  const State before = newGame(4, agreedRules(boardR3()));
  State after = before;
  int turned = 0;
  for (Planet &planet : after.planets) {
    if (planet.flipped || planet.homeOf >= 0 || turned == planets)
      continue;
    planet.flipped = true;
    planet.kind = PlanetKind::GasGiant;
    planet.number = 6;
    ++turned;
  }
  return {before, after};
}
} // namespace

// A tile: its close-up drawn first, the game held, the board's tile face
// down until it's back, the map fading it in, then the game let go
TEST(TopShowsTest, ADiscoveryHoldsTheGameUntilItsDone) {
  const auto [before, after] = aTurnOver();
  DiscoveryShow show;
  show.arrived(before, after, 1.0);
  ASSERT_NE(show.current(), nullptr);
  const Hex hex = show.current()->hex;
  EXPECT_FALSE(show.moving(0.5)); // (the ship's slide first)
  EXPECT_TRUE(show.stillToShow(hex, 0.5));
  EXPECT_FALSE(show.flying(hex, 0.5));
  int closeUps = 0;
  const auto closeUp = [&](Art::Face face) {
    ++closeUps;
    EXPECT_EQ(face, Art::Face::PlanetFuel);
  };
  const auto first = show.update(2.0, closeUp);
  EXPECT_EQ(closeUps, 1);
  EXPECT_TRUE(first.hold);
  EXPECT_TRUE(show.moving(2.1));
  EXPECT_TRUE(show.flying(hex, 2.1));
  EXPECT_FALSE(show.update(3.0, closeUp).hold); // (held once)
  EXPECT_EQ(closeUps, 1);
  // Back on the board, fading in below
  const double down = 2.0 + Discovery::kSeconds - Discovery::kSecondsFade;
  EXPECT_FALSE(show.flying(hex, down + 0.01));
  EXPECT_FALSE(show.stillToShow(hex, down + 0.01));
  EXPECT_TRUE(show.update(down + 0.1, closeUp).redrawMap);
  // Done: let go, nothing left, shown big (so the board doesn't flip it)
  const auto last = show.update(2.0 + Discovery::kSeconds + 0.01, closeUp);
  EXPECT_TRUE(last.letGo);
  EXPECT_FALSE(show.waiting());
  EXPECT_TRUE(show.shownBig(hex));
}

// Two in one move: one after the other; the second waits face down
TEST(TopShowsTest, TwoTilesOneAfterTheOther) {
  const auto [before, after] = aTurnOver(2);
  DiscoveryShow show;
  show.arrived(before, after, 0);
  ASSERT_EQ(show.waitingAfterCurrent().size(), 1u);
  show.update(0, nullptr);
  const auto step = show.update(Discovery::kSeconds + 0.01, nullptr);
  EXPECT_FALSE(step.letGo); // (the second's next)
  EXPECT_TRUE(show.waiting());
  EXPECT_TRUE(show.waitingAfterCurrent().empty());
}

// The cards dealt: their close-ups first, then thrown, then turned
TEST(TopShowsTest, DealtCardsThrownThenTurned) {
  DealtShow dealt;
  dealt.deal({Art::Card::Enforcer, Art::Card::SupplyDrop}, 10);
  EXPECT_TRUE(dealt.needsCloseUp());
  dealt.closeUpDrawn(11);
  EXPECT_FALSE(dealt.needsCloseUp());
  EXPECT_FLOAT_EQ(dealt.thrown(0, 11), 0);
  EXPECT_FLOAT_EQ(dealt.thrown(0, 11 + DealtShow::kSecondsThrow), 1);
  EXPECT_FLOAT_EQ(dealt.turned(0, 11 + DealtShow::kSecondsThrow), 0);
  EXPECT_LT(dealt.turned(1, 12.1), dealt.turned(0, 12.1)); // (one after the other)
  EXPECT_FLOAT_EQ(dealt.turned(1, 20), 1);
  dealt.deal({Art::Card::Enforcer, Art::Card::SupplyDrop}, 30); // (the same: no restart)
  EXPECT_FLOAT_EQ(dealt.turned(1, 20), 1);
  dealt.stop();
  EXPECT_FALSE(dealt.moving());
}

// A panel: down as it opens, up as it closes, then gone
TEST(TopShowsTest, APanelSlidesDownAndBackUp) {
  PanelShow panel(0.3f);
  EXPECT_FALSE(panel.showing(0));
  panel.open(2, 1);
  EXPECT_EQ(panel.which(), 2);
  EXPECT_FLOAT_EQ(panel.down(1), 0);
  EXPECT_FLOAT_EQ(panel.down(1.3), 1);
  panel.close(5);
  EXPECT_TRUE(panel.showing(5.1));
  EXPECT_FLOAT_EQ(panel.down(5.3), 0);
  EXPECT_FALSE(panel.showing(5.31));
}

// Who goes first: each round thrown after the last, then time to read
TEST(TopShowsTest, TheFirstRollsRoundsThenTheOrder) {
  FirstRoll roll;
  roll.first = 2;
  roll.rounds = {{5, 6, 6, 1}, {0, 2, 4, 0}};
  EXPECT_FLOAT_EQ(FirstRollShow::duration(roll),
                  2 * FirstRollShow::kSecondsRound + FirstRollShow::kSecondsRead);
  EXPECT_FLOAT_EQ(*FirstRollShow::tumble(0, 0), 0);
  EXPECT_FALSE(FirstRollShow::tumble(1, 1.0f).has_value());
  EXPECT_FLOAT_EQ(*FirstRollShow::tumble(1, FirstRollShow::kSecondsRound + 5), 1);
  EXPECT_FALSE(FirstRollShow::settled(roll, FirstRollShow::kSecondsRound));
  EXPECT_TRUE(
      FirstRollShow::settled(roll, FirstRollShow::kSecondsRound + FirstRollShow::kSecondsTumble));
  FirstRollShow show;
  show.start(100);
  EXPECT_FALSE(show.done(101, roll));
  EXPECT_TRUE(show.done(100 + FirstRollShow::duration(roll), roll));
}

// A home's pay: the die out, tumbling, settled; the card big, then away;
// the game held meanwhile and let go after
TEST(TopShowsTest, AHomePaysOnItsDie) {
  HomePayShow show;
  show.saw({{2, Action{Action::Kind::RollHomeDie, 3}}}, 10);
  ASSERT_NE(show.current(), nullptr);
  EXPECT_EQ(show.current()->player, 2);
  EXPECT_EQ(show.current()->resource, Resource::Water);
  EXPECT_TRUE(show.update(10).hold);
  const auto settled = show.at(HomePayShow::kDieSeconds);
  EXPECT_FLOAT_EQ(settled.settle, 1);
  EXPECT_FLOAT_EQ(settled.card, 0);
  EXPECT_FLOAT_EQ(show.at(HomePayShow::kSeconds).away, 1);
  EXPECT_TRUE(show.update(10 + HomePayShow::kSeconds).letGo);
  EXPECT_FALSE(show.moving(10 + HomePayShow::kSeconds));
}

// The star: held at the settled die until the choice is made, then its card
TEST(TopShowsTest, TheStarWaitsForTheChoice) {
  HomePayShow show;
  show.saw({{0, Action{Action::Kind::RollHomeDie, kResources}}}, 0);
  EXPECT_FALSE(show.current()->resource.has_value());
  show.update(0);
  show.update(5); // (long after the die settled: still waiting)
  const auto waiting = show.at(*show.seconds(5));
  EXPECT_TRUE(waiting.waitingForChoice);
  EXPECT_FLOAT_EQ(waiting.card, 0);
  show.saw({{0, Action{Action::Kind::ChooseHome, 1}}}, 6);
  EXPECT_EQ(show.current()->resource, Resource::Food);
  EXPECT_FALSE(show.at(*show.seconds(6)).waitingForChoice);
  EXPECT_TRUE(show.update(6 + HomePayShow::kSeconds).letGo);
}
