// Cosmic Hex's rules: the board, a turn, production, the blockade, moving
// and turning tiles over, settling, colonies, Core Control, contracts,
// winning, hidden contracts, and the text the action log keeps.
#include <CosmicHex/BotStyles.hpp>
#include <CosmicHex/Rules.hpp>

#include <GameBots/Bots.hpp>
#include <SeededRandom/Seed.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace CosmicHex;
using Kind = Action::Kind;

namespace {

bool has(const std::vector<Action> &actions, const Action &action) {
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}
bool hasKind(const std::vector<Action> &actions, Kind kind) {
  return std::any_of(actions.begin(), actions.end(),
                     [&](const Action &a) { return a.kind == kind; });
}

// A game at player 0's main phase, after their home income and a quiet roll
State atMain(int players = 2) {
  State state = newGame(players);
  EXPECT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  EXPECT_TRUE(Game::apply(state, {Kind::Roll, 1, 2})); // 3: nothing turned over yet
  EXPECT_EQ(state.phase, Phase::Main);
  return state;
}

int planetIndex(const State &state, Hex hex) {
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (state.planets[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}

// Turns a planet over by hand, as a reveal would
void flip(State &state, int planet, PlanetKind kind, int number) {
  state.planets[static_cast<std::size_t>(planet)].flipped = true;
  state.planets[static_cast<std::size_t>(planet)].kind = kind;
  state.planets[static_cast<std::size_t>(planet)].number = number;
}

} // namespace

TEST(RulesTest, TheBoard) {
  const State state = newGame(4);
  EXPECT_EQ(state.planets.size(), 19u);
  EXPECT_EQ(state.zone.size(), 12u);
  int homes = 0, core = 0, faceDown = 0;
  for (const Planet &planet : state.planets) {
    homes += planet.homeOf >= 0;
    core += planet.core;
    faceDown += !planet.flipped;
  }
  EXPECT_EQ(homes, 4);
  EXPECT_EQ(core, 7); // the centre and the six around it
  EXPECT_EQ(faceDown, 15);
  int corners = 0;
  for (const ZoneSite &site : state.zone)
    corners += HexGrid::length(site.hex) == 4;
  EXPECT_EQ(corners, 6); // the zone's inner six moved to the corners
  // Without the variant, the zone is the 12 space hexes within 2 of the centre
  Settings middle;
  middle.zoneCorners = false;
  for (const ZoneSite &site : newGame(4, middle).zone)
    EXPECT_LE(HexGrid::length(site.hex), 2);
  // The tiles and tokens to draw match the slots
  EXPECT_EQ(state.kindsLeft[0] + state.kindsLeft[1] + state.kindsLeft[2] + state.kindsLeft[3] +
                state.kindsLeft[4],
            15);
  EXPECT_EQ(state.numbersLeft.size(), 15u);
}

// The core can be named instead of the 7 within 2 of the centre (round
// 28's option B: the centre and one opposite pair)
TEST(RulesTest, ASmallerCore) {
  Settings settings;
  settings.coreHexes = {{0, 0}, {1, -2}, {-1, 2}};
  const State state = newGame(4, settings);
  int core = 0;
  for (const Planet &planet : state.planets)
    core += planet.core;
  EXPECT_EQ(core, 3);
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}).settings.coreHexes, settings.coreHexes);
}

TEST(RulesTest, TheStart) {
  const State state = newGame(3);
  for (int p = 0; p < 3; ++p) {
    EXPECT_EQ(points(state, p), 1); // the home station
    EXPECT_EQ(state.player[static_cast<std::size_t>(p)].resources, (Resources{1, 1, 1, 1, 1}));
  }
  EXPECT_EQ(state.ships.size(), 3u);
  EXPECT_EQ(Game::currentPlayer(state), 0);
  EXPECT_EQ(Game::legalActions(state).size(), 5u); // the home's resource
}

// Any player can go first, each keeping their home; the start reads back
TEST(RulesTest, AnyoneCanGoFirst) {
  const State state = newGame(4, {}, 2);
  EXPECT_EQ(state.turn, 2);
  EXPECT_EQ(Game::currentPlayer(state), 2);
  EXPECT_EQ(state.ships[2].at, newGame(4).ships[2].at); // the same home
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}), state);
}

