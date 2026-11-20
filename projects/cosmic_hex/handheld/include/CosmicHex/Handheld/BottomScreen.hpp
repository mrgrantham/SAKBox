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
#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/ChoiceScreens.hpp>
#include <CosmicHex/Handheld/OtherPlayers.hpp>
#include <CosmicHex/Handheld/Pickers.hpp>
#include <CosmicHex/Handheld/TradeRoom.hpp>

#include <ScratchPadd/Buttons/Buttons.hpp>
#include <ScratchPadd/Canvas/Canvas.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

using ScratchPadd::Canvas; // (ScratchPadd/Canvas)

class BottomScreen {
public:
  enum class Mode { Board, Build, Resources, Players, Placing, Cards, Keep, Others, Trade };

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
  // The resource picker's card in focus (the one a single pick takes), for
  // the top screen to show big; nothing when it isn't open
  std::optional<Resource> resourceInFocus() const;
  // The picker's choice, while it's open (its title: "Your home pays 1")
  const std::optional<Pickers::ResourceChoice> &resourceChoice() const { return resources_; }

  // What's shown on the top screen for looking at, any time (// the user's: they pause the game
  // while open): your resources and cards (Show resources); another tap closes it
  // or another player (the other players' tabs: infoPlayer() whose)
  enum class Info { None, Resources, Player };
  Info info() const {
    // (a player's only while their tabs show: a picker opening by itself
    // closes them)
    return info_ == Info::Player && mode_ != Mode::Others ? Info::None : info_;
  }
  int infoPlayer() const { return othersChosen_; }
  // After buying, which of the two dealt is chosen (-1: none yet), for the
  // top screen to mark it too
  int keepChosen() const { return mode_ == Mode::Keep ? keepChosen_ : -1; }
  // Debug builds: the move list button (BoardButtons::Button::MoveList),
  // and whether the list of moves shows (off at first)
  void setMoveListButton(bool shown) { moveListButton_ = shown; }
  bool showingMoveList() const { return moveListButton_ && moveList_; }
  // A move to make next, once the rules allow it (the trade room's bank
  // trade after ending the turn's offers to players): taken once
  std::optional<Action> nextQueued();
  // The trade room, for the top screen to say the deal
  const TradeRoom &trade() const { return trade_; }
  // In Placing: the corners where the chosen build, or the ship touched,
  // may go, to be marked
  std::vector<Corner> placingCorners() const;
  // ...of those, for a ship's move, the ones it needs fuel to reach (the
  // boost), to mark differently
  std::vector<Corner> placingCornersWithFuel() const;
  // The ship touched, whose moves are marked (an index into the state's
  // ships), to show it selected
  std::optional<int> selectedShip() const;
  // At the start (placing your home colony, then your ship), what's been
  // touched and End Turn would place, to show there; nothing before a touch.
  // Touching another place moves it, touching it again (or B) takes it back,
  // and End Turn (or A) places it
  std::optional<Action> startPlacement() const { return startPlacement_; }

  // Feedback for a tap on a button (the user's: taps that did
  // nothing gave no sign they'd landed): the button flashes and a short
  // message says what's happening or why not. The app clears it after a
  // moment (clearFeedback), and redraws
  const std::string &message() const { return buttons_.message(); }
  bool showingFeedback() const { return buttons_.showingFeedback(); }
  // The time now (seconds, the app's clock), each frame: a pressed
  // button glows a moment, and pressing() says to draw again meanwhile
  void setTime(double now) { now_ = now; }
  // Who's who at the table (the tabs and the trade room say names)
  void setNames(PlayerNames names) { trade_.setNames(std::move(names)); }
  bool pressing() const { return buttons_.pressing(now_); }
  void clearFeedback();

  // Whether a choosing screen covers the board (else the board shows, with
  // the buttons over it)
  bool coversBoard() const {
    return mode_ == Mode::Build || mode_ == Mode::Resources || mode_ == Mode::Players ||
           mode_ == Mode::Cards || mode_ == Mode::Keep || mode_ == Mode::Others ||
           mode_ == Mode::Trade;
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
  std::vector<Action> placing_;     // (Placing: the chosen build's actions)
  std::vector<bool> placingFuel_;   // (a ship's moves: each one's fuel needed)
  int placingShip_ = -1;            // (a ship's moves: whose)
  bool placingEndsTrading_ = false; // (and building ends the turn's offers first)
  // A build's move: at once, or after ending the turn's offers to players
  Outcome build(const Action &action, bool endsTrading);
  bool startPlacing_ = false; // (Placing: the start's, touch then End Turn)
  std::optional<Action> startPlacement_;
  std::optional<Pickers::ResourceChoice> resources_;
  std::vector<Resource> picked_;
  bool resourcesCanGoBack_ = false;
  std::optional<Pickers::PlayerChoice> players_;
  int playerChosen_ = -1, cardChosen_ = -1;
  int resourceFocus_ = 0; // (the D-pad's card on the resource picker)
  void openPicker(Pickers::ResourceChoice choice, bool canGoBack);
  void focusNextResource(int direction);
  std::vector<HeldCard> held_; // (the cards screen: your cards)
  int heldChosen_ = -1;
  std::optional<KeepChoice> keep_; // (after buying: the two to keep one of)
  int keepChosen_ = -1;
  double now_ = 0;                                 // (setTime)
  bool moveListButton_ = false, moveList_ = false; // (debug builds' list of moves)
  TradeRoom trade_;                                // (the trade room)
  std::vector<Action> queued_;                     // (its moves still to make)
  std::vector<PlayerView> others_;                 // (the other players' tabs)
  int othersChosen_ = -1;
  Info info_ = Info::None;
  // The board's buttons (ScratchPadd's Buttons: placed by BoardButtons'
  // layout, their flash and message when tapped)
  ScratchPadd::Buttons::Panel buttons_;
  void layOutButtons();    // (buttons_ from the layout, for the situation now)
  Outcome useChosenCard(); // (the cards screen's Play or Reveal)
  // (update's work: the screen following the game)
  void follow(const State &state, const std::vector<Action> &legal, bool yourMove,
              bool yourRollWaits);
  void closeOthers();     // (the other players' Back: the board again)
  Outcome confirmTrade(); // (the trade room's Trade, Offer or Accept)
  CardSprites cards_;
  CardSprites bigCards_; // (the cards screens': bigger, four a row)
  std::array<std::vector<Canvas::Colour>, Art::kIcons> icons_;
  int iconSize_ = 0;
};

} // namespace CosmicHex::Handheld
