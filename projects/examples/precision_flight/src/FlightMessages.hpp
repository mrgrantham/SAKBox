// FlightMessages.hpp - data shared by Precision Flight's padds: game
// constants, the world state the simulation publishes each frame, the input
// the display sends, and the events the simulation reports. Plain structs with
// no ScratchPadd, GL or ImGui dependency, so the simulation and its tests can
// use them headlessly.
#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdio>
#include <string>
#include <variant>
#include <vector>

namespace PrecisionFlight {

constexpr float kLaneHalfWidth = 6.0f;
constexpr float kLaneMinY = -3.0f;
constexpr float kLaneMaxY = 3.5f;
constexpr float kGroundY = -4.5f;
constexpr float kCeilingY = 5.5f;
constexpr float kShipHalfX = 0.55f;
constexpr float kShipHalfY = 0.25f;
constexpr float kShipHalfZ = 0.3f;
constexpr float kWallHalfZ = 0.1f;
constexpr float kGateBar = 0.3f;
constexpr double kStepSeconds = 1.0 / 60.0; // fixed simulation step

enum class Kind { Gate, GoldGate, PhaseWall, FocusPickup, PhasePickup };

struct Obstacle {
  int id = 0; // unique since the last reset; events refer to obstacles by id
  Kind kind;
  float z;
  float cx = 0.0f; // hole centre for gates, position for pickups
  float cy = 0.0f;
  float w = 0.0f; // hole size for gates
  float h = 0.0f;
  bool resolved = false; // scored, crashed, collected or left behind
  bool touched = false;  // the hull sampled a position overlapping it
};

// A timed powerup effect.
struct Effect {
  float remaining = 0.0f;
  float duration = 0.0f;
  bool active() const { return remaining > 0.0f; }
};

// The effective precision of one group this step, after powerup overrides.
struct GroupBits {
  std::string name;
  int fracBits;
};

// ---------------------------------------------------------------------------
// Display -> simulation

// Sent by the display once per frame: how much real time passed and the
// player's steering (-1..1 per axis; ignored while the autopilot flies).
struct FrameInput {
  double seconds = 0.0;
  float inputX = 0.0f;
  float inputY = 0.0f;
};

// One-off commands from key presses.
struct GameCommand {
  enum class Type { TogglePause, Restart, ToggleAutopilot };
  Type type;
};

// ---------------------------------------------------------------------------
// Simulation -> display, once per frame

struct WorldSnapshot {
  double z = 0.0;           // rail position; the ship flies toward -z
  glm::vec2 observed{0.0f}; // lateral position as physics sees it (Flight)
  float observedZ = 0.0f;   // rail position as collisions see it (Hull)
  float vx = 0.0f, vy = 0.0f;
  std::vector<Obstacle> obstacles;
  int score = 0;
  float shield = 100.0f;
  int gates = 0, goldGates = 0, phasedWalls = 0;
  Effect focus, phase;
  std::array<GroupBits, 3> groups; // Flight, Hull, Render
  int renderBits = 9;              // effective Render precision
  bool paused = false, over = false, autopilot = false;
};

// ---------------------------------------------------------------------------
// Simulation events. `distance` is how far along the rail it happened.

struct GatePassed {
  int obstacleId;
  bool gold;
  int points;
  double distance;
};
struct Crashed {
  int obstacleId;
  bool wall; // an energy wall, otherwise a gate's frame
  float damage;
  double distance;
};
struct PowerupCollected {
  int obstacleId;
  Kind kind; // FocusPickup or PhasePickup
  double distance;
};
struct WallPhased {
  int obstacleId;
  int points;
  double distance;
};
struct GameOver {
  double distance;
};

using FlightEvent = std::variant<GatePassed, Crashed, PowerupCollected, WallPhased, GameOver>;

// The text shown and logged for an event, e.g. "GATE  +100".
inline std::string describe(const FlightEvent &event) {
  struct Describe {
    std::string operator()(const GatePassed &e) const {
      return e.gold ? "GOLD GATE  +300" : "GATE  +100";
    }
    std::string operator()(const Crashed &e) const {
      char text[64];
      std::snprintf(text, sizeof(text), "%s  -%.0f shield", e.wall ? "WALL HIT" : "CLIPPED GATE",
                    e.damage);
      return text;
    }
    std::string operator()(const PowerupCollected &e) const {
      return e.kind == Kind::FocusPickup ? "FOCUS  flight precision up" : "PHASE  hull precision down";
    }
    std::string operator()(const WallPhased &) const { return "PHASED  +150"; }
    std::string operator()(const GameOver &) const { return "GAME OVER - press R"; }
  };
  return std::visit(Describe{}, event);
}

inline double distanceOf(const FlightEvent &event) {
  return std::visit([](const auto &e) { return e.distance; }, event);
}

// The log line for an event: "[  103.3] FOCUS  flight precision up".
inline std::string logLine(const FlightEvent &event) {
  char line[96];
  std::snprintf(line, sizeof(line), "[%7.1f] %s", distanceOf(event), describe(event).c_str());
  return line;
}

} // namespace PrecisionFlight
