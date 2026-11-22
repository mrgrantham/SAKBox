// ShowTimes.hpp - how long things take on the stage as a game goes: a
// move's time on screen, the pieces' moves, the points bars' outline, the
// other players' panel. The app's shows (GameShows) and its drawing share
// them
#pragma once

#include <CosmicHex/Handheld/BoardMotion.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

// The points bars' outline moving to the next player's row
// (0.6 s, the user's: at 0.35 s, a row's 5 pixels, as the turn's banner
// came in, it read as a jump)
constexpr float kSecondsOutline = 0.6f;
// How long the other players' view takes to slide down (or up)
// (long enough to see it come: at 0.3 s a DS's 20 or so frames a second
// showed it in four or five jumps)
constexpr float kSecondsPanel = 0.45f;
// How long each move stays on screen before the next, so a game can be
// watched (the dealer's draws go straight on), and how long its slide or
// flip takes
constexpr double kSecondsAMove = 0.6;
// (a ship's flight: long enough to see its engine burn, within a move's
// time on screen)
constexpr double kSecondsAnimating = 0.5;
// A planet's number chip, once it's turned over, drops this far onto it
// and settles in this long
constexpr double kSecondsChipDrop = 0.35;
constexpr float kChipDropHeight = 1.2f;
// How long the board moves after a move: the pieces, a chip dropping, a
// building's dust fading
constexpr double kSecondsBoardMoving =
    std::max(kSecondsAnimating + kSecondsChipDrop, BoardMotion::kLandingEnds *kSecondsAnimating);
// How long the board, come down, is looked at before who goes first
constexpr double kSecondsLookingAtBoard = 2;

} // namespace CosmicHex::Handheld
