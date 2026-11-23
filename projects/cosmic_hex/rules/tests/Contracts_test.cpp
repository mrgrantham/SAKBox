// Contracts (Contracts.cpp): each Charter met and not met, revealing one
// (once), buying and keeping cards, the action cards (Enforcer,
// Interstellar Cartel, Shipyard Surplus, Supply drop: one a turn, not the
// turn bought), and the Galactic Embargo for the most Enforcers played.
#include <CosmicHex/Rules.hpp>

#include "TestGames.hpp"

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <utility>

using namespace CosmicHex;
using namespace CosmicHex::TestGames;
using Kind = Action::Kind;

namespace {

bool met(const State &state, int charter, int player = 0) {
  return contracts().at(static_cast<std::size_t>(charter)).met(state, player);
}

// A station of the player's (a colony if asked) on the planet at `hex`,
// turned over as `kind` with `number` if it's still face down (on hexes)
void stationOn(State &state, Hex hex, PlanetKind kind, int player = 0, bool colony = false,
               int number = 10) {
  const int planet = planetIndex(state, hex);
  ASSERT_GE(planet, 0) << "no planet at " << hex.q << "," << hex.r;
  if (!state.planets[static_cast<std::size_t>(planet)].flipped)
    flip(state, planet, kind, number);
  dock(state, planet, player, colony);
}

// Planets on hexes, none of them a home or a core planet
constexpr std::array<Hex, 6> kOuter{{{3, 0}, {0, 3}, {-3, 0}, {0, -3}, {3, -3}, {-3, 3}}};

// Player `player` plays an Enforcer in their main part; the blockade has
// nowhere to go yet (nothing is turned over)
void playEnforcer(State &state, int player) {
  state.turn = player;
  state.phase = Phase::Main;
  state.cardPlayed = false;
  state.player[static_cast<std::size_t>(player)].contracts.push_back(kEnforcerCard);
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, -1, -1}));
  ASSERT_EQ(state.phase, Phase::Main);
}

} // namespace

// The Charters' ids are their places in contracts(): the deck's three are
// 21, 19 and 20, and each scores its table's points (the relay its setting)
TEST(ContractsTest, TheChartersAreNumberedByTheirPlace) {
  ASSERT_EQ(contracts().size(), 22u);
  EXPECT_EQ(Settings{}.charters, (std::vector<int>{21, 19, kDeepSpaceRelay}));
  EXPECT_EQ(contracts()[21].name, "Stations on 3 planets of one kind");
  EXPECT_EQ(contracts()[19].name.substr(0, 9), "Occupier:");
  EXPECT_EQ(contracts()[kDeepSpaceRelay].name.substr(0, 16), "Deep-space relay");
  for (int id = 0; id < static_cast<int>(contracts().size()); ++id) {
    EXPECT_TRUE(isCharter(id));
    if (id != kDeepSpaceRelay) {
      EXPECT_EQ(charterPoints(Settings{}, id), contracts()[static_cast<std::size_t>(id)].points);
    }
  }
  for (int card : {kEnforcerCard, kCartelCard, kShipyardCard, kSupplyDropCard, -1, 22})
    EXPECT_FALSE(isCharter(card));
}

// Stations on 3, 4 and all 5 kinds of planet: kinds counted once each;
// homes and moons are no kind
TEST(ContractsTest, KindsOfPlanet) {
  State state = newGame(2);
  stationOn(state, kOuter[0], PlanetKind::AsteroidBelt);
  stationOn(state, kOuter[1], PlanetKind::AsteroidBelt);
  stationOn(state, kOuter[2], PlanetKind::LushPlanet);
  stationOn(state, kOuter[3], PlanetKind::Moon, 0, false, 0);
  EXPECT_FALSE(met(state, 0)); // two kinds, a moon and the home
  stationOn(state, kOuter[4], PlanetKind::GasGiant);
  EXPECT_TRUE(met(state, 0));
  EXPECT_FALSE(met(state, 18));
  stationOn(state, kOuter[5], PlanetKind::IceMoon);
  EXPECT_TRUE(met(state, 18));
  EXPECT_FALSE(met(state, 14));
  stationOn(state, Hex{2, 2}, PlanetKind::Nebula);
  EXPECT_TRUE(met(state, 14));
  EXPECT_FALSE(met(state, 14, 1)); // (another player's)
}

