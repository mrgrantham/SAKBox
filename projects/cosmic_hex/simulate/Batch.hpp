// Batch.hpp - Cosmic Hex games played between bots, one at a time, for the
// command-line simulator (simulate.cpp) and the balance lab: the options a
// batch is run with, the rules settings they make, one game played from
// them, and what came of it (as a line of CSV).
//
//   CosmicHex::Batch::Options options;          // or from the command line
//   options.layout = "R2";
//   CosmicHex::Batch::readLayout(options);      // its homes and planets
//   const CosmicHex::Batch::Bots bots(options);
//   for (int game = 0; game < options.games; ++game) {
//     const CosmicHex::Batch::GameResult result = playGame(options, game, bots);
//     csv << csvLine(options, result);
//   }
//
// Each game has its own seed, made from the batch's seed and the game's
// number, so a batch can be shared out over threads (the balance lab plays
// several games at once) and still give the same games.
#pragma once

#include <CosmicHex/BotStyles.hpp>
#include <CosmicHex/Rules.hpp>

#include <ActionLog/ActionLog.hpp>
#include <GameBots/Bots.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Batch {

struct Options {
  int games = 50;
  int players = 4;
  int iterations = 100;
  int rolloutDepth = 60;
  int maxRounds = 200;
  std::uint64_t seed = 1;
  std::string logs; // a folder to write each game's log to, if given
  std::string csv;  // a file to write one line a game to, if given
  // The defaults are the rules as agreed (round 70), each with a flag to
  // turn it off: corners and edges, the deep-space start, the home's income
  // 2 a turn below 5 points on the home die and the home flipping at 5 (the
  // hybrid), the Imperial Tithe at 7, trading between players, finds beside
  // the small systems
  int catchUpBelow = 5, catchUpGap = 0;     // catching up (Settings)
  int discardOver = 7;                      // the Imperial Tithe on a 7 (Settings; 0: off)
  bool skipFirst = false;                   // the first player skips their first income
  bool randomFirst = false;                 // a random player goes first, rather than player 1
  std::array<int, kMaxPlayers> seatBonus{}; // extra start resources by turn order
  int empty = 5;    // empty tiles of the zone's 12 (the rest finds, shared out)
  int outposts = 2; // alien outposts among them (round 60: two)
  // Costs instead of the rules' (round 61: free stations, and a ship
  // costing one more), and how common each kind of planet is (round 62)
  // (The defaults: free stations, round 72; the ship minerals, fuel, water, round 74)
  std::optional<Resources> stationCost = Resources{0, 0, 0, 0, 0};
  std::optional<Resources> shipCost = Resources{1, 0, 1, 1, 0};
  Resources kindWeights{1, 1, 1, 1, 1};
  // The exhausted home, at this many points (0: off), its kind drawn rather
  // than chosen
  int exhaustHomesAt = 5;
  bool exhaustedKindRandom = false;
  // The home's income in steps by points ({from, a turn}, round 67); empty:
  // the catch-up rule
  std::vector<std::array<int, 2>> homeIncomeSteps;
  bool homeDie = true;          // the home's resources rolled on the home die (round 66's hybrid)
  bool finishRound = true;      // play out the round when someone reaches the points to win
  bool coreControl = false;     // Core Control's points (off: round 89)
  bool firstRoundBoost = false; // fuel boosts in the first round (off: the agreed rule, round 90)
  bool trading = true;          // trading between players (round 57), the bots by the trading rule
  int cartelTake = 0; // the Interstellar Cartel takes all (0), or up to this many from each
  // Each simulated game in the bots' search in a frame arena of this many
  // bytes (GameBots' Budget::frameArenaBytes; the simulator links the
  // counting allocator, so it takes effect: 18% faster on a Mac, round 92;
  // the same games); 0: none (--arena 0)
  std::size_t frameArenaBytes = 128 * 1024;
  int strongIterations = 0;   // a stronger bot, thinking this much, at one home in turn
  bool randomOthers = false;  // the other players choose any legal move at random (round 97)
  std::string variant;        // a label for the CSV
  std::string core = "seven"; // the core planets: seven (the centre and the six around it) or three
  std::vector<Hex> planets;   // a layout's planets (empty: one every third hex)
  std::vector<Hex> homes;     // and its homes (empty: the usual ones)
  int moons = 0;              // moons among the face-down planets
  std::vector<Hex> zone;      // where the finds are (empty: the usual places)
  std::string layout;         // a layout's name in the layouts file
  std::string layoutsFile = "projects/cosmic_hex/layouts/layouts.txt";
  // The layout's finds beside its small systems (the rule from round 58; a
  // layout without that list keeps the usual places)
  bool smallFinds = true;
  int win = 0;                  // points to win (0: the rules' default)
  int relayPoints = -1;         // the relay Charter's own points (-1: the rules')
  int dominionPoints = -1;      // Imperial Dominion's points (-1: the rules'; 0: none)
  std::vector<int> charters;    // the Charters in the deck (empty: the rules')
  std::vector<BotStyle> styles; // the players' styles, turned round a seat each game
  bool edges = true;            // corners and edges (Settings::corners)
  int move = 0, boost = 0;      // a ship's move and its fuel boost (0: the default for the rules)
  bool deepSpace = true;        // (edges) the starting ship anywhere touching no planet
};

