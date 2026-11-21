// RulesInternal.hpp - what the rules' source files share, and nothing
// outside the rules library sees (the public API is
// include/CosmicHex/Rules.hpp). The rules are split by part of the turn:
//
//   Board.cpp              the board's lookups: the hex index, corners and
//                          their reach, whose stations and ships are where
//   Setup.cpp              the boards, the agreed rules, newGame, the
//                          starting hands and the start's placements
//   Production.cpp         the home's income, the roll and its payouts, the
//                          blockade and stealing
//   Trading.cpp            the bank, alien outposts and players' offers
//   BuildingAndMoving.cpp  ships built and moved, stations and colonies
//   Exploring.cpp          tiles turned over, the zone's finds, badges
//   Contracts.cpp          the contract deck: Charters and action cards
//   Scoring.cpp            points, the titles (Core Control, Embargo,
//                          Dominion) and the game's end
//   Text.cpp               the state and actions as the action log writes them
//   SettingsText.cpp       the settings as one line of text
//   Rules.cpp              Game: one function a phase, the legal actions,
//                          the dealer's draws
//
// The small lookups the bots' move listing calls in its loops are defined
// here, inline, so each file can still inline them as when the rules were
// one file. Functions marked [[gnu::hot]] are those (measured): the DS build
// puts them in ITCM, its 32 KB of fast instruction memory (the Makefile's
// hot_in_itcm step, as for Cyclone's resolver); elsewhere it's only a hint
// to the compiler.
#pragma once

#include <CosmicHex/Rules.hpp>

#include <HexGrid/CornersAndEdges.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CosmicHex::Internal {

using Kind = Action::Kind;

constexpr int kBoardRadius = 4;

// ---------------------------------------------------------------------------
// Resources

inline bool canAfford(const Resources &have, const Resources &cost) {
  for (int i = 0; i < kResources; ++i) {
    if (have[static_cast<std::size_t>(i)] < cost[static_cast<std::size_t>(i)])
      return false;
  }
  return true;
}
inline void pay(Resources &have, const Resources &cost) {
  for (int i = 0; i < kResources; ++i)
    have[static_cast<std::size_t>(i)] -= cost[static_cast<std::size_t>(i)];
}
inline int total(const Resources &have) { return std::accumulate(have.begin(), have.end(), 0); }

// The player whose turn it is
inline Player &current(State &state) { return state.player[static_cast<std::size_t>(state.turn)]; }

// Takes one `value` out of `from`; false if there's none
bool removeOne(std::vector<int> &from, int value);

// ---------------------------------------------------------------------------
// The board (Board.cpp)

// A hex's place in State's hex index (planetOnHex, siteOnHex), or -1 off it
[[gnu::hot]] inline int hexCell(Hex hex) {
  constexpr int kSide = State::kHexGridSide, kMiddle = kSide / 2;
  const int column = hex.q + kMiddle, row = hex.r + kMiddle;
  if (column < 0 || column >= kSide || row < 0 || row >= kSide)
    return -1;
  return row * kSide + column;
}