// Stations on 2 of a named kind (gas giants, ice moons, nebulae, asteroid
// belts, lush planets), or 2 of any one kind
TEST(ContractsTest, TwoOfAKind) {
  for (const auto [charter, kind] :
       {std::pair{1, PlanetKind::GasGiant}, std::pair{11, PlanetKind::IceMoon},
        std::pair{12, PlanetKind::Nebula}, std::pair{15, PlanetKind::AsteroidBelt},
        std::pair{16, PlanetKind::LushPlanet}}) {
    State state = newGame(2);
    stationOn(state, kOuter[0], kind);
    stationOn(state, kOuter[1],
              kind == PlanetKind::GasGiant ? PlanetKind::Nebula : PlanetKind::GasGiant);
    EXPECT_FALSE(met(state, charter)) << charter;
    EXPECT_FALSE(met(state, 17)) << charter;
    stationOn(state, kOuter[2], kind);
    EXPECT_TRUE(met(state, charter)) << charter;
    EXPECT_TRUE(met(state, 17)) << charter;
  }
}

// Stations on 3 planets of one kind (today's deck): not 3 kinds
TEST(ContractsTest, ThreeOfOneKind) {
  State state = newGame(2);
  stationOn(state, kOuter[0], PlanetKind::GasGiant);
  stationOn(state, kOuter[1], PlanetKind::GasGiant);
  stationOn(state, kOuter[2], PlanetKind::IceMoon);
  EXPECT_FALSE(met(state, 21));
  stationOn(state, kOuter[3], PlanetKind::GasGiant);
  EXPECT_TRUE(met(state, 21));
}

// Two colonies built (the home not one); a colony on a 6 or 8
TEST(ContractsTest, Colonies) {
  State state = newGame(2);
  stationOn(state, kOuter[0], PlanetKind::GasGiant, 0, true, 6);
  EXPECT_FALSE(met(state, 2));
  EXPECT_TRUE(met(state, 10));
  stationOn(state, kOuter[1], PlanetKind::GasGiant, 0, true, 5);
  EXPECT_TRUE(met(state, 2));
  // A station (not a colony) on an 8, or a colony on a 5, isn't met
  State other = newGame(2);
  stationOn(other, kOuter[0], PlanetKind::GasGiant, 0, false, 8);
  stationOn(other, kOuter[1], PlanetKind::GasGiant, 0, true, 5);
  EXPECT_FALSE(met(other, 10));
}

// A station on a core planet
TEST(ContractsTest, ACoreStation) {
  State state = newGame(2);
  stationOn(state, kOuter[0], PlanetKind::GasGiant);
  EXPECT_FALSE(met(state, 3));
  stationOn(state, Hex{0, 0}, PlanetKind::GasGiant);
  EXPECT_TRUE(met(state, 3));
}

// The player's own counts: 6 badges earned, 2 finds kept, 6 resources held,
// 3 ships in play
TEST(ContractsTest, CountsOfThePlayersOwn) {
  State state = newGame(2);
  state.player[0].resources = {1, 1, 1, 1, 1};
  state.player[0].badgesEarned = 5;
  state.player[0].findsKept = 1;
  state.ships.push_back(Ship{0, Hex{0, 0}, false}); // (two, with the first)
  for (int charter : {4, 5, 9, 13})
    EXPECT_FALSE(met(state, charter)) << charter;
  state.player[0].resources[0] = 2;
  state.player[0].badgesEarned = 6;
  state.player[0].findsKept = 2;
  state.ships.push_back(Ship{0, Hex{0, 0}, false});
  for (int charter : {4, 5, 9, 13})
    EXPECT_TRUE(met(state, charter)) << charter;
}