// "q,r,q,r,...": hexes, or nothing if the text isn't pairs of numbers
std::optional<std::vector<Hex>> hexesFrom(std::string_view text);

// Reads options.layout from options.layoutsFile (a line a layout: its name,
// then homes=, planets=, zone= and small_finds=, each q,r pairs) into the
// options' homes, planets and zone (small_finds with smallFinds). Returns what went wrong, if
// anything.
std::optional<std::string> readLayout(Options &options);
// The layouts' names in a layouts file, in its order
std::vector<std::string> layoutNames(const std::string &file);

// The rules' settings for a batch's options
Settings settingsFor(const Options &options);

// What happened in a game, from its log: how much the blockade did (moves
// to a planet, thefts, resources its planets didn't pay), how often a player
// took a catching-up resource, the hand limit's discards, the Enforcers
// played, and each contract's story (dealt, kept, revealed, revealed by the
// winner)
struct GameStats {
  int blockades = 0, thefts = 0, denied = 0, caughtUp = 0, discarded = 0;
  int enforcers = 0;
  int cartels = 0;     // Interstellar Cartels played
  int shipyards = 0;   // Shipyard Surplus played
  int supplyDrops = 0; // Supply drops played
  // Round 61's statistics, player by player: trades at the bank, at an alien
  // outpost by ship and by station, with other players (and the resources
  // that changed hands in them), and offers made
  std::array<int, kMaxPlayers> bankTrades{}, shipOutpostTrades{}, stationOutpostTrades{},
      playerTrades{}, resourcesTraded{}, offersMade{};
  // And the game's: how often the lead changed hands (at turns' ends), the
  // round the winner took it for good, the most the winner was behind, 7s
  // rolled, and 7s on which the Imperial Tithe took someone's resources
  int leadChanges = 0, leadTakenRound = 0, comeback = 0, sevens = 0, sevensWithDiscards = 0;
  std::vector<std::array<int, 4>> contracts =
      std::vector<std::array<int, 4>>(CosmicHex::contracts().size());
};
GameStats statsOf(const ActionLog::Log<Game> &log);

// Where a player's points came from
struct Sources {
  int stations = 0, colonies = 0, core = 0, embargo = 0, dominion = 0, badges = 0, contracts = 0;
};
Sources sourcesOf(const State &state, int player);

// Stations at the end by the size of the system they're in (or with
// corners, the biggest they touch): singles, pairs, threes
std::array<int, 3> stationsBySystem(const State &state);

// The bots a batch plays with: the plain search, and one for each style
struct Bots {
  explicit Bots(const Options &options);
  GameBots::Ismcts<Game> plain;
  std::array<GameBots::Ismcts<Game>, kBotStyleNames.size()> styled;
};

// One game and what came of it
struct GameResult {
  int game = 0;
  std::string seed;       // its seed's code
  int first = 0;          // who went first (from 0)
  int strong = -1;        // who had the stronger bot, or -1
  int winner = -1;        // or -1 if it was stopped unfinished
  bool sharedWin = false; // (a tie after the round: the winner one of those sharing it)
  int rounds = 0;
  int actions = 0;
  std::array<int, kMaxPlayers> points{};
  Sources winnerSources; // (all 0 if unfinished)
  GameStats stats;
  std::vector<BotStyle> styles; // each player's
  std::array<int, 3> bySystem{};
  int badgesAll = 0;        // badges every player earned
  ActionLog::Log<Game> log; // the whole game, to replay
};

// Plays game number `game` of the batch (its seed from the batch's and the
// game's number), the bots thinking options.iterations a move
GameResult playGame(const Options &options, int game, const Bots &bots);

// The CSV: its header line, and a game's line (each ending in a newline)
std::string csvHeader();
std::string csvLine(const Options &options, const GameResult &result);

} // namespace CosmicHex::Batch
