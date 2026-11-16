// Rules.hpp - Cosmic Hex's rules, as plain C++ with no screens or padds:
// the game's state, its actions, and what each action does. The rules are
// the ones in ../../design.md ("The rules engine's first version").
//
// The rules describe themselves to the action log (ActionLog::Game) and the
// bots (GameBots::BotGame) through CosmicHex::Game, so a game can be
// recorded, replayed and played by bots, and thousands of games simulated
// to tune the numbers. Every number a rule uses is in Settings, one table,
// stored in the state, so a log replays under the numbers it was played with.
//
// Chance is the dealer's: the dice, and the tiles and cards drawn. Face-down
// tiles aren't assigned at the start; a tile is drawn from what's left of
// its pile when a ship turns it over, which is the same as a shuffled
// face-down tile but leaves nothing hidden on the board. The only hidden
// information is the contract cards in each player's hand.
//
//   CosmicHex::State game = CosmicHex::newGame(4);
//   while (!CosmicHex::Game::isOver(game)) { ... CosmicHex::Game::apply(game, action); }
#pragma once

#include <GameBots/BotGame.hpp>
#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Hex.hpp>
#include <SeededRandom/Generator.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace CosmicHex {

using HexGrid::Corner;
using HexGrid::Hex;

// ---------------------------------------------------------------------------
// The pieces

inline constexpr int kResources = 5;
enum class Resource : std::uint8_t { Minerals, Food, Fuel, Water, Crystals };
using Resources = std::array<int, kResources>;

// A planet's kind; kinds 0-4 each give the resource with the same number.
// A moon gives nothing (no resource, no number), but turning it over still
// earns a badge, and it can hold a station (round 42; named in round 44).
enum class PlanetKind : std::uint8_t {
  AsteroidBelt,
  LushPlanet,
  GasGiant,
  IceMoon,
  Nebula,
  Home,
  Moon
};
// Whether planets of this kind give a resource
constexpr bool givesResources(PlanetKind kind) { return kind < PlanetKind::Home; }

// What a face-down tile in the deep-space zone turns out to be
enum class Find : std::uint8_t { Nothing, Salvage, Ruins, AlienStation, PirateHideout };
inline constexpr int kFinds = 5;

inline constexpr int kMaxPlayers = 4;
inline constexpr int kDocks = 2; // stations a planet can hold, one per player

// ---------------------------------------------------------------------------
// The numbers, all in one table (the design doc's defaults; the bots tune them)

struct Settings {
  int winPoints = 12; // (10 until round 41)
  // Corners and edges (agreed in rounds 39 to 50; design.md): stations and
  // colonies on corners, each earning from every planet it touches, none on
  // a corner next to another; ships on corners, moving along edges
  // (shipMove edges, boostMove more for fuel); a planet or zone tile turns
  // over when a ship ends its move on one of its corners; ships built only
  // at colonies; each player starts with a colony on a corner of their home
  // planet and a ship, placed in snake order. Off: today's rules (stations
  // docked on planets, ships moving hex to hex).
  bool corners = false;
  int startShipReach = 3; // (corners) edges from the home colony a starting ship may stand
  int maxShips = 3;
  int shipMove = 3;   // hexes a ship moves a turn, free
  int boostMove = 2;  // more, for 1 fuel
  int coreRadius = 2; // planet slots this close to the centre are core planets...
  std::vector<Hex>
      coreHexes; // ...unless these name them (the round-28 option B: {0,0}, {1,-2}, {-1,2})
  // The homes, player by player, instead of the usual layout for the number
  // of players (spread evenly round the edge: two opposite; three a third of
  // the way round; four in opposite pairs). Edge slots that aren't homes
  // hold planets, with extraTiles and extraNumbers added for them.
  std::vector<Hex> homeHexes;
  // The planets (besides the homes), if a layout names them, such as round
  // 44's cluster boards; otherwise one every third hex. Hexes off the board
  // are ignored, and the deep-space zone's sites leave out planets.
  std::vector<Hex> planetHexes;
  // How many of the face-down planets turn out moons (drawn like the other
  // kinds, from the same pile)
  int moons = 0;
  // Where the deep-space zone's face-down tiles (the finds) are, if a
  // layout names them, such as round 45's finds beside the small systems;
  // otherwise the 12 hexes 2 from the centre, or with zoneCorners the inner
  // six moved to the board's corners. Planets are left out.
  std::vector<Hex> zoneHexes;
  // Tiles and numbers for planets beyond planetTiles' and numbers' (fewer
  // homes than four leave more planet slots), used in turn
  std::vector<int> extraTiles{0, 1};    // an asteroid belt, a lush planet
  std::vector<int> extraNumbers{10, 3}; // (evening up 4 and 10, then 3 and 11)
  // Catching up (as in some of the genre's games): a player behind takes one
  // more resource of their choice with their home's income: when they have
  // fewer than catchUpBelow points, or are catchUpGap or more behind the
  // leader (0: off)
  int catchUpBelow = 0;
  int catchUpGap = 0;
  // The first player goes without their home's income on their first turn
  // (half a resource's worth of making up for going first)
  bool firstSkipsIncome = false;
  // A hand limit: on a roll of 7, a player holding more than this many
  // resources gives back half of them (rounded down), the ones they hold most
  // of first (0: off). It's decided by the rules, not chosen, so the bots
  // need no new moves.
  int discardOver = 0; // (named the Imperial Tithe)
  // The card deck: these Charters (ids into contracts()) and this many
  // Enforcers. The agreed deck (round 37): 25 cards, as the genre's best-known
  // game has, 14 Enforcers and 11 Charters.
  std::vector<int> charters{6, 18, 4, 0, 10, 2, 5, 3, 9, 8, 17};
  int enforcers = 14;
  // The Galactic Embargo: a bonus like Core Control, held by whoever has
  // played the most Enforcers, at least this many; it passes on overtaking
  int embargoEnforcers = 3;
  int embargoPoints = 2;
  int coreStationsForControl = 2;
  int coreControlPoints = 2;
  int badgesPerPoint = 3;
  int ruinsBadges = 2;
  int salvageResources = 2;
  int bankRate = 4;  // give this many of one resource for 1 of another
  int alienRate = 3; // at the zone's alien station, with a station or ship next to it
  int stationPays = 1;
  int colonyPays = 2;
  int startResources = 1; // of each, at the start
  // Extra resources by turn order (the 1st player's, the 2nd's...), to make
  // up for going later: given in turn, minerals, food, fuel, water, crystals
  std::array<int, kMaxPlayers> seatBonus{0, 0, 0, 0};
  bool zoneCorners = true;              // the zone's inner six at the board's corners
  Resources planetTiles{3, 3, 3, 3, 3}; // how many of each kind
  std::vector<int> numbers{2, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 11, 12};
  std::array<int, kFinds> finds{6, 2, 2, 1, 1}; // nothing, salvage, ruins, alien, pirate
  Resources shipCost{1, 0, 1, 0, 0};            // minerals, fuel
  Resources stationCost{0, 1, 0, 1, 0};         // food, water (and the ship)
  Resources colonyCost{2, 2, 0, 0, 0};          // minerals, food
  Resources contractCost{0, 1, 0, 1, 1};        // food, water, crystals
  bool operator==(const Settings &) const = default;
};

