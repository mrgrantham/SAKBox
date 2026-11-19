// BoardButtons.hpp - the board screen's buttons (rounds 93 to 100, the
// user's layout): small icon buttons in the corners the whole board leaves
// empty. Top left: Show resources, and the other players under it; top
// right, down the side: Build, Trade, Cards; bottom left Roll and bottom
// right End turn, never both at once. Each icon is art/icons' SVG.
//
//   const auto buttons = BoardButtons::layout(256, 192, situation);
//   if (const auto hit = BoardButtons::at(buttons, touchX, touchY)) ...
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld::BoardButtons {

enum class Button { ShowResources, OtherPlayers, Build, Trade, Cards, Roll, EndTurn };

// What the buttons depend on: whether it's your move, and what you can do
struct Situation {
  bool yourMove = false;
  bool canRoll = false;    // your roll is waiting for you to throw it
  bool canEndTurn = false; // the rules would take EndTurn (or EndTrading)
  bool canTrade = false;   // the bank, an outpost or the players
  bool holdsCards = false; // any action cards or Charters
};
// The situation from the game: the player to move's legal actions
Situation situationOf(const State &state, const std::vector<Action> &legal, bool yourMove);

struct Place {
  Button button;
  float x, y, size; // its square, in pixels
  bool usable;      // (shown dimmed when not)
};
// The buttons shown on a `width` x `height` screen: 22 pixels on a DS,
// larger in proportion on a 3DS, 3 pixels in from its edges. Roll and End
// turn are left out when they can't be used; the rest show, dimmed
std::vector<Place> layout(int width, int height, const Situation &situation);

// The usable button at (x, y), if any
std::optional<Button> at(const std::vector<Place> &places, int x, int y);

Art::Icon iconOf(Button button);

} // namespace CosmicHex::Handheld::BoardButtons