// A start bonus goes by turn order, not by home: with player 2 first, the
// bonus for the 2nd to play goes to player 3
TEST(RulesTest, AStartBonusByTurnOrder) {
  Settings settings;
  settings.seatBonus = {0, 1, 2, 3};
  const State state = newGame(4, settings, 2);
  EXPECT_EQ(state.player[2].resources, (Resources{1, 1, 1, 1, 1})); // 1st to play
  EXPECT_EQ(state.player[3].resources, (Resources{2, 1, 1, 1, 1})); // 2nd
  EXPECT_EQ(state.player[0].resources, (Resources{2, 2, 1, 1, 1})); // 3rd
  EXPECT_EQ(state.player[1].resources, (Resources{2, 2, 2, 1, 1})); // 4th
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}).settings.seatBonus, settings.seatBonus);
}

// Three players' homes can be spread evenly; the other slots hold planets
TEST(RulesTest, HomesCanBeNamed) {
  Settings settings;
  settings.homeHexes = {{4, -2}, {-2, -2}, {-2, 4}};
  settings.planetTiles[0] = 4;
  settings.numbers.push_back(10);
  const State state = newGame(3, settings);
  int homes = 0, faceDown = 0;
  for (const Planet &planet : state.planets) {
    homes += planet.homeOf >= 0;
    faceDown += !planet.flipped;
  }
  EXPECT_EQ(homes, 3);
  EXPECT_EQ(faceDown, 16);
  EXPECT_EQ(state.ships[2].at, (Hex{-2, 4}));
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}).settings.homeHexes, settings.homeHexes);
}

// Homes are spread evenly for any number of players, and every planet slot
// that isn't a home has a tile and a number to draw
TEST(RulesTest, HomesSpreadEvenlyForEachCount) {
  for (int players = 2; players <= 4; ++players) {
    const State state = newGame(players);
    int homes = 0, faceDown = 0;
    for (const Planet &planet : state.planets) {
      homes += planet.homeOf >= 0;
      faceDown += !planet.flipped;
      EXPECT_FALSE(planet.flipped && planet.homeOf < 0) << "an empty home";
    }
    EXPECT_EQ(homes, players);
    EXPECT_EQ(faceDown, 19 - players);
    int tiles = 0;
    for (int n : state.kindsLeft)
      tiles += n;
    EXPECT_EQ(tiles, faceDown);
    EXPECT_EQ(static_cast<int>(state.numbersLeft.size()), faceDown);
    // Every pair of homes the same distance apart as every other (the even spacing)
    std::set<int> gaps;
    for (int a = 0; a < players; ++a)
      gaps.insert(HexGrid::distance(state.ships[static_cast<std::size_t>(a)].at,
                                    state.ships[static_cast<std::size_t>((a + 1) % players)].at));
    EXPECT_LE(gaps.size(), players == 4 ? 2u : 1u);
  }
}

// A player behind takes an extra resource with their home's income
TEST(RulesTest, CatchingUp) {
  Settings settings;
  settings.catchUpGap = 2;
  State state = newGame(2, settings);
  EXPECT_EQ(state.homeIncomeLeft, 1); // level at the start
  state.player[0].badgePoints = 2;    // player 0 two ahead
  Game::apply(state, {Kind::ChooseHome, 0});
  Game::apply(state, {Kind::Roll, 1, 2});
  Game::apply(state, {Kind::EndTurn});
  EXPECT_EQ(state.homeIncomeLeft, 2); // player 1, behind
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 1}));
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 2}));
  EXPECT_EQ(state.phase, Phase::RollDraw);
  EXPECT_EQ(state.player[1].resources, (Resources{1, 2, 2, 1, 1}));
}

// On a 7, a player over the hand limit gives back half, from what they hold
// most of; a player at the limit keeps everything
TEST(RulesTest, AHandLimitOnASeven) {
  Settings settings;
  settings.discardOver = 7;
  State state = newGame(2, settings);
  state.player[0].resources = {5, 2, 1, 1, 1};            // 10 (with the home's 1 to come)
  state.player[1].resources = {3, 1, 1, 1, 1};            // 7: at the limit
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 4})); // a crystal: 11
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 3, 4}));
  EXPECT_EQ(state.phase, Phase::Blockade);
  // 11 held, {5, 2, 1, 1, 2}: 5 given back, one at a time from the most:
  // minerals to 2, then the first of the tied 2s each time
  EXPECT_EQ(state.player[0].resources, (Resources{1, 1, 1, 1, 2}));
  EXPECT_EQ(state.player[1].resources, (Resources{3, 1, 1, 1, 1}));
}

