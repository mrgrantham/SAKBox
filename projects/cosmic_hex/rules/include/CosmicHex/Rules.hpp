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

#include <algorithm>
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
// earns a badge, and it can hold a station.
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

// The Deep-space relay Charter's id: its holder may build one
// station on a corner touching no planet and no alien outpost; the station
// scores 1 as any does, the card nothing
inline constexpr int kDeepSpaceRelay = 20;

struct Settings {
  int winPoints = 12; // (10 at first)
  // Finish the round (for turn order): reaching winPoints doesn't
  // end the game at once; the round is played out so everyone has had as
  // many turns, and the most points wins (a tie: the later in turn order,
  // who had the less time ahead). Off: the first to winPoints on their own
  // turn wins
  bool finishRound = false;
  // Corners and edges (design.md): stations and
  // colonies on corners, each earning from every planet it touches, none on
  // a corner next to another; ships on corners, moving along edges
  // (shipMove edges, boostMove more for fuel); a planet or zone tile turns
  // over when a ship ends its move on one of its corners; ships built only
  // at colonies; each player starts with a colony on a corner of their home
  // planet and a ship, placed in snake order. Off: today's rules (stations
  // docked on planets, ships moving hex to hex).
  bool corners = false;
  int startShipReach = 3; // (corners) edges from the home colony a starting ship may stand
  // (corners) The first deep-space mission (the user's idea): the
  // starting ship goes on any corner that touches no planet, anywhere on the
  // board, rather than near home; ships are still placed in reverse order,
  // so the last to play chooses first; and the finds the ship's corner
  // touches turn over, as where a move ends.
  bool deepSpaceStart = false;
  // (corners) Each player chooses their home: at the start, in turn order,
  // a home colony goes on a corner of any of the board's homes not yet
  // taken, which makes it theirs (the user's choice: a choice of world
  // and of corner). Off: each seat's home is fixed by the board.
  bool chooseHome = false;
  // (corners) Fuel boosts the first round (the user's question:
  // without, a ship starting in empty space can't reach a three's centre
  // corner, 3 edges away, before the others have moved)
  bool firstRoundBoost = true;
  // The piles of planet kinds and numbers made for the board's planets
  // numbers in the genre's mix (2 and 12 half as common as the
  // others; Catan's own 18 for 18 planets), kinds shared evenly, one of
  // each for every face-down planet that isn't a moon. Off: planetTiles and
  // numbers as given, with extraTiles and extraNumbers for planets beyond.
  bool fitPiles = true;
  // The genre's red-number rule: a 6 or 8 isn't dealt to a planet
  // next to a face-up 6 or 8 while the pile has another number
  bool redNumbersApart = true;
  // How common each kind of planet is in the pile that fits the board
  // (fitPiles), as weights: asteroid belts, lush planets, gas giants, ice
  // moons, nebulae. Even by default; one test made gas giants twice as
  // common (more fuel, so ships move more).
  Resources kindWeights{1, 1, 1, 1, 1};
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
  // layout names them, such as finds beside the small systems;
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
  // Or the home's income in steps by points (after Starfarers'
  // reserve pile: 2 a turn, then 1, then none): each step is {from this
  // many points, this many a turn}, in order; the last one reached counts.
  // Set, it takes the place of catchUpBelow and catchUpGap
  std::vector<std::array<int, 2>> homeIncomeSteps;
  // The exhausted home (the user's idea): on a player's
  // first turn with exhaustHomesAt points or more, the resource they choose
  // from their home is its last: the home becomes a planet of that kind,
  // with a number drawn from exhaustedNumbers, paying every station on it
  // (its owner's too) by the dice, and open to anyone to build by (the
  // distance rule as everywhere); its 1 a turn stops (0: off)
  int exhaustHomesAt = 0;
  std::vector<int> exhaustedNumbers{4, 5, 9, 10};
  // Its kind drawn at random (an equal chance of each, as a die would)
  // instead of the resource chosen (simulated both ways)
  bool exhaustedKindRandom = false;
  // The first player goes without their home's income on their first turn
  // (half a resource's worth of making up for going first)
  bool firstSkipsIncome = false;
  // A hand limit: on a roll of 7, a player holding more than this many
  // resources gives back half of them (rounded down), the ones they hold most
  // of first (0: off). It's decided by the rules, not chosen, so the bots
  // need no new moves.
  int discardOver = 0; // (named the Imperial Tithe)
  // The card deck: these Charters (ids into contracts()) and this many
  // Enforcers. The agreed deck: 25 cards, as the genre's best-known
  // game has, 14 Enforcers and 11 Charters.
  //
  // The deck (23 cards, close to the genre's 25): one
  // of each of three Charters (stations on 3 planets of one kind, Occupier,
  // Deep-space relay), and copies only of the cards that act by themselves:
  // 14 Enforcers, 2 Interstellar Cartels, 2 Shipyard Surplus and 2 Supply
  // drops. (A player can't reveal a Charter twice.)
  // (the user's: stations on 3 planets of one kind, 21, in place of
  // 3 kinds, 0, which 88 to 91% of its keepers met anyway)
  std::vector<int> charters{21, 19, kDeepSpaceRelay};
  int enforcers = 14;
  // The Interstellar Cartel (the user's idea, as the genre's
  // monopoly card): played in the main part of your turn (one card a turn,
  // not one bought this turn), name a resource, and every other player
  // gives you all of it, or with cartelTake up to that many each
  int cartels = 2;
  int cartelTake = 0; // (0: all of it)
  // Shipyard Surplus (the user's idea, as the genre's road
  // building): played as you build a ship, a second comes free at the same
  // colony (room for both under the ship limit, or keep the card). "Under
  // budget, ahead of schedule."
  int shipyards = 2;
  // The home die (the hybrid): each resource from the home is
  // rolled on a six-sided die, one face for each resource and the sixth
  // "your choice", rather than chosen (and the exhausted home, if on, flips
  // as before). Off: chosen, as before
  bool homeDie = false;
  // Supply drop (as the genre's year of plenty): any 2 resources
  // from the bank
  int supplyDrops = 2;
  // Trading between players: after the roll is dealt with
  // (production, or the blockade on a 7) and before anything else in the
  // turn, the player whose turn it is may offer any resources for any
  // resources, to everyone or to one player; each in turn accepts,
  // declines or counters once, and the offerer accepts or declines a
  // counter. No limit on offers. (Off here so the rules' tests keep today's
  // turn; the simulator and the lab turn it on.)
  bool playerTrading = false;
  // The Galactic Embargo: a bonus like Core Control, held by whoever has
  // played the most Enforcers, at least this many; it passes on overtaking
  int embargoEnforcers = 3;
  int embargoPoints = 2;
  // What the Deep-space relay Charter itself scores once revealed, on top
  // of its station's 1 (nothing, the user's: 1, which
  // made it a goal players chase without tilting the seats)
  int relayPoints = 1;
  // Imperial Dominion (the user's idea; off until tried): a bonus
  // like the Embargo, to whoever has the most colonies on other players'
  // former homes (Settings::exhaustHomesAt), at least dominionColonies;
  // it passes on overtaking. 0 points: no Dominion
  int dominionPoints = 0;
  int dominionColonies = 1;
  int coreStationsForControl = 2;
  int coreControlPoints = 2; // (0: no Core Control at all; the agreed rules')
  int badgesPerPoint = 3;
  // Badges cashed in as soon as there are enough, rather than when their
  // holder chooses (agreed: holding them back was nearly always
  // the same choice, a tap to forget, and changed the bots' games by 0.3
  // rounds in 1,800; off: held until cashed, as before)
  bool autoCashBadges = true;
  int ruinsBadges = 2;
  int salvageResources = 2;
  int bankRate = 4; // give this many of one resource for 1 of another
  // The alien outposts (the zone's alien stations): trade
  // there at alienRate for 1 with a ship on one of an outpost's corners, at
  // alienStationRate with a station on one. A station may stand by an
  // outpost though it touches no planet, but becomes a colony only if it
  // touches a planet too.
  int alienRate = 3;
  int alienStationRate = 2;
  int stationPays = 1;
  int colonyPays = 2;
  int startResources = 1; // of each, at the start
  // Extra resources by turn order (the 1st player's, the 2nd's...), to make
  // up for going later: given in turn, minerals, food, fuel, water, crystals
  std::array<int, kMaxPlayers> seatBonus{0, 0, 0, 0};
  bool zoneCorners = true;              // the zone's inner six at the board's corners
  Resources planetTiles{3, 3, 3, 3, 3}; // how many of each kind
  std::vector<int> numbers{2, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 11, 12};
  // Nothing, salvage, ruins, alien outposts, pirate hideout (the second
  // outpost in place of a 'nothing')
  std::array<int, kFinds> finds{5, 2, 2, 2, 1};
  Resources shipCost{1, 0, 1, 0, 0};     // minerals, fuel
  Resources stationCost{0, 1, 0, 1, 0};  // food, water (and the ship)
  Resources colonyCost{2, 2, 0, 0, 0};   // minerals, food
  Resources contractCost{0, 1, 0, 1, 1}; // food, water, crystals
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
  int formerHomeOf = -1;                 // whose home it was, if it's an exhausted home
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
  // Action cards bought this turn (ids), not playable until the next
  std::vector<int> boughtThisTurn;
  bool operator==(const Player &) const = default;
};

