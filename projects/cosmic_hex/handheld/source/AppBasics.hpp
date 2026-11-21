// AppBasics.hpp - what every part of the Cosmic Hex app shares: the
// build's switches, its clock, and how long things take on screen
#pragma once

#include <CosmicHex/Handheld/BoardMotion.hpp>

#include <algorithm>
#include <chrono>

namespace CosmicHex::Handheld::App {

namespace Scene = ScratchPadd::Scene;

// The build that watches four bots by itself (COSMIC_HEX_WATCH_AT_START:
// the -watch apps), for unattended runs in an emulator or on a console,
// with what each frame costs and the memory on the stage
#if defined(COSMIC_HEX_WATCH_AT_START)
constexpr bool kWatchAtStart = true;
#else
constexpr bool kWatchAtStart = false;
#endif

// The title screen: the 3D scene, or the plain text title (false). Which
// one a player sees, and how it's chosen, is an open question on the
// roadmap page; the scene for now
constexpr bool kTitleScene = true;

using Clock = std::chrono::steady_clock;
// The seconds since `start`
inline double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// The points bars' outline moving to the next player's row
// (0.6 s, the user's: at 0.35 s, a row's 5 pixels, as the turn's banner
// came in, it read as a jump)
constexpr float kSecondsOutline = 0.6f;
// How long the other players' view takes to slide down (or up)
// (long enough to see it come: at 0.3 s a DS's 20 or so frames a second
// showed it in four or five jumps)
constexpr float kSecondsPanel = 0.45f;
// How long the title takes to fade in from black, once the start's work
// is done (a little after the last)
constexpr double kSecondsTitleFadeIn = 0.6;

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

} // namespace CosmicHex::Handheld::App