TEST(RulesTest, AHandLimitIsKeptInTheLog) {
  Settings settings;
  settings.discardOver = 7;
  const State state = newGame(2, settings);
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}).settings.discardOver, 7);
}

// The agreed deck: 25 cards, 14 Enforcers and 11 Charters
TEST(RulesTest, TheDeck) {
  const State state = newGame(4);
  EXPECT_EQ(state.deck.size(), 25u);
  EXPECT_EQ(std::count(state.deck.begin(), state.deck.end(), kEnforcerCard), 14);
}

namespace {
bool canPlay(const State &state) {
  const auto legal = Game::legalActions(state);
  return std::find(legal.begin(), legal.end(), Action{Action::Kind::PlayEnforcer}) != legal.end();
}
// Player 0's turn, in its main part (a roll of 3: nothing pays)
State mainPartOfATurn() {
  State state = newGame(3);
  Game::apply(state, {Action::Kind::ChooseHome, 0});
  Game::apply(state, {Action::Kind::Roll, 1, 2});
  return state;
}
} // namespace

// An Enforcer bought this turn waits for the next; then it moves the
// blockade and steals, as on a 7, one a turn
TEST(RulesTest, AnEnforcer) {
  State state = mainPartOfATurn();
  state.player[0].resources = {0, 1, 0, 1, 1}; // a card's cost: food, water, crystals
  ASSERT_TRUE(Game::apply(state, {Kind::BuyContract}));
  ASSERT_TRUE(Game::apply(state, {Kind::DealContracts, kEnforcerCard, kEnforcerCard}));
  ASSERT_TRUE(Game::apply(state, {Kind::KeepContract, kEnforcerCard}));
  EXPECT_FALSE(canPlay(state)); // bought this turn
  EXPECT_FALSE(Game::apply(state, {Kind::PlayEnforcer}));

  state.player[0].enforcersNew = 0; // as at the start of their next turn
  ASSERT_TRUE(canPlay(state));
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  EXPECT_EQ(state.phase, Phase::Blockade);
  EXPECT_EQ(state.player[0].enforcersPlayed, 1);
  EXPECT_EQ(
      std::count(state.player[0].contracts.begin(), state.player[0].contracts.end(), kEnforcerCard),
      0);
}

TEST(RulesTest, OneEnforcerATurn) {
  State state = mainPartOfATurn();
  state.player[0].contracts = {kEnforcerCard, kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  // No planet is turned over yet, so the blockade has nowhere to go and
  // there's no one to steal from: straight back to the main part
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, -1, -1}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_FALSE(canPlay(state));
}

// The Galactic Embargo: 3 Enforcers played claim it (2 points); a tie
// keeps it; more takes it
TEST(RulesTest, TheGalacticEmbargo) {
  State state = mainPartOfATurn();
  const int before = points(state, 0);
  state.player[0].enforcersPlayed = 2;
  state.player[0].contracts = {kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  EXPECT_EQ(state.embargoHolder, 0);
  EXPECT_EQ(points(state, 0), before + 2);
  state.player[1].enforcersPlayed = 3; // a tie: still player 0's
  EXPECT_EQ(state.embargoHolder, 0);
}

TEST(RulesTest, TheFirstPlayerCanSkipTheirFirstIncome) {
  Settings settings;
  settings.firstSkipsIncome = true;
  const State state = newGame(2, settings, 1);
  EXPECT_EQ(state.phase, Phase::RollDraw);
  EXPECT_EQ(state.player[1].resources, (Resources{1, 1, 1, 1, 1}));
  const auto again = Game::readState(Game::writeState(state));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}), state);
}