// Where in a turn the game is. Phases ending in "Draw" are the dealer's.
enum class Phase : std::uint8_t {
  HomeIncome,   // the player picks their home's resource
  HomeDieDraw,  // (Settings::homeDie) the dealer rolls the home die
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
  PlaceColony,    // (corners) the start: each player's home colony, in turn order
  PlaceShip,      // (corners) then each player's ship, in reverse order
  HomeNumberDraw, // the dealer draws an exhausted home's number
  Trading,        // (playerTrading) the player may offer trades, or end trading
  TradeReply,     // another player answers the offer: accept, decline, counter
  TradeCounter,   // the offerer answers a counter-offer
};

// Some resources as one number (to fit an action): each resource's count,
// 0 to 15, in 4 bits, minerals lowest
int bundleOf(const Resources &resources);
Resources resourcesOf(int bundle);

// An offer between players (Settings::playerTrading): the offerer's `give`
// for the other's `get` (bundles); `to` one player, or -1 for anyone in
// turn; who's answering it; and a counter-offer, if one's been made (what
// the answerer would give and get)
struct TradeOffer {
  int give = 0, get = 0, to = -1;
  int replier = -1;
  int counterGive = 0, counterGet = 0;
  bool operator==(const TradeOffer &) const = default;
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
  bool placingShip = false;      // (corners) a starting ship's finds being turned over
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
  int dominionHolder = -1; // who holds Imperial Dominion (Settings::dominionPoints), or -1
  bool cardPlayed = false; // an Enforcer played this turn (one a turn)
  int winner = -1;
  // Everyone who shares the win (finishing the round, a tie on
  // points goes to the most stations and colonies, then is shared); the
  // winner is the first of them in turn order
  std::vector<int> winners;
  bool finalRound = false; // (Settings::finishRound) someone has reached winPoints
  int lastRoll = 0;
  int pendingPlanet = -1, pendingSite = -1; // being turned over
  std::vector<int> victims;                 // still to be stolen from (the blockade, the pirates)
  int salvageLeft = 0;
  int homeIncomeLeft = 1;             // resources still to take from the home this turn
  std::array<int, 2> offered{-1, -1}; // contracts dealt, to keep one of
  // Trading between players this turn: whether it's over, the offer being
  // answered, and the offers made (give, get), so a bot makes each once
  bool tradingDone = false;
  TradeOffer offer;
  std::vector<std::array<int, 2>> offersMade;
  // Which planet and which zone tile is on each hex (their indexes, -1 for
  // none), so the rules look them up rather than search the lists (round
  // 92: the bots' search was mostly such searches). Made by newGame (the
  // planets and tiles never move), on a grid of the board's hexes
  // (kHexGridSide square round the centre); a State put together by hand
  // has none (hexesIndexed false), and the rules search as before.
  static constexpr int kHexGridSide = 9; // (the board's radius is 4)
  std::array<std::int8_t, kHexGridSide * kHexGridSide> planetOnHex{}, siteOnHex{};
  bool hexesIndexed = false;
  bool operator==(const State &) const = default;
};

