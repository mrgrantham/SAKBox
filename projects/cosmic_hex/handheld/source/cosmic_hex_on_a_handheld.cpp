// Cosmic Hex on a Nintendo 3DS or DS, in padds (one app, built for each:
// ../3ds, ../ds; the few differences in #if blocks and PlatformProfile):
// the agreed rules (the rules library's agreedRules, on R3), the bots as the
// simulator plays them, fewer iterations a move.
//
//   ScreenPadd (main thread)  the screens (ScreenPadd.hpp): the board in 3D
//              on the stage, the top screen (the scene layer's 3DS or DS
//              backend: the tiles' faces (art/), stations, colonies, ships
//              and the blockade in the players' colours, toon shaded, with
//              the round, points, resources and the last move over it); and
//              the touch panel, the bottom screen: the menus, and the board
//              drawn flat (BoardPicture), to touch and zoom
//   TablePadd  (own thread)  plays the game (TableGame): the dealer's draws
//              and the bots' moves, one at a time, paced so they can be
//              watched, and sends ScreenPadd each new state (TablePadd.hpp)
//
// On your move: touch the board or its buttons, or the pad (up, down, left,
// right, A to choose, B back). Watching: A pauses or goes on.
// X held with R/L: zoom the bottom board in and out   L/R: left and right
// on a choosing screen   touch: a corner, a ship, a button (drag: move the
// board when zoomed)   START: pause
#include "ScreenPadd.hpp"
#include "TablePadd.hpp"

#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/ConsoleScreens/ConsoleScreens.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/StartupSplash/StartupSplash.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>

#include <spdlog/spdlog.h>

#if defined(COSMIC_HEX_BENCH)
#include "CosmicHexBench.hpp"
#endif

int main() {
  namespace ConsoleScreens = ScratchPadd::ConsoleScreens;
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
#if defined(COSMIC_HEX_BENCH) && defined(__NDS__)
  // The -bench build: its timings on the bottom screen, nothing else
  // running, then wait to be switched off
  consoleDemoInit();
  CosmicHexBench::run();
  while (ConsoleScreens::keepRunning())
    ConsoleScreens::waitForTheRefresh();
  return 0;
#endif
  // (black until the splash fades in: the setup below shows nothing)
  ScratchPadd::StartupSplash::darkenScreens();
  const ConsoleScreens::Graphics graphics; // (before ScreenPadd's Display)
#if defined(SAKBOX_DEBUG)
  ScratchPadd::DebugLog::Session log; // a debug build's log, over Wi-Fi
#endif
  {
    ScratchPadd::SystemImplementation<
        ScratchPadd::AboutPadd, ScratchPadd::Input::GamePadd, CosmicHex::Handheld::App::ScreenPadd,
        CosmicHex::Handheld::App::TablePadd, ScratchPadd::Storage::StoragePadd>
        system;
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START)
  }
  ScratchPadd::AppEnded::show(); // ("<its title> has ended.")
  return 0;
}
