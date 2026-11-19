// BottomScreen.hpp - what the bottom screen shows in a game and what a tap
// on it does: the board with its buttons; the build screen;
// the resource and player pickers; and the board again, its corners marked,
// while you choose where something goes. It decides from the rules' legal
// moves, so it has no rules of its own; the app draws what it says and
// sends the moves it makes.
//
//   screen.update(state, legal, yourMove);         // each new snapshot
//   const auto outcome = screen.tap(x, y, spotUnderTheTap);
//   if (outcome.play) send(*outcome.play);
//   screen.draw(canvas, boardPixels);              // when it's to be redrawn
//
// Touching one of your ships marks where it can go; touching a marked
// corner moves it there. Taps it doesn't use (elsewhere on the board, when
// nothing's being placed) are the app's: selecting a corner to list its
// moves.
#pragma once

#include <CosmicHex/Handheld/BoardButtons.hpp>
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BuildMenu.hpp>
#include <CosmicHex/Handheld/ChoiceScreens.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

using ScratchPadd::Canvas; // (ScratchPadd/Canvas)

class BottomScreen {
public:
  enum class Mode { Board, Build, Resources, Players, Placing };

  BottomScreen(int width, int height);

  // A new look at the game: the screen follows it (the resource picker
  // opens by itself when the move is a choice of resources; any other
  // screen closes when it isn't your move)
  void update(const State &state, const std::vector<Action> &legal, bool yourMove,
              bool yourRollWaits = false);

  struct Outcome {
    std::optional<Action> play; // a move to make
    bool used = false;          // the tap was this screen's (else the app's)
    bool roll = false;          // throw the dice (Roll, when your roll waits)
  };
  // A tap at (x, y), with the board's spot under it in the board modes
  Outcome tap(int x, int y, const std::optional<BoardPicture::Spot> &spot);

  // The buttons on a choosing screen (the user's question: the
  // D-pad moves between its rows, cards or players, A picks and then
  // confirms, B goes back). On the board they're the app's (its list of
  // moves), except B, which puts down what's being placed
  enum class Key { Up, Down, Left, Right, Confirm, Back };
  Outcome press(Key key);

  Mode mode() const { return mode_; }

  // What's shown on the top screen for looking at, any time (// the user's: they pause the game
  // while open): your resources and cards (Show resources); another tap closes it
  enum class Info { None, Resources };
  Info info() const { return info_; }
  // In Placing: the corners where the chosen build, or the ship touched,
  // may go, to be marked
  std::vector<Corner> placingCorners() const;
  // At the start (placing your home colony, then your ship), what's been
  // touched and End Turn would place, to show there; nothing before a touch.
  // Touching another place moves it, touching it again (or B) takes it back,
  // and End Turn (or A) places it
  std::optional<Action> startPlacement() const { return startPlacement_; }

  // Feedback for a tap on a button (the user's: taps that did
  // nothing gave no sign they'd landed): the button flashes and a short
  // message says what's happening or why not. The app clears it after a
  // moment (clearFeedback), and redraws
  const std::string &message() const { return message_; }
  bool showingFeedback() const { return !message_.empty() || flash_.has_value(); }
  void clearFeedback();

  // Whether a choosing screen covers the board (else the board shows, with
  // the buttons over it)
  bool coversBoard() const {
    return mode_ == Mode::Build || mode_ == Mode::Resources || mode_ == Mode::Players;
  }
  // The choosing screen, onto `canvas`, when one covers the board
  void draw(Canvas &canvas);
  // The buttons, and a tapped button's message, over the board's own
  // pixels (Canvas::over: no copy of the map)
  void drawOverBoard(Canvas &board);
  // The areas drawOverBoard draws on now: the map under them is drawn
  // again before they are (BoardPicture::redrawNext), now and after they
  // change, so each is drawn over the map once
  std::vector<BoardPicture::Area> overBoardAreas() const;
  // Draws one of its pictures (cards, icons) not yet drawn, for spare
  // frames; whether they're all ready
  bool prepare();

private:
  int width_, height_;
  Mode mode_ = Mode::Board;
  State state_;
  std::vector<Action> legal_;
  bool yourMove_ = false;
  BoardButtons::Situation situation_;
  std::vector<BuildRow> rows_;
  int buildChosen_ = -1;
  std::vector<Action> placing_; // (Placing: the chosen build's actions)
  bool startPlacing_ = false;   // (Placing: the start's, touch then End Turn)
  std::optional<Action> startPlacement_;
  std::optional<Pickers::ResourceChoice> resources_;
  std::vector<Resource> picked_;
  bool resourcesCanGoBack_ = false;
  std::optional<Pickers::PlayerChoice> players_;
  int playerChosen_ = -1, cardChosen_ = -1;
  int resourceFocus_ = 0; // (the D-pad's card on the resource picker)
  Info info_ = Info::None;
  std::string message_;
  std::optional<BoardButtons::Button> flash_;
  CardSprites cards_;
  std::array<std::vector<Canvas::Colour>, Art::kIcons> icons_;
  int iconSize_ = 0;
};

} // namespace CosmicHex::Handheld