// The planet (or zone tile) on a hex, or -1: from the index newGame makes,
// or found in the list in a State without one
[[gnu::hot]] inline int planetAt(const State &state, Hex hex) {
  if (state.hexesIndexed) {
    const int cell = hexCell(hex);
    return cell < 0 ? -1 : state.planetOnHex[static_cast<std::size_t>(cell)];
  }
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (state.planets[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}
[[gnu::hot]] inline int siteAt(const State &state, Hex hex) {
  if (state.hexesIndexed) {
    const int cell = hexCell(hex);
    return cell < 0 ? -1 : state.siteOnHex[static_cast<std::size_t>(cell)];
  }
  for (std::size_t i = 0; i < state.zone.size(); ++i) {
    if (state.zone[i].hex == hex)
      return static_cast<int>(i);
  }
  return -1;
}

// Fills State's hex index from its planets and zone tiles
void indexHexes(State &state);

// ---- Corners and edges (Settings::corners)

bool onBoard(Corner corner);

[[gnu::hot]] inline bool touches(Corner corner, Hex hex) {
  const auto touching = HexGrid::hexesTouching(corner);
  return std::find(touching.begin(), touching.end(), hex) != touching.end();
}

// The station on `corner`, or -1
[[gnu::hot]] inline int stationAt(const State &state, Corner corner) {
  for (std::size_t i = 0; i < state.stations.size(); ++i)
    if (state.stations[i].at == corner)
      return static_cast<int>(i);
  return -1;
}

// The distance rule: no station on the corner, nor on any corner an edge away
[[gnu::hot]] inline bool cornerFree(const State &state, Corner corner) {
  if (stationAt(state, corner) >= 0)
    return false;
  for (Corner next : HexGrid::neighbours(corner))
    if (stationAt(state, next) >= 0)
      return false;
  return true;
}

// A corner (its number) and the fewest edges to it
struct CornerReach {
  std::int16_t corner;
  std::int16_t steps;
};

// For each corner, every other within a move and its boost, nearest first:
// worked out once, at start (Rules.cpp makes it before any game)
const std::vector<std::vector<CornerReach>> &cornerReachTable();

// Every corner within `steps` edges of `from`, with the fewest edges to
// each, nearest first
std::vector<CornerReach> cornersWithin(Corner from, int steps);

// ---- Who has what where

// The player's home colony (the first station they placed), or -1
int homeColonyOf(const State &state, int player);

bool dockedAt(const Planet &planet, int player);
int freeDock(const Planet &planet);

// A planet a station can be built on: turned over, and not a home
[[gnu::hot]] inline bool settleable(const Planet &planet) {
  return planet.flipped && planet.homeOf < 0 && planet.kind != PlanetKind::Home;
}

// Whether the player has a station on the planet (docked, or on a corner
// touching it), or a colony
bool hasStationOn(const State &state, const Planet &planet, int player);
bool hasColonyOn(const State &state, const Planet &planet, int player);
// How many players have stations on the planet (without listing them)
int ownerCount(const State &state, const Planet &planet);
int shipsOf(const State &state, int player);
// Stations on core planets (with corners: stations touching one)
int coreStationsOf(const State &state, int player);

// ---------------------------------------------------------------------------
// Setting up (Setup.cpp)

bool applyStartCard(State &state, const Action &action);
bool applyPlaceColony(State &state, const Action &action);
bool applyPlaceShip(State &state, const Action &action);
// (corners) After an action: a starting ship's finds all turned over, the
// setup goes on
void finishPlacingShip(State &state);
void colonyPlacements(const State &state, std::vector<Action> &out);
void shipPlacements(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Production (Production.cpp)

// How many resources a player takes from their home at the start of their
// turn: 1, and 1 more if they're behind (Settings::catchUpBelow, catchUpGap)
int homeIncomeFor(const State &state, int player);
// The home's income starts (or goes on): rolled on the home die, or chosen;
// none left, the roll
void nextHomeIncome(State &state);

bool applyHomeIncome(State &state, const Action &action);
bool applyHomeNumber(State &state, const Action &action);
bool applyRoll(State &state, const Action &action);
bool applyBlockade(State &state, const Action &action);
bool applySteal(State &state, const Action &action);
void blockadeActions(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Trading (Trading.cpp)

int tradeRate(const State &state, int player);

bool applyTrading(State &state, const Action &action);
bool applyTradeReply(State &state, const Action &action);
bool applyTradeCounter(State &state, const Action &action);
bool applyBankTrade(State &state, const Action &action);
void tradingActions(const State &state, std::vector<Action> &out);
void bankTradeActions(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Building and moving (BuildingAndMoving.cpp)

// (corners) Whether a turned-over alien outpost has a corner here
bool byAnOutpost(const State &state, Corner corner);
// A Deep-space relay's place: touching no planet (homes included), and no
// find unless it's turned over and isn't an alien outpost
bool isRelaySpot(const State &state, Corner corner);

// Building a ship (or two, with Shipyard Surplus), moving one, settling it
// as a station, or upgrading a station to a colony
bool applyBuildOrMove(State &state, const Action &action);
void buildAndMoveActions(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Exploring (Exploring.cpp)

// (corners) Turns over the next planet or zone tile the ship's stop touches,
// if any is left face down
void turnNextOver(State &state);
// The red-number rule (Settings::redNumbersApart)
bool isRed(int number);
bool redNumberNextTo(const State &state, int planet);
bool pileHasOtherThanRed(const State &state);

bool applyPlanetDraw(State &state, const Action &action);
bool applyZoneDraw(State &state, const Action &action);
bool applySalvage(State &state, const Action &action);
bool applyCashBadges(State &state, const Action &action);
// Badges cashed at once, when that's the rule (Settings::autoCashBadges)
void cashBadgesAtOnce(State &state);
void cashBadgesActions(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Contracts and action cards (Contracts.cpp)

// Colonies built (the home colony left out)
int colonies(const State &state, int player);
// Whether the player whose turn it is may play an action card
bool canPlayCard(const State &state, int card);
// The card's gone from the hand, and the turn's card is played
void playCard(State &state, int card);

bool applyBuyContract(State &state, const Action &action);
bool applyContractDraw(State &state, const Action &action);
bool applyKeepContract(State &state, const Action &action);
bool applyRevealContract(State &state, const Action &action);
bool applyEnforcer(State &state, const Action &action);
bool applySupplyDrop(State &state, const Action &action);
bool applyCartel(State &state, const Action &action);
void keepContractActions(const State &state, std::vector<Action> &out);
void contractActions(const State &state, std::vector<Action> &out);
void cardActions(const State &state, std::vector<Action> &out);

// ---------------------------------------------------------------------------
// Scoring (Scoring.cpp)

void updateCoreControl(State &state, int player);
void updateEmbargo(State &state, int player);
void updateDominion(State &state);
// (finishRound) The round played out: the winners named, the game over
void declareWinners(State &state);
// After an action: whether the player whose turn it is has won (or, with
// finishRound, starts the final round)
void checkForWin(State &state);

// ---------------------------------------------------------------------------
// Text (Text.cpp, SettingsText.cpp)

std::optional<int> toInt(std::string_view text);
std::vector<std::string_view> split(std::string_view text, char by);
// Numbers split by commas; "-" for none
std::optional<std::vector<int>> intList(std::string_view text);
template <typename Container> std::string joined(const Container &values) {
  std::string out;
  for (const auto &value : values) {
    if (!out.empty())
      out += ',';
    out += std::to_string(value);
  }
  return out.empty() ? "-" : out;
}

std::string settingsText(const Settings &settings);
std::optional<Settings> readSettings(std::string_view line);

} // namespace CosmicHex::Internal
