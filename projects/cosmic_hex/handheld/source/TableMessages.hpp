// TableMessages.hpp - what TablePadd (the game, on its own thread) and
// ScreenPadd (the screens) say to each other
#pragma once

#include <CosmicHex/Handheld/TableGame.hpp>
#include <CosmicHex/Handheld/TableSnapshot.hpp>
#include <CosmicHex/Rules.hpp>

#include <array>

namespace CosmicHex::Handheld::App {

// (TablePadd to ScreenPadd, after each move: TableSnapshot.hpp)

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
