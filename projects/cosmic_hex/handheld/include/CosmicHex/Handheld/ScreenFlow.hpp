// ScreenFlow.hpp - which screen the console shows, and what moves it on
// (round 90: the user's review of round 89's state diagram; White, Making
// Embedded Systems, "State Machines": the states drawn first, then kept
// apart so each can be tested). The screens:
//
//   Title     the 3D title on top; Play or Quit below
//   Setup     the game's settings below (StartMenu), the board on top
//   Game      playing, or watching the bots
//   Paused    the game held, a menu below: Resume, Restart, Title, Quit
//   Won       the title again, with who won; any button for the title
//
//   Title --Play--> Setup --Start--> Game --someone wins--> Won --any--> Title
//                   Setup --Back--> Title
//   Game --START--> Paused --Resume--> Game
//                   Paused --Restart--> Game (a new game, the same settings)
//                   Paused --Title--> Title,  Paused --Quit--> (the app ends)
//   Title --Quit--> (the app ends)
//
// The dice's own stages (thrown, showing, pulled away) are within Game.
//
//   ScreenFlow flow;
//   const auto step = flow.on(ScreenFlow::Event::Play);   // Title -> Setup
//   if (step.startGame) ...                                // what to do
#pragma once

#include <cstdint>

namespace CosmicHex::Handheld {

class ScreenFlow {
public:
  enum class Screen : std::uint8_t { Title, Setup, Game, Paused, Won };
  enum class Event : std::uint8_t {
    Play,     // (Title) to the settings
    Start,    // (Setup) begin the game as set
    Back,     // (Setup) back to the title
    Pause,    // (Game) START: hold it, with the menu
    Resume,   // (Paused) back to the game
    Restart,  // (Paused) a new game, the same settings
    ToTitle,  // (Paused) leave the game for the title
    Quit,     // (Title, Paused) end the app
    GameWon,  // (Game) someone has won
    Continue, // (Won) any button: to the title
  };
  // What an event asks the app to do, beside changing the screen
  struct Step {
    bool changed = false;   // the screen changed (redraw the bottom screen)
    bool startGame = false; // begin a game with the setup's settings
    bool stopGame = false;  // the game is left (pausing doesn't stop it)
    bool holdGame = false;  // pause (true) or carry on (false) the game, when holdChanged
    bool holdChanged = false;
    bool quit = false; // end the app
  };

  Screen screen() const { return screen_; }

  // An event: the step it makes, or nothing if it means nothing here
  Step on(Event event);

private:
  Screen screen_ = Screen::Title;
};

} // namespace CosmicHex::Handheld
