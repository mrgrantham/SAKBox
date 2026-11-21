// TableMessages.hpp - what TablePadd (the game, on its own thread) and
// ScreenPadd (the screens) say to each other
#pragma once

#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Rules.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CosmicHex::Handheld::App {

// The game as it stands (TablePadd to ScreenPadd), after each move; and on
// a person's move, the moves they can make, each in words
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

// ScreenPadd to TablePadd: start a game (you against three bots, or four
// bots to watch, on a board from boardChoices() and a play style); pause or
// go on (watching); the move you chose
struct StartGame {
  bool withPerson = false;
  int players = 4;
  CosmicHex::Board board = CosmicHex::boardR3(); // (a StartMenu choice)
  bool finishRound = true; // (the round played out, or the first to the target wins)
  std::array<int, CosmicHex::kMaxPlayers> seatBonus{}; // (the board's, if chosen)
  // Held until the screen lets it go (SetHold off): the board coming down
  // from the setup first, so no move is made before it's in view
  bool held = false;
};
struct TogglePause {};
// The pause menu holds the game (true) or lets it go on (false); leaving a
// game stops it
struct SetHold {
  bool on;
};
struct StopGame {};
struct DiceRolled {
  int first, second; // (the faces the dice settled on)
};
struct PlayMove {
  CosmicHex::Action move;
};

} // namespace CosmicHex::Handheld::App