// Stations on opposite sides: 6 or more hexes apart, the home counting
TEST(ContractsTest, OppositeSides) {
  State state = newGame(2); // (player 0's home at 4,-2)
  stationOn(state, Hex{2, -1}, PlanetKind::GasGiant);
  EXPECT_FALSE(met(state, 6));
  stationOn(state, Hex{-3, 0}, PlanetKind::GasGiant); // 7 from the home
  EXPECT_TRUE(met(state, 6));
}

// Sharing 2 planets with other players
TEST(ContractsTest, SharingTwoPlanets) {
  State state = newGame(3);
  stationOn(state, kOuter[0], PlanetKind::GasGiant, 0);
  stationOn(state, kOuter[0], PlanetKind::GasGiant, 1);
  stationOn(state, kOuter[1], PlanetKind::GasGiant, 0);
  EXPECT_FALSE(met(state, 7));
  stationOn(state, kOuter[1], PlanetKind::GasGiant, 2);
  EXPECT_TRUE(met(state, 7));
  EXPECT_FALSE(met(state, 7, 1)); // (one shared)
}

// A station next to a turned-over alien station (on hexes: a hex away)
TEST(ContractsTest, NextToTheAlienStation) {
  State state = newGame(2);
  stationOn(state, Hex{2, -1}, PlanetKind::GasGiant);
  const int site = siteIndex(state, Hex{2, -2});
  ASSERT_GE(site, 0);
  state.zone[static_cast<std::size_t>(site)].find = Find::AlienStation;
  EXPECT_FALSE(met(state, 8)); // (face down)
  state.zone[static_cast<std::size_t>(site)].flipped = true;
  EXPECT_TRUE(met(state, 8));
  state.zone[static_cast<std::size_t>(site)].find = Find::Ruins;
  EXPECT_FALSE(met(state, 8));
}

// Occupier: a station on another player's exhausted home, not one's own
TEST(ContractsTest, Occupier) {
  State state = newGame(2);
  for (Planet &planet : state.planets)
    if (planet.homeOf >= 0) {
      planet.formerHomeOf = planet.homeOf;
      planet.homeOf = -1;
      planet.kind = PlanetKind::GasGiant;
    }
  EXPECT_FALSE(met(state, 19)); // (player 0's own former home, docked at the start)
  stationOn(state, Hex{-4, 2}, PlanetKind::GasGiant); // player 1's
  EXPECT_TRUE(met(state, 19));
}

// Deep-space relay: a station touching no planet and no outpost
TEST(ContractsTest, TheDeepSpaceRelay) {
  State state = cornersAtMain();
  EXPECT_FALSE(met(state, kDeepSpaceRelay)); // (the home colony touches its home)
  std::optional<Corner> empty;
  for (Corner corner : boardCorners()) {
    bool byAFind = false;
    for (const ZoneSite &site : state.zone)
      byAFind = byAFind || touchesHex(corner, site.hex);
    if (planetsAt(state, corner) == 0 && !byAFind) {
      empty = corner;
      break;
    }
  }
  ASSERT_TRUE(empty.has_value());
  state.stations.push_back(Station{empty.value_or(Corner{}), 0, false});
  EXPECT_TRUE(met(state, kDeepSpaceRelay));
  EXPECT_FALSE(met(state, kDeepSpaceRelay, 1));
}