// A new game for 2 to 4 players: the board laid out, homes face up with a
// station each, a ship at each home, and the starting resources. Player
// `first` takes the first turn (each player's home is the same whoever goes
// first, so a simulation can tell turn order and home apart).
State newGame(int players, const Settings &settings = {}, int first = 0);

// A board: where the homes, the planets and the deep-space zone's sites are
// (corners and edges' layouts, as in layouts/layouts.txt)
struct Board {
  std::vector<Hex> homes, planets, zone; // (zone empty: the finds' usual places)
  bool operator==(const Board &) const = default;
};
// R3, one of the fairest layouts the simulations found, with its
// finds beside the small systems
Board boardR3();

// The rules as agreed (the design doc's), on a board. Settings'
// own defaults are older rules the tests are written against; this is what
// a real game plays (the consoles), and the simulator's defaults are checked
// to give the same (AgreedRules_test)
// With three players the target is 10 points (games as long as
// four players' at 12)
Settings agreedRules(const Board &board, int players = 4);
inline constexpr int kThreePlayerWinPoints = 10;

// A board from a line of the layouts file ("R3 homes=4,-2,... planets=...
// small_finds=..."; zone= or small_finds= is where the finds go; other
// fields are for people reading the file); nullopt if a field isn't q,r pairs
std::optional<Board> boardFromLine(std::string_view line);