TEST(RulesTest, ATurn) {
  State state = newGame(2);
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, static_cast<int>(Resource::Water)}));
  EXPECT_EQ(state.player[0].resources[static_cast<std::size_t>(Resource::Water)], 2);
  EXPECT_EQ(Game::currentPlayer(state), GameBots::kDealer); // the roll
  EXPECT_FALSE(Game::apply(state, {Kind::EndTurn}));        // not yet
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 2, 3}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_TRUE(has(Game::legalActions(state), {Kind::EndTurn}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_EQ(state.turn, 1);
  EXPECT_EQ(state.phase, Phase::HomeIncome);
  ASSERT_TRUE(Game::apply(state, {Kind::ChooseHome, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Roll, 1, 1}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_EQ(state.turn, 0);
  EXPECT_EQ(state.round, 2);
}

// A roll pays each station on a turned-over planet with that number: 1, or
// 2 for a colony; a blockaded planet pays nothing
TEST(RulesTest, Production) {
  State state = newGame(2);
  const int planet = planetIndex(state, Hex{0, 0});
  flip(state, planet, PlanetKind::GasGiant, 8);
  state.planets[static_cast<std::size_t>(planet)].docks = {0, 1};
  state.planets[static_cast<std::size_t>(planet)].colony = {false, true};
  Game::apply(state, {Kind::ChooseHome, 0});
  Game::apply(state, {Kind::Roll, 4, 4});
  EXPECT_EQ(state.player[0].resources[static_cast<std::size_t>(Resource::Fuel)], 2);
  EXPECT_EQ(state.player[1].resources[static_cast<std::size_t>(Resource::Fuel)], 3);
  // Blockaded, it pays nothing
  State blocked = newGame(2);
  flip(blocked, planet, PlanetKind::GasGiant, 8);
  blocked.planets[static_cast<std::size_t>(planet)].docks = {0, -1};
  blocked.blockade = planet;
  Game::apply(blocked, {Kind::ChooseHome, 0});
  Game::apply(blocked, {Kind::Roll, 4, 4});
  EXPECT_EQ(blocked.player[0].resources[static_cast<std::size_t>(Resource::Fuel)], 1);
}

// A 7 moves the blockade ship, and its mover takes a resource from someone
// with a station there (the dealer picks which)
TEST(RulesTest, TheBlockade) {
  State state = newGame(2);
  Game::apply(state, {Kind::ChooseHome, 0});
  Game::apply(state, {Kind::Roll, 3, 4});
  ASSERT_EQ(state.phase, Phase::Blockade);
  // Nothing turned over yet: nowhere to put it
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::MoveBlockade, -1, -1}}));

  State later = newGame(2);
  const int planet = planetIndex(later, Hex{0, 0});
  flip(later, planet, PlanetKind::IceMoon, 5);
  later.planets[static_cast<std::size_t>(planet)].docks = {1, -1};
  Game::apply(later, {Kind::ChooseHome, 0});
  Game::apply(later, {Kind::Roll, 3, 4});
  EXPECT_EQ(Game::legalActions(later), (std::vector<Action>{{Kind::MoveBlockade, planet, 1}}));
  ASSERT_TRUE(Game::apply(later, {Kind::MoveBlockade, planet, 1}));
  EXPECT_EQ(later.blockade, planet);
  EXPECT_EQ(Game::currentPlayer(later), GameBots::kDealer);
  ASSERT_TRUE(Game::apply(later, {Kind::Steal, static_cast<int>(Resource::Food)}));
  EXPECT_EQ(later.player[1].resources[static_cast<std::size_t>(Resource::Food)], 0);
  EXPECT_EQ(later.player[0].resources[static_cast<std::size_t>(Resource::Food)], 2);
  EXPECT_EQ(later.phase, Phase::Main);
}

