// The top screen's shows on their own: when each starts, moves and ends,
// and what it asks of the app (hold the game, let it go, redraw the map)
#include <CosmicHex/Handheld/DicePieces.hpp>
#include <CosmicHex/Handheld/TopShows.hpp>

#include <ScratchPadd/GameClock.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>

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

namespace {
// A game, and the same with a deep-space find turned over by player
// `by`: salvage (its choice of resources to come), or ruins
std::pair<State, State> aFindTurnedOver(Find find, int by = 0) {
  State before = newGame(4, agreedRules(boardR3()));
  before.turn = by;
  State after = before;
  for (ZoneSite &site : after.zone)
    if (!site.flipped) {
      site.flipped = true;
      site.find = find;
      break;
    }
  if (find == Find::Salvage) {
    after.phase = Phase::Salvage;
    after.salvageLeft = after.settings.salvageResources;
  }
  return {before, after};
}
} // namespace

// A salvage find: the choice waits until its words are in (the picker
// stays shut); from then the game's let go, for the choice to be made; at
// the end of its hold the tile waits, facing you, until it's released;
// then back down as ever, the game held again. Another find never waits
TEST(TopShowsTest, ASalvageFindWaitsForItsChoice) {
  const auto [before, after] = aFindTurnedOver(Find::Salvage);
  DiscoveryShow show;
  show.arrived(before, after, 0);
  ASSERT_NE(show.current(), nullptr);
  EXPECT_TRUE(show.current()->choiceAfter);
  EXPECT_TRUE(show.choiceStillToShow(0));
  EXPECT_TRUE(show.update(0, nullptr).hold);
  EXPECT_TRUE(show.choiceStillToShow(Discovery::kSecondsToWords - 0.01));
  EXPECT_FALSE(show.waitingForChoice(Discovery::kSecondsToWords - 0.01));
  // Its words in: the game let go; the picker can open
  const double words = Discovery::kSecondsToWords + 0.01;
  EXPECT_FALSE(show.choiceStillToShow(words));
  EXPECT_TRUE(show.waitingForChoice(words));
  EXPECT_TRUE(show.update(words, nullptr).letGo);
  // Long past its hold: still facing you, not on its way down
  const double long_ = Discovery::kSeconds + 5;
  show.update(long_, nullptr);
  ASSERT_TRUE(show.moment(long_).has_value());
  EXPECT_FLOAT_EQ(show.moment(long_)->down, 0);
  EXPECT_FLOAT_EQ(show.moment(long_)->lifted(), 1);
  // Released: down from where it waited, the game held till it's done
  show.release(long_);
  EXPECT_FALSE(show.waitingForChoice(long_));
  EXPECT_TRUE(show.update(long_ + 0.01, nullptr).hold);
  const double downAt = long_ + 0.1;
  EXPECT_GT(show.moment(downAt)->down, 0);
  const double ends = long_ + (Discovery::kSeconds - Discovery::kSecondsToDown);
  EXPECT_FALSE(show.update(ends - 0.05, nullptr).letGo);
  EXPECT_TRUE(show.update(ends + 0.01, nullptr).letGo);
  EXPECT_FALSE(show.waiting());
  // Ruins: no choice; the game held throughout
  const auto [plain, ruins] = aFindTurnedOver(Find::Ruins);
  DiscoveryShow other;
  other.arrived(plain, ruins, 0);
  EXPECT_FALSE(other.choiceStillToShow(0));
  other.update(0, nullptr);
  EXPECT_FALSE(other.waitingForChoice(Discovery::kSecondsToDown + 0.5));
  EXPECT_TRUE(other.update(Discovery::kSeconds + 0.01, nullptr).letGo);
}

// Released before its hold ends (the choice made quickly): no wait at all
TEST(TopShowsTest, ASalvageFindChosenQuicklyDoesntWait) {
  const auto [before, after] = aFindTurnedOver(Find::Salvage);
  DiscoveryShow show;
  show.arrived(before, after, 0);
  show.update(0, nullptr);
  show.release(Discovery::kSecondsToWords + 0.2);
  EXPECT_TRUE(show.update(Discovery::kSeconds + 0.01, nullptr).letGo);
}