// ---------------------------------------------------------------------------
// The state

struct Planet {
  Hex hex;
  bool flipped = false;
  PlanetKind kind = PlanetKind::Home; // once flipped (homes start face up)
  int number = 0;                     // its dice number once flipped (0: none)
  bool core = false;
  int homeOf = -1;                       // the player whose home it is, or -1
  std::array<int, kDocks> docks{-1, -1}; // the players with stations here
  std::array<bool, kDocks> colony{false, false};
  bool operator==(const Planet &) const = default;
};

struct ZoneSite {
  Hex hex;
  bool flipped = false;
  Find find = Find::Nothing;
  bool operator==(const ZoneSite &) const = default;
};

struct Ship {
  int owner = 0;
  Hex at;             // (hexes) where it is
  bool moved = false; // this turn
  Corner corner{};    // (corners) where it stands
  bool operator==(const Ship &) const = default;
};

// (corners) A station or colony on a corner, earning from every planet it
// touches
struct Station {
  Corner at;
  int owner = 0;
  bool colony = false;
  bool operator==(const Station &) const = default;
};

struct Player {
  Resources resources{};
  int badges = 0;             // not yet cashed in
  int badgesEarned = 0;       // all ever earned
  int badgePoints = 0;        // points from cashed badges
  int findsKept = 0;          // zone finds taken (salvage, ruins, the pirate hideout)
  std::vector<int> contracts; // cards held, hidden from the others (Charters and Enforcers)
  std::vector<int> revealed;  // Charters scored
  int enforcersPlayed = 0;    // for the Galactic Embargo
  int enforcersNew = 0;       // bought this turn: not playable until the next
  bool operator==(const Player &) const = default;
};

// Where in a turn the game is. Phases ending in "Draw" are the dealer's.
enum class Phase : std::uint8_t {
  HomeIncome,   // the player picks their home's resource
  RollDraw,     // the dealer rolls
  Blockade,     // a 7: the player moves the blockade ship
  StealDraw,    // the dealer picks which resource is taken
  Main,         // trade, build, move, settle, contracts, badges, end the turn
  PlanetDraw,   // the dealer turns over a planet tile
  ZoneDraw,     // the dealer turns over a zone tile
  Salvage,      // the player takes salvage, one resource at a time
  ContractDraw, // the dealer deals 2 contracts
  KeepContract, // the player keeps one
  Over,
  PlaceColony, // (corners) the start: each player's home colony, in turn order
  PlaceShip,   // (corners) then each player's ship, in reverse order
};

