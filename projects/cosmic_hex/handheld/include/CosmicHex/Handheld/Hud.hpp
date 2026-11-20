// Hud.hpp - what's always over the board on the top screen: each player's
// points as a row of dashes (PointsBars), the turn's row outlined and the
// outline moving from row to row; points and badges just won popping in
// (ScorePops); the round top left in a turquoise hexagon; the badges top
// right, towards the next point; the last move along the bottom; and who
// won, once someone has.
//
//   drawHud(frame, state, {.outlineRow = row, .badgesOf = 0, .pops = &pops,
//                          .lastMove = "Lyra builds a station.", .winner = "You win!"});
#pragma once

#include <CosmicHex/Handheld/ScorePops.hpp>
#include <CosmicHex/Handheld/TopViews.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace CosmicHex::Handheld {

struct HudLook {
  std::optional<float> outlineRow; // (the outline part way between rows; none: the turn's)
  int badgesOf = 0;                // (whose badges show top right)
  const ScorePops *pops = nullptr; // (what's popping in; none: nothing)
  std::string_view lastMove;       // (as it fits a line; none: not shown)
  std::string winner;              // ("You win!", say; none: no one yet)
};

void drawHud(const TopFrame &f, const State &state, const HudLook &look);

} // namespace CosmicHex::Handheld
