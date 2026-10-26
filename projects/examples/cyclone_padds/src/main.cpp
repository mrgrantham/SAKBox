// Cyclone in padds: the ragdoll from Cyclone's demos, its physics on one
// thread and its window on another, talking only by messages.
//
//   RagdollPhysicsPadd (own thread)  steps the ragdoll in fixed steps and
//                                    sub-steps; its controls set them
//   RagdollWindowPadd  (main thread) draws its snapshots at the screen's rate,
//                                    blended; every padd's controls; keys
//
//   bazel run //projects/examples/cyclone_padds
#include "RagdollPhysicsPadd.hpp"
#include "RagdollWindowPadd.hpp"

#include <ScratchPadd/ScratchPadd.hpp>

#include <spdlog/spdlog.h>

int main() {
  spdlog::set_level(spdlog::level::warn);
  ScratchPadd::SystemImplementation<CyclonePadds::RagdollPhysicsPadd,
                                    CyclonePadds::RagdollWindowPadd>
      system;
  system.instantiate();
  system.start(); // returns when the window closes
  return 0;
}
