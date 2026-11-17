// StartMenu.hpp - the start screen's menu (round 87, layout C): on the touch
// screen as buttons, worked by touch or by the console's buttons. Rows, top
// to bottom:
//
//   Players  < 4 >                 4: you and three bots; 3: you and two
//   Board    < R3 >                boardChoices(players), the default first
//   End      < finish the round >  or the first to the target wins
//   Bonus    < off >               the board's seat bonus, if it has one (R3)
//   [ Play ]  [ Watch ]
//
// Up and down move between rows; left and right change a row's value (or,
// on the last row, choose Play or Watch); A starts what's chosen on the last
// row, and plays from any other; B watches. A touch on a row's arrows
// changes it, on Play or Watch starts it.
//
//   StartMenu menu;
//   if (menu.press(StartMenu::Key::Confirm) == StartMenu::Outcome::Play) ...
//   for (const auto &button : menu.buttons(320, 240)) ...  // where to draw
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

class StartMenu {
public:
  enum class Row : std::uint8_t { Players, Board, End, SeatBonus, Start };
  enum class Key : std::uint8_t { Up, Down, Left, Right, Confirm, Back };
  enum class Outcome : std::uint8_t { None, Play, Watch };

  StartMenu();

  // What's chosen
  int players() const { return players_; }
  std::size_t board() const { return board_; } // (in boardChoices(players()))
  const std::string &boardName() const { return boards_[board_].name; }
  const Board &boardItself() const { return boards_[board_].board; }
  bool finishRound() const { return finishRound_; }
  // The board's seat bonus, if it has one and it's chosen; else all 0
  std::array<int, kMaxPlayers> seatBonus() const;
  Row focus() const { return focus_; }
  bool watchChosen() const { return watch_; } // (on the last row: Watch, not Play)

  // A row's words: its label and its value as shown ("4: you and three bots")
  std::string label(Row row) const;
  std::string value(Row row) const;

  // A button pressed
  Outcome press(Key key);

  // The touch screen's buttons on a screen this size: each row's arrows
  // (step -1 and +1) and the Play and Watch buttons (step 0)
  struct Button {
    Row row;
    int step; // -1: the left arrow, +1: the right, 0: Play or Watch
    Outcome starts;
    float x, y, width, height;
  };
  std::vector<Button> buttons(int width, int height) const;
  // Where a row's value sits (between its arrows), for drawing
  struct Area {
    float x, y, width, height;
  };
  Area valueArea(Row row, int width, int height) const;
  // A touch at (x, y): changes a row or starts a game
  Outcome touch(int x, int y, int width, int height);

private:
  void change(Row row, int step);

  int players_ = 4;
  std::vector<NamedBoard> boards_;
  std::size_t board_ = 0;
  bool finishRound_ = true;
  bool seatBonus_ = false; // (off unless chosen: round 89)
  Row focus_ = Row::Players;
  bool watch_ = false;
};

} // namespace CosmicHex::Handheld