struct State {
  Settings settings;
  int players = 4;
  int turn = 0;  // whose turn
  int round = 1; // counted from 1; a round is every player's turn
  Phase phase = Phase::HomeIncome;
  std::vector<Planet> planets; // in board order (ring by ring from the centre)
  std::vector<ZoneSite> zone;
  std::vector<Ship> ships;
  std::vector<Station> stations; // (corners)
  int first = 0;                 // who took the first turn
  int setupStep = 0;             // (corners) how many setup placements are done
  std::vector<Hex> turning;      // (corners) planets and tiles still to turn over this move
  std::array<Player, kMaxPlayers> player{};
  // What's left to draw
  Resources kindsLeft{};
  int moonsLeft = 0; // moons still face down
  std::vector<int> numbersLeft;
  std::array<int, kFinds> findsLeft{};
  std::vector<int> deck; // contract ids
  // The rest
  int blockade = -1;       // the planet the blockade ship is on, or -1
  int coreHolder = -1;     // who holds Core Control, or -1
  int embargoHolder = -1;  // who holds the Galactic Embargo, or -1
  bool cardPlayed = false; // an Enforcer played this turn (one a turn)
  int winner = -1;
  int lastRoll = 0;
  int pendingPlanet = -1, pendingSite = -1; // being turned over
  std::vector<int> victims;                 // still to be stolen from (the blockade, the pirates)
  int salvageLeft = 0;
  int homeIncomeLeft = 1;             // resources still to take from the home this turn
  std::array<int, 2> offered{-1, -1}; // contracts dealt, to keep one of
  bool operator==(const State &) const = default;
};

// A new game for 2 to 4 players: the board laid out, homes face up with a
// station each, a ship at each home, and the starting resources. Player
// `first` takes the first turn (each player's home is the same whoever goes
// first, so a simulation can tell turn order and home apart).
State newGame(int players, const Settings &settings = {}, int first = 0);

// ---------------------------------------------------------------------------
// The actions

struct Action {
  enum class Kind : std::uint8_t {
    ChooseHome,   // a: the resource
    Roll,         // dealer: a, b: the dice
    MoveBlockade, // a: the planet (-1 if none is flipped), b: who to steal from (-1: nobody)
    Steal,        // dealer: a: the resource taken
    Trade,        // a: give (bankRate or alienRate of it), b: get 1
    BuildShip,    // a: the planet (corners: the colony, its index in stations)
    Move,         // a: the ship, b, c: q, r of where to (corners: b the corner)
    RevealPlanet, // dealer: a: the kind, b: the number
    RevealFind,   // dealer: a: the find
    TakeSalvage,  // a: the resource
    Settle,       // a: the ship
    BuildColony,  // a: the planet (corners: the station)
    BuyContract,
    DealContracts,  // dealer: a, b: the contracts (b -1 if only one was left)
    KeepContract,   // a: the contract
    RevealContract, // a: the contract
    PlayEnforcer,   // then MoveBlockade, as on a 7
    CashBadges,
    EndTurn,
    PlaceColony, // (corners) a: the corner (its number in boardCorners())
    PlaceShip,   // (corners) a: the corner
  };
  Kind kind = Kind::EndTurn;
  int a = 0, b = 0, c = 0;
  bool operator==(const Action &) const = default;
};

// ---------------------------------------------------------------------------
// Contracts: secret goals, each checked against the board and the player's
// tokens when it's revealed

// The card id of an Enforcer in a deck or hand (Charters are their index in
// contracts()). Played on your turn, from the turn after it's bought: the
// blockade moves and the player steals, as on a 7.
inline constexpr int kEnforcerCard = 99;

struct Contract {
  std::string_view name;
  int points = 1;
  bool (*met)(const State &, int player);
};
const std::vector<Contract> &contracts();

// ---------------------------------------------------------------------------
// Corners (Settings::corners): every corner of the board, in a fixed order,
// so an action can name one by its number. (A Move then takes the ship and
// the corner's number; BuildShip and BuildColony take a station's index.)

const std::vector<Corner> &boardCorners();
int cornerNumber(Corner corner); // -1 if it isn't on the board

// ---------------------------------------------------------------------------
// Scoring

int points(const State &state, int player);
int stationsOf(const State &state, int player); // the home included

// ---------------------------------------------------------------------------
// The rules, for the action log and the bots

struct Game {
  using State = CosmicHex::State;
  using Action = CosmicHex::Action;
  static constexpr std::string_view kName = "cosmic_hex";
  static constexpr int kRulesVersion = 1;

  static bool apply(State &state, const Action &action);
  static std::vector<Action> legalActions(const State &state);

  static int playerCount(const State &state) { return state.players; }
  static int currentPlayer(const State &state);
  static bool isOver(const State &state) { return state.winner >= 0; }
  static double score(const State &state, int player) { return state.winner == player ? 1.0 : 0.0; }
  // For a game cut off before its end: how its points compare with the others'
  static double estimate(const State &state, int player);
  static Action dealerAction(const State &state, SeededRandom::Generator &random);
  static State determinize(const State &state, int viewer, SeededRandom::Generator &random);

  // The state as text. Any state can be written (for fingerprints); only a
  // starting state reads back (a log keeps its starting state and actions).
  static std::string writeState(const State &state);
  static std::optional<State> readState(std::string_view text);
  static std::string writeAction(const Action &action);
  static std::optional<Action> readAction(std::string_view text);
};
static_assert(GameBots::BotGame<Game>);

} // namespace CosmicHex