// A Charter met is revealed from the hand and scores at once; one not met,
// not held, or not a Charter can't be
TEST(ContractsTest, RevealingACharter) {
  State state = atMain();
  state.player[0].contracts = {21, kEnforcerCard};
  stationOn(state, kOuter[0], PlanetKind::GasGiant);
  stationOn(state, kOuter[1], PlanetKind::GasGiant);
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::RevealContract, 21}));
  EXPECT_FALSE(Game::apply(state, {Kind::RevealContract, 21})); // (not met)
  stationOn(state, kOuter[2], PlanetKind::GasGiant);
  EXPECT_FALSE(Game::apply(state, {Kind::RevealContract, 19}));            // (not held)
  EXPECT_FALSE(Game::apply(state, {Kind::RevealContract, kEnforcerCard})); // (not a Charter)
  ASSERT_TRUE(has(Game::legalActions(state), {Kind::RevealContract, 21}));
  const int before = points(state, 0);
  ASSERT_TRUE(Game::apply(state, {Kind::RevealContract, 21}));
  EXPECT_EQ(state.player[0].contracts, (std::vector<int>{kEnforcerCard}));
  EXPECT_EQ(state.player[0].revealed, (std::vector<int>{21}));
  EXPECT_EQ(points(state, 0), before + 2);
}

// A Charter isn't revealed twice, even holding a second copy; once revealed
// it scores though no longer met
TEST(ContractsTest, ACharterIsRevealedOnceAndKept) {
  State state = atMain();
  state.player[0].contracts = {9, 9}; // (6 resources in hand)
  state.player[0].resources = {2, 2, 2, 0, 0};
  ASSERT_TRUE(Game::apply(state, {Kind::RevealContract, 9}));
  EXPECT_FALSE(has(Game::legalActions(state), {Kind::RevealContract, 9}));
  EXPECT_FALSE(Game::apply(state, {Kind::RevealContract, 9}));
  const int scored = points(state, 0);
  state.player[0].resources = {0, 0, 0, 0, 0};
  EXPECT_EQ(points(state, 0), scored);
}

// Buying a card pays its cost; the dealer deals two, one is kept and the
// other goes under the deck
TEST(ContractsTest, BuyingDealsTwoAndOneIsKept) {
  State state = atMain();
  state.player[0].resources = {0, 1, 0, 1, 0}; // (no crystal)
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::BuyContract));
  EXPECT_FALSE(Game::apply(state, {Kind::BuyContract}));
  state.player[0].resources = {0, 1, 0, 1, 1};
  state.deck = {21, kCartelCard, kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::BuyContract}));
  EXPECT_EQ(state.player[0].resources, (Resources{0, 0, 0, 0, 0}));
  EXPECT_EQ(state.phase, Phase::ContractDraw);
  EXPECT_FALSE(Game::apply(state, {Kind::DealContracts, 19, 21}));          // (not in the deck)
  EXPECT_FALSE(Game::apply(state, {Kind::DealContracts, kCartelCard, -1})); // (two are left)
  ASSERT_TRUE(Game::apply(state, {Kind::DealContracts, kCartelCard, 21}));
  EXPECT_EQ(Game::legalActions(state),
            (std::vector<Action>{{Kind::KeepContract, kCartelCard}, {Kind::KeepContract, 21}}));
  EXPECT_FALSE(Game::apply(state, {Kind::KeepContract, kEnforcerCard}));
  ASSERT_TRUE(Game::apply(state, {Kind::KeepContract, kCartelCard}));
  EXPECT_EQ(state.player[0].contracts, (std::vector<int>{kCartelCard}));
  EXPECT_EQ(state.player[0].boughtThisTurn, (std::vector<int>{kCartelCard}));
  EXPECT_EQ(state.deck, (std::vector<int>{kEnforcerCard, 21})); // (21 under the deck)
  EXPECT_EQ(state.phase, Phase::Main);
}

// The deck's last card is dealt alone; an empty deck can't be bought from
TEST(ContractsTest, TheDecksLastCard) {
  State state = atMain();
  state.player[0].resources = {0, 2, 0, 2, 2};
  state.deck = {kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::BuyContract}));
  ASSERT_TRUE(Game::apply(state, {Kind::DealContracts, kEnforcerCard, -1}));
  ASSERT_TRUE(Game::apply(state, {Kind::KeepContract, kEnforcerCard}));
  EXPECT_TRUE(state.deck.empty());
  EXPECT_FALSE(hasKind(Game::legalActions(state), Kind::BuyContract));
  EXPECT_FALSE(Game::apply(state, {Kind::BuyContract}));
}