// Ships move up to 3 hexes, 5 for a fuel; onto a face-down tile they stop
// and it's turned over; they can't pass through one
TEST(RulesTest, MovingAndTurningTilesOver) {
  State state = atMain();
  const Ship &ship = state.ships[0];
  ASSERT_EQ(ship.at, (Hex{4, -2}));
  const std::vector<Action> legal = Game::legalActions(state);
  // The face-down planet 2 hexes in can be moved onto
  const Hex faceDownPlanet{2, -1};
  ASSERT_GE(planetIndex(state, faceDownPlanet), 0);
  EXPECT_TRUE(has(legal, {Kind::Move, 0, faceDownPlanet.q, faceDownPlanet.r}));
  // Beyond 5 hexes, never
  for (const Action &action : legal) {
    if (action.kind == Kind::Move) {
      EXPECT_LE(HexGrid::distance(ship.at, Hex{action.b, action.c}), 5);
    }
  }
  // A move of 4 or more hexes costs a fuel. (Early on, face-down tiles hem
  // a ship in, so this is on a board with every tile turned over.)
  State open = state;
  for (Planet &planet : open.planets)
    planet.flipped = true;
  for (ZoneSite &site : open.zone)
    site.flipped = true;
  const std::vector<Action> openMoves = Game::legalActions(open);
  const auto far = std::find_if(openMoves.begin(), openMoves.end(), [&](const Action &action) {
    return action.kind == Kind::Move && HexGrid::distance(ship.at, Hex{action.b, action.c}) >= 4;
  });
  ASSERT_NE(far, openMoves.end());
  ASSERT_TRUE(Game::apply(open, *far));
  EXPECT_EQ(open.player[0].resources[static_cast<std::size_t>(Resource::Fuel)], 0);
  // And with no fuel, nothing beyond 3
  open = state;
  for (Planet &planet : open.planets)
    planet.flipped = true;
  for (ZoneSite &site : open.zone)
    site.flipped = true;
  open.player[0].resources[static_cast<std::size_t>(Resource::Fuel)] = 0;
  for (const Action &action : Game::legalActions(open)) {
    if (action.kind == Kind::Move) {
      EXPECT_LE(HexGrid::distance(ship.at, Hex{action.b, action.c}), 3);
    }
  }
  // Onto the face-down planet: it's turned over by the dealer, with a badge
  ASSERT_TRUE(Game::apply(state, {Kind::Move, 0, faceDownPlanet.q, faceDownPlanet.r}));
  EXPECT_EQ(state.phase, Phase::PlanetDraw);
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::EndTurn}));
  const std::size_t numbersBefore = state.numbersLeft.size();
  ASSERT_TRUE(Game::apply(state, {Kind::RevealPlanet, static_cast<int>(PlanetKind::Nebula), 6}));
  const Planet &planet =
      state.planets[static_cast<std::size_t>(planetIndex(state, faceDownPlanet))];
  EXPECT_TRUE(planet.flipped);
  EXPECT_EQ(planet.kind, PlanetKind::Nebula);
  EXPECT_EQ(planet.number, 6);
  EXPECT_EQ(state.player[0].badges, 1);
  EXPECT_EQ(state.kindsLeft[static_cast<std::size_t>(PlanetKind::Nebula)], 2);
  EXPECT_EQ(state.numbersLeft.size(), numbersBefore - 1);
  // The ship has moved this turn
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::Move));
}

TEST(RulesTest, SettlingAndColonies) {
  State state = atMain();
  const int planet = planetIndex(state, Hex{2, -1});
  flip(state, planet, PlanetKind::LushPlanet, 9);
  state.ships[0].at = Hex{2, -1};
  ASSERT_TRUE(has(Game::legalActions(state), {Kind::Settle, 0}));
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, 0}));
  EXPECT_TRUE(state.ships.empty() || state.ships[0].owner != 0); // the ship became the station
  EXPECT_EQ(state.planets[static_cast<std::size_t>(planet)].docks[0], 0);
  EXPECT_EQ(points(state, 0), 2);
  // A colony needs 2 minerals and 2 food
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::BuildColony));
  state.player[0].resources = {2, 2, 0, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::BuildColony, planet}));
  EXPECT_EQ(points(state, 0), 3);
}

// A second player can share a planet; a third can't (two docks)
TEST(RulesTest, TwoStationsAPlanet) {
  State state = atMain(3);
  const int planet = planetIndex(state, Hex{2, -1});
  flip(state, planet, PlanetKind::LushPlanet, 9);
  state.planets[static_cast<std::size_t>(planet)].docks = {1, 2};
  state.ships[0].at = Hex{2, -1};
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::Settle));
}

// Core Control: 2 points to whoever first has 2 core stations, passing only
// to someone with more
TEST(RulesTest, CoreControl) {
  State state = atMain();
  const int a = planetIndex(state, Hex{0, 0}), b = planetIndex(state, Hex{1, 1});
  ASSERT_TRUE(state.planets[static_cast<std::size_t>(a)].core &&
              state.planets[static_cast<std::size_t>(b)].core);
  flip(state, a, PlanetKind::GasGiant, 4);
  flip(state, b, PlanetKind::IceMoon, 10);
  state.player[0].resources = {0, 5, 0, 5, 0};
  state.ships[0].at = Hex{0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, 0}));
  EXPECT_EQ(state.coreHolder, -1); // one isn't enough
  state.ships.push_back(Ship{0, Hex{1, 1}, false});
  ASSERT_TRUE(Game::apply(state, {Kind::Settle, static_cast<int>(state.ships.size()) - 1}));
  EXPECT_EQ(state.coreHolder, 0);
  EXPECT_EQ(points(state, 0), 1 + 2 + 2);
}

