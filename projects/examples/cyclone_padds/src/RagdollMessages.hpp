// RagdollMessages.hpp - what the Cyclone padds send each other: the
// physics padd's snapshots of the ragdoll, and the window's commands. Plain
// structs with no Cyclone, GL or ImGui dependency.
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <vector>

namespace CyclonePadds {

// How often the physics padd wakes to step and send a snapshot
constexpr double kPhysicsTickSeconds = 1.0 / 60;

// One bone: a box, where it is and how it's turned
struct BoneSnapshot {
  glm::vec3 position{0.0f};
  glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 halfSize{0.5f};
  bool awake = true;
};

// One joint: a point on each of two bones (in the bone's own coordinates),
// held no more than `slack` apart. The window works out where they are from
// the bones it draws.
struct JointSnapshot {
  int bones[2] = {0, 0};
  glm::vec3 points[2]{glm::vec3(0.0f), glm::vec3(0.0f)};
  float slack = 0;
};

// What the physics padd measured, for the window to show
struct PhysicsStats {
  int stepsPerSecond = 60;
  int subSteps = 1;
  // The share of one CPU core the physics took over the last second
  double busy = 0;
  // The most any joint was beyond its slack after a step: now, and the most
  // since the ragdoll was last reset (m)
  double stretchNow = 0;
  double worstStretch = 0;
  // Knocks sent to AudioPadd so far, and whether there's one to send them to
  std::uint32_t knocks = 0;
  bool audio = false;
};

// The ragdoll after a physics padd tick: sent each tick that took a step.
struct RagdollSnapshot {
  // Simulated seconds since the padd started (keeps counting across
  // resets), and the ragdoll's reset count, so the window doesn't blend
  // between a ragdoll and its reset
  double time = 0;
  std::uint32_t generation = 0;
  std::vector<BoneSnapshot> bones;
  std::vector<JointSnapshot> joints;
  PhysicsStats stats;
};

// The window's camera has turned: its angle around the vertical (degrees,
// looking at the origin from +x when 0), so knocks are panned to where they
// are on screen
struct CameraTurned {
  float degrees = 0;
};

// Keys in the window, for the physics padd
struct RagdollCommand {
  enum class Type { Reset, TogglePause };
  Type type = Type::Reset;
};

} // namespace CyclonePadds
