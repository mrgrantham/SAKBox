// TableSnapshot.hpp - the game as the screens get it, after each move: the
// state, the moves made since the last, and on a person's move the moves
// they can make. The app's table (TablePadd, on its own thread) sends one
// after each move; the scripted player's table (ScriptedPlay) makes them
// the same way
#pragma once

#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Rules.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

struct TableSnapshot {
  State state;
  std::vector<std::string> lastMoves; // the newest last
  double lastThinkSeconds = 0;        // the last bot move's search
  bool paused = false;
  bool yourMove = false;
  std::vector<CosmicHex::Action> moves;
  std::vector<std::string> moveText;
  // The game waits for the dice: the screen throws them with
  // this seed (a new one each roll) and sends back what they show
  bool waitingForDice = false;
  std::uint64_t diceSeed = 0;
  FirstRoll firstRoll; // (the dice that chose who goes first)
  // The moves made since the last snapshot, as made (the home die's face)
  std::vector<TableGame::Played> played;
  int botGames = 0;                // (the last bot move's simulated games, for measuring)
  GameBots::SearchTimes botSearch; // (and where its time went)
};

// The table as it is: `game`, the moves since the first `playedFrom` of
// its played(), held or not, its dice thrown from `seed` and how far it's
// got (the same game throws the same)
TableSnapshot snapshotOf(const TableGame &game, std::size_t playedFrom, bool paused,
                         std::uint64_t seed);

} // namespace CosmicHex::Handheld