// Contracts: bought, two dealt, one kept (the other back in the deck),
// revealed when met, scored at once
TEST(RulesTest, Contracts) {
  State state = atMain();
  state.player[0].resources = {0, 1, 0, 1, 1};
  ASSERT_TRUE(Game::apply(state, {Kind::BuyContract}));
  EXPECT_EQ(Game::currentPlayer(state), GameBots::kDealer);
  const std::size_t deckBefore = state.deck.size();
  // Contract 9: "6 resources in hand"
  ASSERT_TRUE(Game::apply(state, {Kind::DealContracts, 9, 4}));
  ASSERT_TRUE(Game::apply(state, {Kind::KeepContract, 9}));
  EXPECT_EQ(state.deck.size(), deckBefore - 1);
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::RevealContract, 9})); // not met yet
  state.player[0].resources = {2, 2, 2, 0, 0};
  ASSERT_TRUE(has(Game::legalActions(state), {Kind::RevealContract, 9}));
  const int before = points(state, 0);
  ASSERT_TRUE(Game::apply(state, {Kind::RevealContract, 9}));
  EXPECT_EQ(points(state, 0), before + contracts()[9].points);
}

TEST(RulesTest, BadgesCashInForPoints) {
  State state = atMain();
  state.player[0].badges = 3;
  ASSERT_TRUE(Game::apply(state, {Kind::CashBadges}));
  EXPECT_EQ(state.player[0].badges, 0);
  EXPECT_EQ(points(state, 0), 2);
}

TEST(RulesTest, TheBankTradesFourForOne) {
  State state = atMain();
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::Trade));
  state.player[0].resources = {4, 0, 0, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::Trade, 0, 4}));
  EXPECT_EQ(state.player[0].resources, (Resources{0, 0, 0, 0, 1}));
}

// The zone's finds: salvage lets you pick, ruins give badges, the pirates
// steal from everyone else
TEST(RulesTest, ZoneFinds) {
  State state = atMain();
  state.ships[0].at = state.zone[0].hex;
  state.pendingSite = 0;
  state.phase = Phase::ZoneDraw;
  ASSERT_TRUE(Game::apply(state, {Kind::RevealFind, static_cast<int>(Find::Salvage)}));
  EXPECT_EQ(state.phase, Phase::Salvage);
  ASSERT_TRUE(Game::apply(state, {Kind::TakeSalvage, 2}));
  ASSERT_TRUE(Game::apply(state, {Kind::TakeSalvage, 2}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.player[0].resources[2], 3);
  EXPECT_EQ(state.player[0].findsKept, 1);

  State pirates = atMain(3);
  pirates.pendingSite = 1;
  pirates.phase = Phase::ZoneDraw;
  ASSERT_TRUE(Game::apply(pirates, {Kind::RevealFind, static_cast<int>(Find::PirateHideout)}));
  EXPECT_EQ(pirates.victims, (std::vector<int>{1, 2}));
  ASSERT_TRUE(Game::apply(pirates, {Kind::Steal, 0}));
  ASSERT_TRUE(Game::apply(pirates, {Kind::Steal, 1}));
  EXPECT_EQ(pirates.phase, Phase::Main);
  EXPECT_EQ(pirates.player[0].resources[0], 3); // 1, the home's 1, and 1 stolen
}

TEST(RulesTest, TwelvePointsOnYourTurnWins) {
  State state = atMain();
  state.player[0].badgePoints = 10;
  state.player[0].badges = 3;
  ASSERT_TRUE(Game::apply(state, {Kind::CashBadges})); // the home's 1 + 11 = 12
  EXPECT_EQ(state.winner, 0);
  EXPECT_TRUE(Game::isOver(state));
  EXPECT_TRUE(Game::legalActions(state).empty());
  EXPECT_EQ(Game::score(state, 0), 1.0);
  EXPECT_EQ(Game::score(state, 1), 0.0);
}

// A bot can't see the others' contracts: a guess deals them again from the
// unseen ones, keeping its own and how many each holds
TEST(RulesTest, OthersContractsAreHidden) {
  State state = newGame(3);
  state.player[0].contracts = {1};
  state.player[1].contracts = {2, 3};
  state.player[2].contracts = {4};
  std::erase_if(state.deck, [](int id) { return id >= 1 && id <= 4; });
  SeededRandom::Generator random(5);
  bool changed = false;
  for (int i = 0; i < 20; ++i) {
    const State guess = Game::determinize(state, 0, random);
    EXPECT_EQ(guess.player[0].contracts, state.player[0].contracts);
    EXPECT_EQ(guess.player[1].contracts.size(), 2u);
    EXPECT_EQ(guess.player[2].contracts.size(), 1u);
    EXPECT_EQ(guess.deck.size(), state.deck.size());
    changed = changed || guess.player[1].contracts != state.player[1].contracts;
  }
  EXPECT_TRUE(changed);
}

// The starting state writes and reads back; every action writes and reads back
TEST(RulesTest, Text) {
  Settings settings;
  settings.winPoints = 12;
  const State start = newGame(4, settings);
  const auto again = Game::readState(Game::writeState(start));
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again.value_or(State{}), start);
  EXPECT_FALSE(Game::readState(Game::writeState(atMain())).has_value()); // not a starting state

  for (const Action &action : std::vector<Action>{{Kind::ChooseHome, 3},
                                                  {Kind::Roll, 2, 6},
                                                  {Kind::MoveBlockade, 7, -1},
                                                  {Kind::Steal, 4},
                                                  {Kind::Trade, 0, 1},
                                                  {Kind::BuildShip, 18},
                                                  {Kind::Move, 2, -3, 1},
                                                  {Kind::RevealPlanet, 4, 11},
                                                  {Kind::RevealFind, 3},
                                                  {Kind::TakeSalvage, 1},
                                                  {Kind::Settle, 0},
                                                  {Kind::BuildColony, 5},
                                                  {Kind::BuyContract},
                                                  {Kind::DealContracts, 3, -1},
                                                  {Kind::KeepContract, 3},
                                                  {Kind::RevealContract, 16},
                                                  {Kind::CashBadges},
                                                  {Kind::EndTurn}}) {
    const auto read = Game::readAction(Game::writeAction(action));
    ASSERT_TRUE(read.has_value()) << Game::writeAction(action);
    EXPECT_EQ(read.value_or(Action{}), action) << Game::writeAction(action);
  }
}

