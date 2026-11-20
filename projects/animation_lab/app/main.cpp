// animation_lab: motions made with physics (Cyclone) three ways, seen from
// the side and above with their timing, baked into small path and timing
// curves and saved for the games (see ../README.md).
//
//   bazel run -c opt //projects/animation_lab:animation_lab
#include "LabWindowPadd.hpp"

#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <cstdlib>

int main() {
  ScratchPadd::DebugLog::Session log; // (the terminal and a file)
  ScratchPadd::SystemImplementation<AnimationLab::LabWindowPadd> system;
  system.instantiate();
  system.start(); // returns when the window closes
  return EXIT_SUCCESS;
}