// The boards a game offers for this many players, the default first: for
// four, R3 then the others fair by turn and by home at 1,800
// games, the four-cluster boards last (fair, games longer); for
// three, T4 then T5, both fair at 1,800 games. Their
// lines are the layouts file's (AgreedRules_test checks they still match)
struct NamedBoard {
  std::string name;
  Board board;
  // Extra start resources by turn position that even the board's turn order
  // out, offered as a choice (R3's 0,0,1,1); all 0 for none
  std::array<int, kMaxPlayers> seatBonus{};
  // Something a player should know when choosing it ("" for nothing)
  std::string note;
  bool hasSeatBonus() const {
    return std::any_of(seatBonus.begin(), seatBonus.end(), [](int b) { return b > 0; });
  }
};
std::vector<NamedBoard> boardChoices(int players = 4);

// ---------------------------------------------------------------------------
// The actions

struct Action {
  enum class Kind : std::uint8_t {
    ChooseHome,   // a: the resource
    RollHomeDie,  // (the dealer) a: the face, 0 to 4 a resource, 5 the player's choice
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
    PlayCartel,     // a: the resource named
    PlayShipyard,   // a: the colony (as BuildShip): two ships for one
    PlaySupplyDrop, // a, b: the two resources taken
    CashBadges,
    EndTurn,
    OfferTrade,  // a: what you give, b: what you get (bundles), c: to whom (-1: anyone)
    AcceptTrade, // (the answerer, or the offerer answering a counter)
    DeclineTrade,
    CounterTrade, // the answerer: a: what they'd give, b: what they'd get (bundles)
    EndTrading,
    PlaceColony, // (corners) a: the corner (its number in boardCorners())
    PlaceShip,   // (corners) a: the corner
    NumberHome,  // dealer: a: an exhausted home's number, b: its kind if drawn (else -1)
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
// An Interstellar Cartel's card id (Settings::cartels)
inline constexpr int kCartelCard = 98;
// Shipyard Surplus's and the Supply drop's card ids
inline constexpr int kShipyardCard = 97;
inline constexpr int kSupplyDropCard = 96;
// Whether a card id is a Charter (an index into contracts())
bool isCharter(int card);

struct Contract {
  std::string_view name;
  int points = 1;
  bool (*met)(const State &, int player);
};
const std::vector<Contract> &contracts();
// What Charter `id` scores once revealed, under `settings` (the relay's is
// a setting: Settings::relayPoints)
int charterPoints(const Settings &settings, int id);

// The settings as one line of text ("settings key=values ..."), as the
// action logs keep them, and read back: nothing if a field is wrong. A
// field repeated later in the line wins, so appending "key=values" changes
// one setting (the simulator's --setting)
std::string settingsLine(const Settings &settings);
std::optional<Settings> settingsFromLine(std::string_view line);

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