// Bots play a whole game to a winner; its log replays exactly
TEST(RulesTest, BotsPlayAWholeGame) {
  SeededRandom::Seed seed(2026);
  SeededRandom::Generator dealer = seed.stream("dealer"), botRandom = seed.stream("bots");
  GameBots::Ismcts<Game> bot({.rolloutDepth = 60});
  const auto choose = [&](const State &state) {
    return bot.choose(state, botRandom, {.iterations = 40});
  };
  ActionLog::Play<Game> play(newGame(2));
  GameBots::playMatch<Game>(play, {choose, choose}, dealer, 20000);
  EXPECT_TRUE(Game::isOver(play.state()))
      << "after " << play.log().actions.size() << " actions, round " << play.state().round;
  const auto log = ActionLog::read<Game>(ActionLog::write(play.log()));
  ASSERT_TRUE(log.has_value());
  const ActionLog::Replay<Game> replay = ActionLog::replay(log.value_or(ActionLog::Log<Game>{}));
  EXPECT_TRUE(replay.ok());
  EXPECT_EQ(replay.state, play.state());
}

// A layout names the planets (round 44's cluster boards): exactly those and
// the homes, in board order; the zone's sites leave them out; and a layout
// reads back from its settings line
TEST(RulesTest, ALayoutNamesThePlanets) {
  Settings settings;
  settings.planetHexes = {{0, 0}, {1, -3}, {2, -3}, {1, -2}, {-1, 3}, {-2, 3}, {-1, 2}};
  const State state = newGame(4, settings);
  ASSERT_EQ(state.planets.size(), 7u + 4u); // and the four homes
  for (const Planet &planet : state.planets) {
    const bool named = std::find(settings.planetHexes.begin(), settings.planetHexes.end(),
                                 planet.hex) != settings.planetHexes.end();
    EXPECT_TRUE(named || planet.homeOf >= 0);
  }
  for (const ZoneSite &site : state.zone) {
    EXPECT_EQ(std::find(settings.planetHexes.begin(), settings.planetHexes.end(), site.hex),
              settings.planetHexes.end());
  }
  const auto read = Game::readState(Game::writeState(state));
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read.value_or(State{}).settings.planetHexes, settings.planetHexes);
}

