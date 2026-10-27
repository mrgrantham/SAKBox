// Precision Flight: a prototype rail shooter in the spirit of SNES Star Fox
// where the numeric precision of the game's subsystems is a resource.
//
// Precision groups (flexmath FlexGroups, owned by a FlexController):
//   Flight - grid the ship's lateral position snaps to as physics sees it
//   Hull   - how finely collisions sample the ship's position along the rail
//   Render - how finely vertex positions resolve when drawing
//
// Powerups change a group's precision for a few seconds:
//   FOCUS (cyan)    Flight 1 -> 5 bits. Gold gates are centred between the
//                   default flight grid points, so only FOCUS can thread them.
//   PHASE (magenta) Hull 2 -> -1 bits. Collisions sample every 2 units, so
//                   thin energy walls slip between samples. Render drops to
//                   3 bits while phased: the world visibly glitches.
//
// The game runs as five ScratchPadd padds (see README.md):
//   FlightSimPadd     (main thread) the simulation; the precision lab is its controls
//   FlightDisplayPadd (main thread) window, input, rendering, HUD, control panels
//   EventLogPadd      (own thread)  prints game events to the terminal
//   FlightSoundPadd   (own thread)  turns game events into sounds
//   AudioPadd         (own thread)  plays them
//
//   bazel run //projects/examples/precision_flight:precision_flight [-- --autopilot]

#include "EventLogPadd.hpp"
#include "FlightDisplayPadd.hpp"
#include "FlightSimPadd.hpp"
#include "FlightSoundPadd.hpp"

#include <AudioPadd.hpp>

#include <ScratchPadd/ScratchPadd.hpp>

#include <spdlog/spdlog.h>

#include <string>

int main(int argc, char **argv) {
  using namespace PrecisionFlight;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--autopilot") {
      FlightOptions::autopilot = true;
    }
  }
  spdlog::set_level(spdlog::level::warn); // keep the terminal for game events

  // The simulation is listed before the display so each frame draws the
  // snapshot answering that frame's input (both run on this thread).
  ScratchPadd::SystemImplementation<FlightSimPadd, FlightDisplayPadd, EventLogPadd, FlightSoundPadd,
                                    AudioPadd>
      system;
  system.instantiate();
  system.start(); // returns when the window closes
  return 0;
}