// An action card bought this turn can't be played until the next; a second
// copy held from before can
TEST(ContractsTest, ACardBoughtThisTurnWaits) {
  for (const Action &play : {Action{Kind::PlayEnforcer}, Action{Kind::PlayCartel, 0},
                             Action{Kind::PlaySupplyDrop, 0, 1}}) {
    const int card = play.kind == Kind::PlayEnforcer ? kEnforcerCard
                     : play.kind == Kind::PlayCartel ? kCartelCard
                                                     : kSupplyDropCard;
    State state = atMain();
    state.player[0].contracts = {card};
    state.player[0].boughtThisTurn = {card};
    EXPECT_FALSE(has(Game::legalActions(state), play)) << card;
    EXPECT_FALSE(Game::apply(state, play)) << card;
    state.player[0].contracts = {card, card};
    EXPECT_TRUE(has(Game::legalActions(state), play)) << card;
    EXPECT_TRUE(Game::apply(state, play)) << card;
  }
}

// One action card a turn, whatever their kinds; the next turn, another
TEST(ContractsTest, OneCardATurn) {
  State state = atMain();
  state.player[0].contracts = {kSupplyDropCard, kCartelCard, kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::PlaySupplyDrop, 0, 4}));
  EXPECT_FALSE(Game::apply(state, {Kind::PlayCartel, 0}));
  EXPECT_FALSE(Game::apply(state, {Kind::PlayEnforcer}));
  for (const Action &action : Game::legalActions(state))
    EXPECT_TRUE(action.kind != Kind::PlayCartel && action.kind != Kind::PlayEnforcer &&
                action.kind != Kind::PlaySupplyDrop);
  ASSERT_TRUE(Game::apply(state, {Kind::EndTurn}));
  EXPECT_FALSE(state.cardPlayed);
}

// Cards are played only in the main part of the turn: not while trading
TEST(ContractsTest, CardsInTheMainPartOnly) {
  State state = atTrading();
  state.player[0].contracts = {kEnforcerCard};
  EXPECT_FALSE(Game::apply(state, {Kind::PlayEnforcer}));
  ASSERT_TRUE(Game::apply(state, {Kind::EndTrading}));
  EXPECT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
}

// An Enforcer moves the blockade and steals, as on a 7, and then the main
// part goes on (trading's done)
TEST(ContractsTest, AnEnforcerMovesTheBlockadeAndSteals) {
  State state = atTrading(2);
  ASSERT_TRUE(Game::apply(state, {Kind::EndTrading}));
  const int planet = planetIndex(state, kOuter[0]);
  flip(state, planet, PlanetKind::IceMoon, 5);
  dock(state, planet, 1);
  state.player[0].contracts = {kEnforcerCard};
  ASSERT_TRUE(Game::apply(state, {Kind::PlayEnforcer}));
  EXPECT_EQ(state.phase, Phase::Blockade);
  EXPECT_EQ(Game::legalActions(state), (std::vector<Action>{{Kind::MoveBlockade, planet, 1}}));
  ASSERT_TRUE(Game::apply(state, {Kind::MoveBlockade, planet, 1}));
  ASSERT_TRUE(Game::apply(state, {Kind::Steal, static_cast<int>(Resource::Water)}));
  EXPECT_EQ(state.phase, Phase::Main);
  EXPECT_EQ(state.blockade, planet);
  EXPECT_EQ(state.player[1].resources[of(Resource::Water)], 0);
}

