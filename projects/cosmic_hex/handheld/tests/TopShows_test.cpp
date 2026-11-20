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
  EXPECT_LT(panel.down(1.25), 1);
  EXPECT_FALSE(panel.inPlace(1.25));
  EXPECT_FLOAT_EQ(panel.down(1.3), 1);
  EXPECT_TRUE(panel.inPlace(1.31));
  panel.close(5);
  EXPECT_FALSE(panel.inPlace(5.1));
  EXPECT_TRUE(panel.showing(5.1));
  EXPECT_FLOAT_EQ(panel.down(5.3), 0);
  EXPECT_FALSE(panel.showing(5.31));
}

// Who goes first: each player's die thrown in turn, landing on their
// number; the ties again; A hurries; then time to read the order
TEST(TopShowsTest, TheFirstRollThrowsInTurn) {
  FirstRoll roll;
  roll.first = 2;
  roll.rounds = {{5, 6, 6, 1}, {0, 2, 4, 0}};
  FirstRollShow show;
  show.start(roll, 9, 1.0 / 30, 0);
  ASSERT_TRUE(show.on());
  double now = 0;
  for (; now < 60 && !show.allLanded(); now += 1.0 / 30)
    show.update(now);
  ASSERT_TRUE(show.allLanded());
  EXPECT_EQ(show.shown(0, 1), 6);
  EXPECT_EQ(show.shown(1, 2), 4);
  EXPECT_EQ(show.shown(1, 0), 0); // (not in the ties' round)
  EXPECT_FALSE(show.done(now));
  EXPECT_TRUE(show.done(now + FirstRollShow::kSecondsRead));
  // Impatient: A through it all, much sooner
  FirstRollShow quick;
  quick.start(roll, 9, 1.0 / 30, 0);
  double t = 0;
  for (int presses = 0; presses < 20 && !quick.done(t); ++presses) {
    quick.hurry(t);
    quick.update(t += 0.05);
  }
  EXPECT_TRUE(quick.done(t));
  EXPECT_LT(t, 2.0);
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
  EXPECT_TRUE(show.update(0).hold);
  EXPECT_TRUE(show.update(5).letGo); // (waiting for the choice: the game goes on, to make it)
  const auto waiting = show.at(*show.seconds(5));
  EXPECT_TRUE(waiting.waitingForChoice);
  EXPECT_FLOAT_EQ(waiting.card, 0);
  show.saw({{0, Action{Action::Kind::ChooseHome, 1}}}, 6);
  EXPECT_EQ(show.current()->resource, Resource::Food);
  EXPECT_FALSE(show.at(*show.seconds(6)).waitingForChoice);
  EXPECT_TRUE(show.update(6).hold); // (held again for its card)
  EXPECT_TRUE(show.update(6 + HomePayShow::kSeconds).letGo);
}

// Show resources: each card in a little after the last; off the same way,
// then gone
TEST(TopShowsTest, ShowResourcesCardsInAndOut) {
  CardsShow cards;
  EXPECT_FALSE(cards.showing(0));
  cards.open(1);
  EXPECT_FLOAT_EQ(cards.arrived(0, 1 + CardsShow::kSecondsCard), 1);
  EXPECT_LT(cards.arrived(3, 1 + CardsShow::kSecondsCard), 1); // (later ones still coming)
  cards.close(5);
  EXPECT_TRUE(cards.showing(5.1));
  EXPECT_FLOAT_EQ(cards.arrived(0, 5 + CardsShow::kSecondsCard), 0);
  EXPECT_FALSE(cards.showing(5 + CardsShow::kSecondsCard +
                             CardsShow::kStagger * CardsShow::kMostCards + 0.01));
}

// What a roll pays: a card for each resource, from its planet, one after
// another; the game held while they fly
TEST(TopShowsTest, ARollsPayFliesCardByCard) {
  State state = newGame(4, agreedRules(boardR3()));
  // (a planet turned over with a 6, a station of player 1 on its corner)
  Planet *paying = nullptr;
  for (Planet &planet : state.planets)
    if (!planet.flipped && planet.homeOf < 0) {
      planet.flipped = true;
      planet.kind = PlanetKind::GasGiant;
      planet.number = 6;
      paying = &planet;
      break;
    }
  ASSERT_NE(paying, nullptr);
  state.stations.push_back({.at = {paying->hex, 0}, .owner = 1});
  RollPayShow show;
  show.saw({{0, Action{Action::Kind::Roll, 2, 4}}}, state);
  ASSERT_FALSE(show.cards().empty());
  EXPECT_TRUE(show.waiting()); // (not before the dice have gone)
  EXPECT_FALSE(show.update(20).hold);
  show.start(10);
  EXPECT_EQ(show.cards().front().player, 1);
  EXPECT_EQ(show.cards().front().resource, Resource::Fuel);
  EXPECT_FALSE(show.at(0, 9.9).has_value());
  EXPECT_TRUE(show.update(10).hold);
  EXPECT_FLOAT_EQ(show.at(0, 10 + RollPayShow::kSecondsPop)->pop, 1);
  EXPECT_TRUE(show.moving(10.1));
  EXPECT_TRUE(show.update(10 + show.seconds()).letGo);
  EXPECT_TRUE(show.cards().empty());
  RollPayShow seven;
  seven.saw({{0, Action{Action::Kind::Roll, 3, 4}}}, state); // (a 7 pays nothing)
  EXPECT_TRUE(seven.cards().empty());
}