// A moon: drawn from the pile like the others, with no number; it
// still earns a badge, can hold a station, and never pays
TEST(RulesTest, Moons) {
  Settings settings;
  settings.moons = 2;
  State state = newGame(4, settings);
  EXPECT_EQ(state.moonsLeft, 2);
  const auto read = Game::readState(Game::writeState(state));
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read.value_or(State{}).settings.moons, 2);

  state = atMain();
  state.moonsLeft = 1;
  const int planet = 0; // (any face-down planet)
  ASSERT_FALSE(state.planets[planet].flipped);
  state.pendingPlanet = planet;
  state.phase = Phase::PlanetDraw;
  const int badges = state.player[0].badgesEarned;
  // A moon with a number is refused
  EXPECT_FALSE(Game::apply(state, {Kind::RevealPlanet, static_cast<int>(PlanetKind::Moon), 6}));
  ASSERT_TRUE(Game::apply(state, {Kind::RevealPlanet, static_cast<int>(PlanetKind::Moon), 0}));
  EXPECT_EQ(state.planets[planet].kind, PlanetKind::Moon);
  EXPECT_EQ(state.moonsLeft, 0);
  EXPECT_EQ(state.player[0].badgesEarned, badges + 1);
  // None left: refused
  state.pendingPlanet = 1;
  state.phase = Phase::PlanetDraw;
  EXPECT_FALSE(Game::apply(state, {Kind::RevealPlanet, static_cast<int>(PlanetKind::Moon), 0}));
  // A station on it pays nothing, whatever's rolled
  state.planets[planet].docks[0] = 1;
  const Resources before = state.player[1].resources;
  for (int roll = 2; roll <= 12; ++roll) {
    if (roll == 7)
      continue; // (the blockade)
    state.phase = Phase::RollDraw;
    ASSERT_TRUE(
        Game::apply(state, {Kind::Roll, roll <= 6 ? 1 : 6, roll <= 6 ? roll - 1 : roll - 6}));
  }
  EXPECT_EQ(state.player[1].resources, before);
  // The action reads back
  EXPECT_EQ(Game::readAction(Game::writeAction({Kind::RevealPlanet, 6, 0})),
            std::make_optional(Action{Kind::RevealPlanet, 6, 0}));
}

// The dealer draws moons from the pile in proportion
TEST(RulesTest, TheDealerDrawsMoons) {
  State state = atMain();
  state.kindsLeft = {1, 0, 0, 0, 0};
  state.moonsLeft = 1;
  state.pendingPlanet = 0;
  state.phase = Phase::PlanetDraw;
  SeededRandom::Generator random(7);
  int moons = 0;
  for (int i = 0; i < 400; ++i) {
    const Action drawn = Game::dealerAction(state, random);
    if (drawn.a == static_cast<int>(PlanetKind::Moon)) {
      ++moons;
      EXPECT_EQ(drawn.b, 0);
    }
  }
  EXPECT_GT(moons, 150);
  EXPECT_LT(moons, 250);
}

// Bot styles: named and read back; each leans its moves its own way
TEST(BotStylesTest, NamesAndLeanings) {
  for (BotStyle style : {BotStyle::Plain, BotStyle::Saver, BotStyle::Rusher, BotStyle::Explorer})
    EXPECT_EQ(botStyleNamed(nameOf(style)), std::make_optional(style));
  EXPECT_FALSE(botStyleNamed("hoarder").has_value());
  const Action endTurn{Kind::EndTurn}, ship{Kind::BuildShip}, move{Kind::Move};
  EXPECT_GT(styleWeight(BotStyle::Saver, endTurn), styleWeight(BotStyle::Saver, ship));
  EXPECT_GT(styleWeight(BotStyle::Rusher, ship), styleWeight(BotStyle::Rusher, endTurn));
  EXPECT_GT(styleWeight(BotStyle::Explorer, move), styleWeight(BotStyle::Explorer, endTurn));
  EXPECT_FALSE(rolloutWeightFor(BotStyle::Plain)); // (plain: the usual random rollouts)
  EXPECT_TRUE(rolloutWeightFor(BotStyle::Saver));
}

// A layout can name where the zone's finds are (beside the small systems,
// say), and it reads back
TEST(RulesTest, ALayoutNamesTheZone) {
  Settings settings;
  settings.zoneHexes = {{3, -1}, {-3, 1}, {2, 0}, {-2, 0}}; // (none a planet)
  const State state = newGame(4, settings);
  ASSERT_EQ(state.zone.size(), 4u);
  for (const ZoneSite &site : state.zone) {
    EXPECT_NE(std::find(settings.zoneHexes.begin(), settings.zoneHexes.end(), site.hex),
              settings.zoneHexes.end());
  }
  const auto read = Game::readState(Game::writeState(state));
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read.value_or(State{}).settings.zoneHexes, settings.zoneHexes);
}
