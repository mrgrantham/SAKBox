// Cyclone in padds: the ragdoll from Cyclone's demos, its physics on one
// thread and its window on another, talking only by messages.
//
//   RagdollPhysicsPadd (own thread)  steps the ragdoll in fixed steps and
//                                    sub-steps; its controls set them
//   RagdollWindowPadd  (main thread) draws its snapshots at the screen's rate,
//                                    blended; every padd's controls; keys
//   AudioPadd          (own thread)  plays the knocks the physics padd sends
//
//   bazel run //projects/examples/cyclone_padds
#include "RagdollPhysicsPadd.hpp"
#include "RagdollWindowPadd.hpp"

#include <AudioPadd.hpp>

#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Storage/StoragePadd.hpp>

#include <spdlog/spdlog.h>

int main() {
  // The log in a file, the terminal showing only warnings
  ScratchPadd::DebugLog::Session log({.terminalLevel = spdlog::level::warn});
  ScratchPadd::SystemImplementation<CyclonePadds::RagdollPhysicsPadd,
                                    CyclonePadds::RagdollWindowPadd, AudioPadd,
                                    ScratchPadd::Storage::StoragePadd>
      system;
  system.instantiate();
  system.start(); // returns when the window closes
  return 0;
}