// The Interstellar Cartel takes all of the named resource from every other
// player (or up to cartelTake each), nothing from its player
TEST(ContractsTest, TheInterstellarCartel) {
  for (int take : {0, 2}) {
    Settings settings;
    settings.cartelTake = take;
    State state = newGame(4, settings);
    state.phase = Phase::Main;
    state.player[0].contracts = {kCartelCard};
    state.player[0].resources = {0, 0, 3, 0, 0};
    state.player[1].resources = {0, 0, 4, 0, 0};
    state.player[2].resources = {0, 0, 1, 0, 0};
    state.player[3].resources = {0, 0, 0, 0, 0};
    ASSERT_TRUE(Game::apply(state, {Kind::PlayCartel, static_cast<int>(Resource::Fuel)}));
    EXPECT_EQ(state.player[0].resources[of(Resource::Fuel)], take == 0 ? 3 + 5 : 3 + 3);
    EXPECT_EQ(state.player[1].resources[of(Resource::Fuel)], take == 0 ? 0 : 2);
    EXPECT_EQ(state.player[2].resources[of(Resource::Fuel)], 0);
    EXPECT_EQ(state.player[3].resources[of(Resource::Fuel)], 0);
    EXPECT_TRUE(state.player[0].contracts.empty());
  }
}

// A Supply drop: any two resources from the bank; a resource that isn't one
// is refused
TEST(ContractsTest, ASupplyDrop) {
  State state = atMain();
  state.player[0].resources = {0, 0, 0, 0, 0};
  state.player[0].contracts = {kSupplyDropCard};
  EXPECT_FALSE(Game::apply(state, {Kind::PlaySupplyDrop, 0, kResources}));
  ASSERT_TRUE(Game::apply(state, {Kind::PlaySupplyDrop, 0, 4}));
  EXPECT_EQ(state.player[0].resources, (Resources{1, 0, 0, 0, 1}));
}

// Shipyard Surplus: two ships for one's cost, as the turn's card; without
// the cost, not played
TEST(ContractsTest, ShipyardSurplus) {
  State state = cornersAtMain();
  state.ships.clear();
  state.stations.push_back(Station{cornerTouching(state, 2), 0, true});
  const int colony = static_cast<int>(state.stations.size()) - 1;
  state.player[0].contracts = {kShipyardCard, kSupplyDropCard};
  State poor = state;
  poor.player[0].resources = {0, 5, 5, 5, 5}; // (no minerals)
  EXPECT_FALSE(Game::apply(poor, {Kind::PlayShipyard, colony}));
  ASSERT_TRUE(has(Game::legalActions(state), {Kind::PlayShipyard, colony}));
  ASSERT_TRUE(Game::apply(state, {Kind::PlayShipyard, colony}));
  EXPECT_EQ(shipsOwned(state, 0), 2);
  EXPECT_EQ(state.player[0].resources, (Resources{4, 5, 4, 5, 5}));
  EXPECT_FALSE(Game::apply(state, {Kind::PlaySupplyDrop, 0, 0})); // (the turn's card)
}

// The Galactic Embargo: to the first to play embargoEnforcers Enforcers; a
// tie keeps it; more takes it, and its points go with it
TEST(ContractsTest, TheGalacticEmbargo) {
  State state = atMain();
  playEnforcer(state, 0);
  playEnforcer(state, 0);
  EXPECT_EQ(state.embargoHolder, -1); // (2 played)
  const int before = points(state, 0);
  playEnforcer(state, 0);
  EXPECT_EQ(state.embargoHolder, 0);
  EXPECT_EQ(points(state, 0), before + state.settings.embargoPoints);
  for (int i = 0; i < 3; ++i)
    playEnforcer(state, 1);
  EXPECT_EQ(state.embargoHolder, 0); // (a tie, 3 each)
  playEnforcer(state, 1);
  EXPECT_EQ(state.embargoHolder, 1);
  EXPECT_EQ(points(state, 0), before);
  // A lower threshold: the first Enforcer claims it
  State early = atMain();
  early.settings.embargoEnforcers = 1;
  playEnforcer(early, 1);
  EXPECT_EQ(early.embargoHolder, 1);
}
