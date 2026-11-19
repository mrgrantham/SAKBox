// MoveSpots.hpp - where on the board a move happens, so touching a spot can
// list the moves there (you choose by touching the board, the
// list the fallback for moves that have no place, such as ending a turn).
//
//   if (concerns(state, move, touched)) ...   // a move at the touched spot
#pragma once

#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

// The corners a move is at (a ship's corner and where it goes, a station's,
// where a piece is placed), and the hex (the blockade's planet); none for
// moves without a place
struct MoveSpot {
  std::vector<Corner> corners;
  std::optional<Hex> hex;
};
MoveSpot spotOf(const State &state, const Action &move);

// Whether `move` happens at `touched`: at its corner, or on its hex (the
// blockade's)
bool concerns(const State &state, const Action &move, const BoardPicture::Spot &touched);

} // namespace CosmicHex::Handheld