// Your salvage: begun when the find's words are in; your picks shown as you
// make them (one changed comes in afresh, the order kept), the words
// dimming; taken, the game held while they're seen and fly off one after
// the other; landed, it's over and the game let go
TEST(TopShowsTest, YourSalvageFollowsThePickerThenFlies) {
  const auto [before, after] = aFindTurnedOver(Find::Salvage, 0);
  SalvageShow show;
  show.saw({}, after, 0, 1);
  ASSERT_TRUE(show.on());
  EXPECT_EQ(show.player(), 0);
  EXPECT_EQ(show.howMany(), 2);
  EXPECT_TRUE(show.cards(2).empty()); // (not begun: the find still turning)
  EXPECT_FALSE(show.waitingForChoice(2));
  show.begin(3);
  EXPECT_TRUE(show.waitingForChoice(3));
  EXPECT_FLOAT_EQ(show.dim(3), 0);
  // Picked: Fuel, then Water instead of a second Fuel
  show.choosing({Resource::Fuel}, 4);
  EXPECT_FALSE(show.waitingForChoice(4));
  ASSERT_EQ(show.cards(4.1).size(), 1u);
  EXPECT_EQ(show.cards(4.1)[0].slot, 0);
  EXPECT_GT(show.cards(4.1)[0].in, 0);
  EXPECT_LT(show.cards(4.1)[0].in, 1);
  EXPECT_FLOAT_EQ(show.dim(4 + SalvageShow::kSecondsIn), SalvageShow::kWordsDim);
  show.choosing({Resource::Fuel, Resource::Fuel}, 5);
  show.choosing({Resource::Fuel, Resource::Water}, 6);
  auto shown = show.cards(6.1);
  ASSERT_EQ(shown.size(), 2u);
  EXPECT_EQ(shown[0].resource, Resource::Fuel);
  EXPECT_FLOAT_EQ(shown[0].in, 1); // (the first kept as it was)
  EXPECT_EQ(shown[1].resource, Resource::Water);
  EXPECT_EQ(shown[1].slot, 1);
  EXPECT_LT(shown[1].in, 1);         // (the changed one afresh)
  EXPECT_FALSE(show.update(7).hold); // (picked, not taken: the game isn't held)
  // Taken, a move each, in order: held
  show.saw({{0, Action{Action::Kind::TakeSalvage, static_cast<int>(Resource::Fuel)}}}, after, 0, 8);
  EXPECT_FALSE(show.update(8).hold); // (one of two)
  show.choosing({}, 8.05);           // (the picker closed: ignored, once taken)
  show.saw({{0, Action{Action::Kind::TakeSalvage, static_cast<int>(Resource::Water)}}}, after, 0,
           8.1);
  EXPECT_TRUE(show.update(8.1).hold);
  shown = show.cards(8.1);
  ASSERT_EQ(shown.size(), 2u);
  EXPECT_FLOAT_EQ(shown[0].away, 0);
  // Off one after the other, at your pace
  const double off = 8.1 + 0.01;
  EXPECT_GT(show.cards(off + SalvageShow::kSecondsAway / 2)[0].away, 0);
  EXPECT_GT(show.cards(off + SalvageShow::kSecondsAway / 2)[0].away,
            show.cards(off + SalvageShow::kSecondsAway / 2)[1].away);
  EXPECT_TRUE(show.moving(off));
  const double landed = 8.1 + SalvageShow::kStagger + SalvageShow::kSecondsAway;
  EXPECT_TRUE(show.on());
  const auto last = show.update(landed + 0.01);
  EXPECT_TRUE(last.letGo);
  EXPECT_FALSE(show.on());
  EXPECT_TRUE(show.cards(landed + 0.01).empty());
}

// A bot's salvage: "Choosing..." until its first pick; its picks as its
// moves come (the first with the find itself), shown once the find's
// words are in; held from the last until they land; the same show a
// little faster than yours
TEST(TopShowsTest, ABotsSalvageComesWithItsMovesAndIsQuicker) {
  const auto [before, after] = aFindTurnedOver(Find::Salvage, 2);
  const auto end = [&](int you) {
    SalvageShow show;
    // (the find and the bot's first pick in one look)
    State one = after;
    one.salvageLeft = 1;
    show.saw({{2, Action{Action::Kind::TakeSalvage, 4}}}, one, you, 0);
    EXPECT_EQ(show.player(), 2);
    EXPECT_TRUE(show.cards(0.5).empty()); // (the find still turning over)
    show.begin(2);
    EXPECT_EQ(show.cards(2.05).size(), 1u);
    EXPECT_FALSE(show.update(2.5).hold); // (the game let go for its second)
    State done = after;
    done.phase = Phase::Main;
    show.saw({{2, Action{Action::Kind::TakeSalvage, 4}}}, done, you, 3);
    EXPECT_TRUE(show.update(3).hold);
    double t = 3;
    while (show.on() && t < 10) {
      t += 0.01;
      show.update(t);
    }
    return t;
  };
  // (watching, every player a bot; and playing, with the bot not you)
  const double watching = end(-1), playing = end(0);
  EXPECT_NEAR(watching, playing, 0.02);
  const double yours = 3 + SalvageShow::kSecondsIn + SalvageShow::kSecondsRead +
                       SalvageShow::kStagger + SalvageShow::kSecondsAway;
  EXPECT_LT(watching, yours - 0.2); // (quicker)
  EXPECT_GT(watching, 3.2);         // (but still seen)
}

