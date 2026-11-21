// BoardButtons.hpp - the board screen's buttons (the
// user's layout): small icon buttons in the corners the whole board leaves
// empty. Top left: Show resources, and the other players under it; top
// right, down the side: Build, Trade, Cards. Bottom left: Roll, which once
// thrown fades away as End Turn (an arrow round to itself, in its own
// colour) fades in there. Bottom right: Confirm (the green check), while
// you're placing something: it puts down what you've chosen, as A does.
// The two kept apart so a double tap can't end your turn. Each icon is
// art/icons' SVG.
//
//   const auto buttons = BoardButtons::layout(256, 192, situation);
//   if (const auto hit = BoardButtons::at(buttons, touchX, touchY)) ...
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld::BoardButtons {

enum class Button {
  ShowResources,
  OtherPlayers,
  Build,
  Trade,
  Cards,
  Roll,
  EndTurn,
  MoveList,
  Confirm
};

// What the buttons depend on: whether it's your move, and what you can do
struct Situation {
  bool yourMove = false;
  bool canRoll = false;    // your roll is waiting for you to throw it
  bool canEndTurn = false; // the rules would take EndTurn (or EndTrading)
  bool canTrade = false;   // the bank, an outpost or the players
  bool holdsCards = false; // any action cards or Charters
  // A ship built this turn still stands on its colony (Settings::
  // newShipsLeave): End Turn shows, dimmed, and says to move it first
  bool newShipMustMove = false;
  // Placing something (the start's home colony and ship, a build, a ship's
  // move): Confirm puts down what's been touched, usable once something has
  bool placing = false, placeChosen = false;
  // Debug builds: the list of moves on the top screen, a button to turn
  // it on and off (under Other players)
  bool moveListButton = false;
};
// The situation from the game: the player to move's legal actions, and
// whether the dice wait for you to throw them (the user's: your
// roll is yours to make; the rules have the dealer roll, so the app says)
Situation situationOf(const State &state, const std::vector<Action> &legal, bool yourMove,
                      bool yourRollWaits = false);

struct Place {
  Button button;
  float x, y, size; // its square, in pixels
  bool usable;      // (shown dimmed when not)
};
// The buttons shown on a `width` x `height` screen: 22 pixels on a DS,
// larger in proportion on a 3DS, 3 pixels in from its edges. Roll, End
// Turn and Confirm are left out when they can't be used; the rest show,
// dimmed
std::vector<Place> layout(int width, int height, const Situation &situation);

// Buttons coming and going (Roll giving way to End Turn, Confirm while
// placing) fade in and out rather than blinking: each new one fades in,
// each one gone fades out where it was. The first layout shows at once
class Fades {
public:
  static constexpr double kSeconds = 0.25;
  struct Shown {
    Place place;
    float opacity; // (0 to 1)
  };
  void update(const std::vector<Place> &places, double now);
  // What to draw at `now`: the layout's buttons and those fading out
  // (not usable), each with how much of it shows
  std::vector<Shown> shown(double now) const;
  bool fading(double now) const;

private:
  struct Tracked {
    Place place;
    double since; // (when it came or went)
    bool going;
  };
  float opacityOf(const Tracked &tracked, double now) const;
  std::vector<Tracked> tracked_;
  bool first_ = true;
};

// The usable button at (x, y), if any
std::optional<Button> at(const std::vector<Place> &places, int x, int y);

Art::Icon iconOf(Button button);

} // namespace CosmicHex::Handheld::BoardButtons
