// AppBasics.hpp - what every part of the Cosmic Hex app shares: the
// build's switches, its clock, and how long things take on screen
// (ShowTimes.hpp)
#pragma once

#include <CosmicHex/Handheld/ShowTimes.hpp>

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

// The build that plays itself (COSMIC_HEX_SCRIPTED_PLAY: the -scripted
// app): player 0's taps and buttons from the scripted player
// (ScriptedPlayer), as the Mac's scripted runs make them, game after game
#if defined(COSMIC_HEX_SCRIPTED_PLAY)
constexpr bool kScriptedPlay = true;
#else
constexpr bool kScriptedPlay = false;
#endif

// The title screen: the 3D scene, or the plain text title (false). Which
// one a player sees, and how it's chosen, is an open question on the
// roadmap page; the scene for now
constexpr bool kTitleScene = true;

// How long the title takes to fade in from black, once the start's work
// is done (a little after the last)
constexpr double kSecondsTitleFadeIn = 0.6;

using Clock = std::chrono::steady_clock;
// The seconds since `start`
inline double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

} // namespace CosmicHex::Handheld::App