// B, and Animations Off (B every frame): the shows' clock jumps ahead and
// the salvage lands at once; a choice still to make waits for it
TEST(TopShowsTest, SkippingASalvageLandsItAtOnce) {
  const auto [before, after] = aFindTurnedOver(Find::Salvage, 0);
  SalvageShow show;
  show.saw({}, after, 0, 0);
  show.begin(0);
  EXPECT_TRUE(show.waitingForChoice(30)); // (skipped ahead: still yours to choose)
  show.choosing({Resource::Food, Resource::Food}, 30);
  show.saw({{0, Action{Action::Kind::TakeSalvage, 1}}, {0, Action{Action::Kind::TakeSalvage, 1}}},
           after, 0, 30.1);
  EXPECT_TRUE(show.update(30.1).hold);
  EXPECT_TRUE(show.update(60).letGo);
  EXPECT_FALSE(show.on());
}

// The cards dealt: thrown from when they're dealt (no close-ups to wait
// for: their icons are loaded sharp), then turned
TEST(TopShowsTest, DealtCardsThrownThenTurned) {
  DealtShow dealt;
  dealt.deal({Art::Card::Enforcer, Art::Card::SupplyDrop}, 11);
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

// Another chosen while it's open: a carousel. The last slides out one way
// as the next comes in from the other, side by side; the next player's
// from the right, the one before from the left
TEST(TopShowsTest, ChoosingAnotherSlidesLikeACarousel) {
  PanelShow panel(0.3f);
  panel.open(2, 0);
  panel.choose(3, 1); // (the next: in from the right)
  EXPECT_EQ(panel.which(), 3);
  auto both = panel.drawn(1.15);
  ASSERT_EQ(both.size(), 2u);
  EXPECT_EQ(both[0].which, 2);
  EXPECT_EQ(both[1].which, 3);
  EXPECT_FLOAT_EQ(both[0].down, 1);                       // (in place, sliding across)
  EXPECT_LT(both[0].across, 0);                           // (going left)
  EXPECT_GT(both[1].across, 0);                           // (coming from the right)
  EXPECT_NEAR(both[1].across - both[0].across, 1, 1e-5f); // (side by side)
  const auto done = panel.drawn(1.31);
  ASSERT_EQ(done.size(), 1u);
  EXPECT_EQ(done[0].which, 3);
  EXPECT_FLOAT_EQ(done[0].across, 0);
  panel.choose(2, 2, -1); // (back: in from the left)
  both = panel.drawn(2.1);
  ASSERT_EQ(both.size(), 2u);
  EXPECT_LT(both[1].across, 0);
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
  // (then the players moved into order: not at once, all the way well
  // before it's done)
  EXPECT_FLOAT_EQ(show.sorted(now), 0);
  EXPECT_FLOAT_EQ(show.sorted(now + FirstRollShow::kSecondsSort + 0.5), 1);
  EXPECT_LT(show.sorted(now + 0.6, 2), show.sorted(now + 0.6, 0)); // (each after the last)
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

// A moment after a part's end (the shows' times are sums of float
// constants, a hair off when added up in another order)
constexpr double kJustAfter = 1e-4;

// A home's pay, one card: the die dips into the board (the wind-up),
// lifts out to face you, spins to its side; the card big, then away; the
// die back over its place, dropped in and settled, space rippling where it
// leaves and where it seats; the game held meanwhile and let go after, the
// board's tile gone while it's out
TEST(TopShowsTest, AHomePaysOnItsDie) {
  using S = HomePayShow;
  HomePayShow show;
  const State paid = newGame(4, agreedRules(boardR3())); // (not the home die's phase: paid)
  show.saw({{2, Action{Action::Kind::RollHomeDie, 3}}}, paid, 10);
  ASSERT_NE(show.current(), nullptr);
  EXPECT_EQ(show.current()->player, 2);
  EXPECT_EQ(show.current()->resource, Resource::Water);
  EXPECT_TRUE(show.update(10).hold);
  EXPECT_EQ(show.dieOutOf(10), 2);
  // (space rippling where it leaves, gone before it faces you)
  ASSERT_TRUE(show.die(10.1)->ripple.has_value());
  EXPECT_GT(*show.die(10.1)->ripple, 0);
  EXPECT_FALSE(show.die(10 + S::kSecondsOut)->ripple.has_value());
  // (the wind-up: down into the board, not lifted yet)
  const auto dipping = show.die(10 + S::kSecondsWindUp * 0.9);
  ASSERT_TRUE(dipping.has_value());
  EXPECT_GT(dipping->sink, S::kDip * 0.9f);
  EXPECT_FLOAT_EQ(dipping->lifted, 0);
  // (out and facing you before it spins; turned to its side once it has)
  const double spins = 10 + S::kSecondsOut;
  EXPECT_NEAR(show.die(spins)->lifted, 1, 1e-5);
  EXPECT_NEAR(show.die(spins)->sink, 0, 1e-5);
  const float landed = show.die(spins + S::kSecondsSpin)->turn;
  EXPECT_NEAR(std::remainder(landed - homeDieTurn(3), 6.2831853f), 0, 1e-4);
  EXPECT_FALSE(show.card(spins + S::kSecondsSpin - 0.01).has_value());
  // (its card, grown and held, then off)
  const auto held = show.card(spins + S::kSecondsSpin + S::kSecondsCard);
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->resource, Resource::Water);
  EXPECT_NEAR(held->grown, 1, 1e-4);
  EXPECT_FLOAT_EQ(held->away, 0);
  const double leaves = spins + S::kSecondsPay;
  EXPECT_GT(show.card(leaves + S::kSecondsAway / 2)->away, 0);
  // (back as the card leaves: over its place, then dropped in, seated)
  EXPECT_LT(show.die(leaves + S::kSecondsBack / 2)->lifted, 1);
  EXPECT_NEAR(show.die(leaves + S::kSecondsBack)->lifted, kHomeDieRisen, 1e-5);
  const double touches = leaves + S::kSecondsBack + S::kSecondsDrop;
  EXPECT_NEAR(show.die(touches)->lifted, 0, 1e-4);
  EXPECT_NEAR(std::remainder(show.die(touches)->turn, 6.2831853f), 0, 1e-4); // (its tile as it lay)
  EXPECT_FALSE(show.die(touches - 0.01)->ripple.has_value());
  ASSERT_TRUE(show.die(touches + 0.1)->ripple.has_value()); // (space rippling where it seats)
  EXPECT_FALSE(show.dieOutOf(touches + S::kSecondsSeat + kJustAfter).has_value()); // (tile back)
  // (the ripple gone: done, the game let go)
  const double end = 10 + HomePayShow::seconds(1) + kJustAfter;
  EXPECT_TRUE(show.moving(end - 0.01));
  EXPECT_FALSE(show.update(end - 0.01).letGo);
  EXPECT_TRUE(show.update(end).letGo);
  EXPECT_FALSE(show.moving(end));
  EXPECT_EQ(show.current(), nullptr);
}

// A home paying two: the die comes out once, spins for the first, spins
// again as its card leaves (never back in between), and goes back only
// after the second's card
TEST(TopShowsTest, AHomePayingTwoStaysOutForBoth) {
  using S = HomePayShow;
  HomePayShow show;
  const State paid = newGame(4, agreedRules(boardR3()));
  show.saw({{1, Action{Action::Kind::RollHomeDie, 0}}, {1, Action{Action::Kind::RollHomeDie, 4}}},
           paid, 0);
  EXPECT_TRUE(show.update(0).hold);
  // (out and facing you from the first spin to the second's card leaving)
  const double second = S::kSecondsOut + S::kSecondsPay;
  for (double t = S::kSecondsOut; t < second + S::kSecondsPay; t += 0.05) {
    EXPECT_NEAR(show.die(t)->lifted, 1, 1e-5) << t;
    EXPECT_FALSE(show.update(t).letGo) << t;
  }
  EXPECT_NEAR(std::remainder(show.die(second)->turn - homeDieTurn(0), 6.2831853f), 0, 1e-4);
  EXPECT_EQ(show.card(second + 0.1)->order, 0); // (the first card leaving as it spins again)
  EXPECT_EQ(show.card(second + S::kSecondsSpin + 0.1)->order, 1);
  EXPECT_NEAR(std::remainder(show.die(second + S::kSecondsSpin)->turn - homeDieTurn(4), 6.2831853f),
              0, 1e-4);
  EXPECT_EQ(show.card(second + S::kSecondsSpin + 0.1)->resource, Resource::Crystals);
  // (then back, once)
  EXPECT_LT(show.die(second + S::kSecondsPay + 0.1)->lifted, 1);
  EXPECT_TRUE(show.update(HomePayShow::seconds(2) + kJustAfter).letGo);
  EXPECT_FALSE(show.moving(HomePayShow::seconds(2) + kJustAfter));
}

// The game saying the home pays again (the dealer rolls next): the die
// waits out after the card, the game let go to roll; the next roll spins
// it again where it is
TEST(TopShowsTest, TheDieWaitsOutForTheNextRoll) {
  using S = HomePayShow;
  HomePayShow show;
  State more = newGame(4, agreedRules(boardR3()));
  more.turn = 0;
  more.phase = Phase::HomeDieDraw;
  show.saw({{0, Action{Action::Kind::RollHomeDie, 2}}}, more, 0);
  EXPECT_TRUE(show.update(0).hold);
  const double leaves = S::kSecondsOut + S::kSecondsPay;
  EXPECT_FALSE(show.update(leaves - 0.01).letGo);
  EXPECT_TRUE(show.update(leaves + kJustAfter).letGo); // (waiting for the roll)
  EXPECT_TRUE(show.moving(leaves + 3));
  EXPECT_FLOAT_EQ(show.die(leaves + 3)->lifted, 1);
  const float waitingTurn = show.die(leaves + 3)->turn;
  State paid = more;
  paid.phase = Phase::RollDraw;
  show.saw({{0, Action{Action::Kind::RollHomeDie, 1}}}, paid, leaves + 3);
  EXPECT_TRUE(show.update(leaves + 3).hold);
  EXPECT_FLOAT_EQ(show.die(leaves + 3)->turn, waitingTurn); // (spun from where it waited)
  EXPECT_FLOAT_EQ(show.die(leaves + 3 + S::kSecondsPay)->lifted, 1);
  const double end = leaves + 3 + S::kSecondsPay + S::kSecondsHome + kJustAfter;
  EXPECT_TRUE(show.update(end).letGo);
  EXPECT_FALSE(show.moving(end));
}

// The star: held at the landed die until the choice is made (the game let
// go to make it), then its card
TEST(TopShowsTest, TheStarWaitsForTheChoice) {
  using S = HomePayShow;
  HomePayShow show;
  State choosing = newGame(4, agreedRules(boardR3()));
  choosing.phase = Phase::HomeIncome;
  show.saw({{0, Action{Action::Kind::RollHomeDie, kResources}}}, choosing, 0);
  EXPECT_FALSE(show.current()->resource.has_value());
  EXPECT_TRUE(show.update(0).hold);
  EXPECT_TRUE(show.update(5).letGo); // (waiting for the choice: the game goes on, to make it)
  EXPECT_TRUE(show.waitingForChoice(5));
  EXPECT_FALSE(show.card(5).has_value());
  EXPECT_NEAR(std::remainder(show.die(5)->turn - homeDieTurn(kResources), 6.2831853f), 0, 1e-4);
  State paid = choosing;
  paid.phase = Phase::RollDraw;
  show.saw({{0, Action{Action::Kind::ChooseHome, 1}}}, paid, 6);
  EXPECT_EQ(show.current()->resource, Resource::Food);
  EXPECT_FALSE(show.waitingForChoice(6));
  EXPECT_TRUE(show.update(6).hold); // (held again for its card)
  EXPECT_EQ(show.card(6.1)->resource, Resource::Food);
  EXPECT_TRUE(
      show.update(6 + S::kSecondsCard + S::kSecondsHold + S::kSecondsHome + kJustAfter).letGo);
}

// B, or Animations Off: the shows' clock skipped on past any end, the
// whole pay done in one update (two spins included), the game let go
TEST(TopShowsTest, SkippingAHomesPay) {
  HomePayShow show;
  const State paid = newGame(4, agreedRules(boardR3()));
  show.saw({{1, Action{Action::Kind::RollHomeDie, 0}}, {1, Action{Action::Kind::RollHomeDie, 2}}},
           paid, 0);
  EXPECT_TRUE(show.update(0.5).hold);
  EXPECT_TRUE(show.update(0.5 + 30).letGo);
  EXPECT_FALSE(show.moving(0.5 + 30));
  EXPECT_FALSE(show.dieOutOf(0.5 + 30).has_value());
}

// Animations Quick: the shows' clock runs twice as fast, so a pay takes
// half the real time, every part in its place
TEST(TopShowsTest, AQuickHomesPayTakesHalfTheTime) {
  using Clock = ScratchPadd::GameClock::RealClock;
  const Clock::time_point start{};
  ScratchPadd::GameClock shows(start);
  shows.setRate(2, start);
  HomePayShow show;
  show.saw({{1, Action{Action::Kind::RollHomeDie, 0}}}, newGame(4, agreedRules(boardR3())),
           shows.seconds(start));
  const auto at = [&](double realSeconds) {
    return shows.seconds(start + std::chrono::duration_cast<Clock::duration>(
                                     std::chrono::duration<double>(realSeconds)));
  };
  EXPECT_TRUE(show.update(at(0)).hold);
  const double half = HomePayShow::seconds(1) / 2;
  EXPECT_FALSE(show.update(at(half - 0.01)).letGo);
  EXPECT_TRUE(show.update(at(half + kJustAfter)).letGo);
}

// A spin: back a little first, off past its side and back to it, at least
// a whole turn on
TEST(TopShowsTest, ASpinWindsUpAndSettles) {
  using S = HomePayShow;
  for (const float from : {0.0f, -2.0f, -9.0f})
    for (int face = 0; face <= kResources; ++face) {
      const float to = S::spinEnd(from, face);
      EXPECT_LE(to, from - 6.2831853f + 1e-4f);
      EXPECT_GT(to, from - 2 * 6.2831853f);
      EXPECT_NEAR(std::remainder(to - homeDieTurn(face), 6.2831853f), 0, 1e-4);
      EXPECT_FLOAT_EQ(S::spinAt(from, face, 0), from);
      EXPECT_NEAR(S::spinAt(from, face, S::kSecondsSpin * 0.15f), from + S::kWindTurn, 1e-4);
      EXPECT_NEAR(S::spinAt(from, face, S::kSecondsSpin * 0.85f), to - S::kOvershoot, 1e-4);
      EXPECT_FLOAT_EQ(S::spinAt(from, face, S::kSecondsSpin), to);
    }
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
  show.saw({{0, Action{Action::Kind::Roll, 2, 4}}}, state, 0);
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
  seven.saw({{0, Action{Action::Kind::Roll, 3, 4}}}, state, 0); // (a 7 pays nothing)
  EXPECT_TRUE(seven.cards().empty());
}

// The starting hand: each card from the bank (the ROUND hexagon), round the
// table from the first player, one a little after another; seen only by
// whoever it's dealt to; the game held while they fly, let go after the last
TEST(TopShowsTest, TheStartingHandIsDealtFromTheBank) {
  Settings settings = agreedRules(boardR3());
  settings.startCards = 3;
  const State before = newGame(4, settings, 2);
  ASSERT_EQ(before.phase, Phase::StartCardsDraw);
  constexpr int kCards = 4 * 3;
  std::vector<TableGame::Played> deal;
  deal.reserve(kCards);
  for (int i = 0; i < kCards; ++i)
    deal.push_back({-1, Action{Action::Kind::DealStartCard, i % kResources}});
  RollPayShow show;
  show.saw(deal, before, 0);
  ASSERT_EQ(show.cards().size(), 12u);
  for (std::size_t i = 0; i < show.cards().size(); ++i) {
    const RollPayShow::Card &card = show.cards()[i];
    EXPECT_EQ(card.player, static_cast<int>((2 + i) % 4)) << "card " << i; // (the first is 2)
    EXPECT_EQ(card.from, RollPayShow::kBank);
    EXPECT_EQ(card.planet, -1);
    EXPECT_EQ(card.resource, static_cast<Resource>(i % kResources));
    EXPECT_TRUE(card.secret);
  }
  // (each starts a stagger after the last, popping out, then flying)
  show.start(10);
  EXPECT_TRUE(show.update(10).hold);
  EXPECT_FALSE(show.at(1, 10 + RollPayShow::kStagger - 0.01).has_value());
  ASSERT_TRUE(show.at(1, 10 + RollPayShow::kStagger + RollPayShow::kSecondsPop).has_value());
  EXPECT_FLOAT_EQ(show.at(1, 10 + RollPayShow::kStagger + RollPayShow::kSecondsPop)->pop, 1);
  EXPECT_FLOAT_EQ(show.at(1, 10 + RollPayShow::kStagger + RollPayShow::kSecondsPop)->fly, 0);
  EXPECT_FLOAT_EQ(show.seconds(),
                  11 * RollPayShow::kStagger + RollPayShow::kSecondsPop + RollPayShow::kSecondsFly);
  EXPECT_LT(show.seconds(), 2.5f); // (twelve cards dealt briskly)
  EXPECT_TRUE(show.moving(10 + show.seconds() - 0.01));
  EXPECT_FALSE(show.update(10 + show.seconds() - 0.01).letGo);
  EXPECT_TRUE(show.update(10 + show.seconds()).letGo);
  EXPECT_FALSE(show.moving(10 + show.seconds()));
}

// ...dealt in more than one look, the next card still goes to the next
// player round the table
TEST(TopShowsTest, AStartingHandSeenInPartsKeepsItsOrder) {
  Settings settings = agreedRules(boardR3());
  settings.startCards = 3;
  State before = newGame(3, settings, 1);
  before.startCardsLeft -= 4; // (four dealt already: the next is the first's second)
  RollPayShow show;
  show.saw(
      {{-1, Action{Action::Kind::DealStartCard, 0}}, {-1, Action{Action::Kind::DealStartCard, 1}}},
      before, 0);
  ASSERT_EQ(show.cards().size(), 2u);
  EXPECT_EQ(show.cards()[0].player, 2);
  EXPECT_EQ(show.cards()[1].player, 0);
}

// A steal: after the blockade names who it's from, the card flies from
// them to the player whose turn it is; the pirate hideout's, from each in
// the order the game has them
TEST(TopShowsTest, AStealFliesFromTheVictim) {
  State before;
  before.turn = 2;
  RollPayShow show;
  show.saw({{2, Action{Action::Kind::MoveBlockade, 3, 1}}, {-1, Action{Action::Kind::Steal, 4}}},
           before, 0);
  ASSERT_EQ(show.cards().size(), 1u);
  EXPECT_EQ(show.cards()[0].from, 1);
  EXPECT_EQ(show.cards()[0].player, 2);
  EXPECT_EQ(show.cards()[0].planet, -1);
  before.victims = {0, 3};
  RollPayShow pirates;
  pirates.saw({{-1, Action{Action::Kind::Steal, 0}}, {-1, Action{Action::Kind::Steal, 2}}}, before,
              0);
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
           before, 0);
  ASSERT_EQ(show.cards().size(), 3u);
  EXPECT_EQ(show.cards()[0].from, 1);
  EXPECT_EQ(show.cards()[0].player, 2);
  EXPECT_EQ(show.cards()[2].from, 2);
  EXPECT_EQ(show.cards()[2].player, 1);
  EXPECT_FALSE(show.cards()[0].secret);
  RollPayShow bank;
  bank.saw({{1, Action{Action::Kind::Trade, 0, 4}}}, before, 0);
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

// Cards gained while the hand shows are shown again after it (the hand's
// show started once a game: it never let itself start again)
TEST(TopShowsTest, CardsGainedShowEveryTime) {
  HandShow hand;
  Resources added{};
  added[2] = 1;
  hand.gained(added);
  hand.start(10);
  hand.gained(added); // (while it shows)
  hand.start(10.5);   // (not over it)
  EXPECT_TRUE(hand.waiting());
  EXPECT_TRUE(hand.showing(10.6));
  hand.start(20);
  EXPECT_FALSE(hand.waiting());
  EXPECT_TRUE(hand.showing(20.1));
}

// Cards joining a flight already under way (a build's cost while the
// roll's pay flies) fly from when they join, after the last, not from the
// flight's start: they were first seen part way along, or not at all
TEST(TopShowsTest, CardsJoiningAFlightFlyFromWhenTheyJoin) {
  State before;
  before.turn = 1;
  RollPayShow show;
  show.saw({{1, Action{Action::Kind::Trade, 0, 4}}}, before, 0);
  show.start(10);
  const std::size_t first = show.cards().size();
  show.saw({{1, Action{Action::Kind::Trade, 1, 3}}}, before, 11);
  ASSERT_GT(show.cards().size(), first);
  ASSERT_TRUE(show.at(first, 11).has_value());
  EXPECT_FLOAT_EQ(show.at(first, 11)->pop, 0);
  EXPECT_TRUE(show.moving(11 + RollPayShow::kSecondsPop));
  EXPECT_FALSE(show.update(11.1).letGo);
}

// Shows over the board take turns (GameShows tells one to wait while
// another has the stage): one still to start waits; one under way doesn't
TEST(TopShowsTest, AShowStillToStartWaitsItsTurn) {
  HomePayShow home;
  home.saw({{1, Action{Action::Kind::RollHomeDie, 0}}}, newGame(4, agreedRules(boardR3())), 5);
  home.waitFor(8);
  EXPECT_FALSE(home.moving(7));
  EXPECT_DOUBLE_EQ(home.outAt(), 8);
  home.update(8.1);
  home.waitFor(9); // (out already: it goes on)
  EXPECT_DOUBLE_EQ(home.outAt(), 8);
  CardPlayShow card;
  card.saw({{1, Action{Action::Kind::PlayEnforcer}}}, 5);
  card.waitFor(6);
  EXPECT_FALSE(card.moving(5.5));
  card.update(6.1);
  card.waitFor(7);
  EXPECT_TRUE(card.moving(6.5));
}

// The board's pointer: fades in at its corner, glides to the next, fades
// away when there's nothing to choose, and comes back at its new corner
TEST(TopShowsTest, ThePointerFadesInGlidesAndFadesAway) {
  const Corner a = CosmicHex::boardCorners()[0], b = CosmicHex::boardCorners()[1],
               c = CosmicHex::boardCorners()[2];
  CornerPointer pointer;
  EXPECT_FALSE(pointer.at(0).has_value());
  pointer.aim(a, 1);
  ASSERT_TRUE(pointer.at(1.1).has_value());
  EXPECT_LT(pointer.at(1.1)->shown, 1);
  EXPECT_EQ(pointer.at(1.1)->to, a);
  EXPECT_FLOAT_EQ(pointer.at(1.5)->shown, 1);
  pointer.aim(b, 2);
  EXPECT_EQ(pointer.at(2.1)->from, a);
  EXPECT_EQ(pointer.at(2.1)->to, b);
  EXPECT_LT(pointer.at(2.1)->along, 1);
  EXPECT_FLOAT_EQ(pointer.at(2.3)->along, 1);
  pointer.aim(std::nullopt, 3);
  EXPECT_LT(pointer.at(3.1)->shown, 1);
  EXPECT_FALSE(pointer.at(3.4).has_value());
  pointer.aim(c, 5); // (back, at its new corner: no glide from the last)
  EXPECT_EQ(pointer.at(5.1)->from, c);
  EXPECT_FALSE(pointer.moving(6));
}

// A corner chosen: the pointer taps it once it's there. Up a little first,
// down to touching, dust from the touch, then back up and settled
TEST(TopShowsTest, ThePointerTapsTheCornerChosen) {
  const Corner a = CosmicHex::boardCorners()[0], b = CosmicHex::boardCorners()[1];
  CornerPointer pointer;
  pointer.aim(a, 0);
  pointer.aim(b, 1); // (gliding to b)
  pointer.tap(1);    // (waits for the glide)
  EXPECT_FLOAT_EQ(pointer.at(1.1)->dip, 0);
  const double start = 1 + CornerPointer::kSecondsGlide;
  EXPECT_LT(pointer.at(start + CornerPointer::kSecondsLift * 0.9)->dip, 0); // (the lift)
  const double touch = start + CornerPointer::kSecondsLift + CornerPointer::kSecondsDrop;
  EXPECT_NEAR(pointer.at(touch - 0.001)->dip, 1, 0.05f);
  EXPECT_FALSE(pointer.at(touch - 0.01)->dust.has_value());
  ASSERT_TRUE(pointer.at(touch + 0.1)->dust.has_value());
  EXPECT_GT(*pointer.at(touch + 0.1)->dust, 0);
  EXPECT_LT(pointer.at(touch + 0.1)->dip, 1); // (going back up)
  EXPECT_TRUE(pointer.moving(touch + 0.1));
  EXPECT_NEAR(pointer.at(touch + 1)->dip, 0, 1e-4f);
  EXPECT_FALSE(pointer.at(touch + 1)->dust.has_value());
  EXPECT_FALSE(pointer.moving(touch + 1));
}