// A steal: after the blockade names who it's from, the card flies from
// them to the player whose turn it is; the pirate hideout's, from each in
// the order the game has them
TEST(TopShowsTest, AStealFliesFromTheVictim) {
  State before;
  before.turn = 2;
  RollPayShow show;
  show.saw({{2, Action{Action::Kind::MoveBlockade, 3, 1}}, {-1, Action{Action::Kind::Steal, 4}}},
           before);
  ASSERT_EQ(show.cards().size(), 1u);
  EXPECT_EQ(show.cards()[0].from, 1);
  EXPECT_EQ(show.cards()[0].player, 2);
  EXPECT_EQ(show.cards()[0].planet, -1);
  before.victims = {0, 3};
  RollPayShow pirates;
  pirates.saw({{-1, Action{Action::Kind::Steal, 0}}, {-1, Action{Action::Kind::Steal, 2}}}, before);
  ASSERT_EQ(pirates.cards().size(), 2u);
  EXPECT_EQ(pirates.cards()[0].from, 0);
  EXPECT_EQ(pirates.cards()[1].from, 3);
}

// A trade: the offer's cards cross between the two (a counter's, when it's
// the counter that's taken); a bank trade's go to and from the bank
TEST(TopShowsTest, TradesCross) {
  State before;
  before.turn = 1;
  Resources give{}, get{};
  give[0] = 2;
  get[3] = 1;
  RollPayShow show;
  show.saw({{1, Action{Action::Kind::OfferTrade, bundleOf(give), bundleOf(get), 2}},
            {2, Action{Action::Kind::AcceptTrade}}},
           before);
  ASSERT_EQ(show.cards().size(), 3u);
  EXPECT_EQ(show.cards()[0].from, 1);
  EXPECT_EQ(show.cards()[0].player, 2);
  EXPECT_EQ(show.cards()[2].from, 2);
  EXPECT_EQ(show.cards()[2].player, 1);
  EXPECT_FALSE(show.cards()[0].secret);
  RollPayShow bank;
  bank.saw({{1, Action{Action::Kind::Trade, 0, 4}}}, before);
  ASSERT_EQ(bank.cards().size(), static_cast<std::size_t>(tradeRateOf(before, 1) + 1));
  EXPECT_EQ(bank.cards().front().player, RollPayShow::kBank);
  EXPECT_EQ(bank.cards().back().from, RollPayShow::kBank);
}

// A card played: shown from when it's seen, rising, turning over, held,
// then away; the next after it; the game held meanwhile
TEST(TopShowsTest, ACardPlayedIsShown) {
  CardPlayShow show;
  show.saw({{1, Action{Action::Kind::PlayCartel, 2}}, {3, Action{Action::Kind::PlayEnforcer}}}, 5);
  ASSERT_NE(show.current(), nullptr);
  EXPECT_EQ(show.current()->card, Art::Card::InterstellarCartel);
  EXPECT_TRUE(show.update(5).hold);
  const auto held = show.at(5 + CardPlayShow::kSecondsUp + 0.5);
  ASSERT_TRUE(held.has_value());
  EXPECT_FLOAT_EQ(held->turned, 1);
  EXPECT_FLOAT_EQ(held->away, 0);
  show.update(5 + CardPlayShow::kSeconds);
  EXPECT_EQ(show.current()->card, Art::Card::Enforcer);
  EXPECT_TRUE(show.update(5 + 2 * CardPlayShow::kSeconds + 0.01).letGo);
  EXPECT_FALSE(show.moving(5 + 2 * CardPlayShow::kSeconds + 0.02));
}

// Choosing a resource: the card in focus slides in from the side moved
// to, the last one out; gone when the choice is
TEST(TopShowsTest, APickSlidesWithTheFocus) {
  PickShow pick;
  EXPECT_FALSE(pick.at(0).has_value());
  pick.show(Resource::Food, 1);
  ASSERT_TRUE(pick.at(1).has_value());
  EXPECT_FLOAT_EQ(pick.at(1)->in, 0);
  EXPECT_FLOAT_EQ(pick.at(1 + PickShow::kSecondsSlide)->in, 1);
  pick.show(Resource::Fuel, 2); // (right: in from the right)
  EXPECT_EQ(pick.at(2.1)->direction, 1);
  EXPECT_EQ(pick.at(2.1)->leaving, Resource::Food);
  EXPECT_TRUE(pick.moving(2.1));
  pick.show(Resource::Minerals, 3); // (left)
  EXPECT_EQ(pick.at(3.1)->direction, -1);
  EXPECT_FALSE(pick.at(4)->leaving.has_value());
  pick.show(std::nullopt, 5);
  EXPECT_FALSE(pick.at(5).has_value());
}

// Cards gained: queued until started, the piles up, each new card dropping
// in after the last, then the piles down and gone
TEST(TopShowsTest, CardsGainedDropIntoTheHand) {
  HandShow hand;
  Resources added{};
  added[1] = 2;
  hand.gained(added);
  EXPECT_TRUE(hand.waiting());
  EXPECT_FALSE(hand.showing(0));
  hand.start(10);
  EXPECT_FALSE(hand.waiting());
  EXPECT_FLOAT_EQ(hand.piles(10), 0);
  EXPECT_FLOAT_EQ(hand.piles(10 + HandShow::kSecondsPiles), 1);
  const double first = 10 + HandShow::kSecondsPiles;
  EXPECT_NEAR(hand.card(Resource::Food, 0, first), 0, 1e-4);
  EXPECT_GT(hand.card(Resource::Food, 0, first + 0.2), hand.card(Resource::Food, 1, first + 0.2));
  EXPECT_FLOAT_EQ(hand.card(Resource::Food, 1, first + 2), 1);
  EXPECT_TRUE(hand.showing(first + 1));
  EXPECT_FALSE(hand.showing(20));
}
