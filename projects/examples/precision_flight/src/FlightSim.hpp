// FlightSim.hpp - Precision Flight's simulation: the ship, the rail, the
// obstacles, powerups and the precision groups, advanced in fixed 1/60 s
// steps. No graphics or input code: steering is passed in and things that
// happen are reported as FlightEvents, so it runs (and is tested) headlessly.
#pragma once

#include "FlightMessages.hpp"

#include <FlexController.hpp>

#include <random>
#include <vector>

namespace PrecisionFlight {

// The precision lab: everything a player or a control panel can tune.
struct FlightSettings {
  int flightBase = 1, hullBase = 2, renderBase = 9; // fractional bits per group
  int focusFlightBits = 5;                          // Flight while FOCUS lasts
  int phaseHullBits = -1, phaseRenderBits = 3;      // Hull and Render while PHASE lasts
  float focusSeconds = 7.0f, phaseSeconds = 4.0f;
  Flex::Math::Rounding rounding = Flex::Math::Rounding::TowardZero;
  bool quantizeVelocity = false; // lossy velocity: small drag steps vanish
  bool lossyPosition = false;    // Flight policy Lossy: small moves vanish
  bool autopilot = false;        // steer toward the next gate or pickup
  float speed = 25.0f;           // rail speed, units per second
};

class FlightSim {
public:
  FlightSettings settings;
  bool paused = false;

  FlightSim() { reset(); }
  // Groups hold references into `precision`, so a FlightSim can't be copied.
  FlightSim(const FlightSim &) = delete;
  FlightSim &operator=(const FlightSim &) = delete;

  // Starts a new run with the current settings.
  void reset();

  // Runs as many whole steps as fit in `seconds` (plus time left over from
  // earlier calls), appending what happened to `events`. How frames slice the
  // time doesn't change the outcome: the steps are always 1/60 s.
  void advance(double seconds, float inputX, float inputY, std::vector<FlightEvent> &events);

  // One 1/60 s step. Does nothing while paused or after game over.
  void step(float inputX, float inputY, std::vector<FlightEvent> &events);

  WorldSnapshot snapshot() const;

  bool over() const { return over_; }
  double z() const { return z_; }
  int score() const { return score_; }
  float shield() const { return shield_; }
  const Flex::Math::FlexGroup &flight() const { return flight_; }
  const Flex::Math::FlexGroup &hull() const { return hull_; }
  const Flex::Math::FlexGroup &render() const { return render_; }

private:
  void updateGroups();
  float random(float lo, float hi);
  float randomOnGrid(float lo, float hi, float grid);
  void spawnSegment();
  void integrateAxis(Flex::Math::FlexPoint &p, Flex::Math::FlexPoint &v, float input, float lo,
                     float hi);
  void crash(Obstacle &o, float damage, bool wall, std::vector<FlightEvent> &events);
  void checkCollisions(std::vector<FlightEvent> &events);
  void autopilotInput(float &inputX, float &inputY) const;

  Flex::Math::FlexController precision_;
  Flex::Math::FlexGroup &flight_ = precision_.addGroup("Flight");
  Flex::Math::FlexGroup &hull_ = precision_.addGroup("Hull");
  Flex::Math::FlexGroup &render_ = precision_.addGroup("Render");
  Flex::Math::FlexOverride focusFlight_, phaseHull_, phaseRender_; // held while a powerup lasts
  Effect focus_, phase_;

  // Lateral physics state, stored at full Q32.32 precision
  Flex::Math::FlexPoint x_, y_, vx_, vy_;
  double z_ = 0.0;

  glm::vec2 observed_{0.0f};
  float observedZ_ = 0.0f;

  int score_ = 0;
  float shield_ = 100.0f;
  bool over_ = false;
  int gates_ = 0, goldGates_ = 0, phasedWalls_ = 0;

  std::vector<Obstacle> obstacles_;
  int nextObstacleId_ = 0;
  double nextSpawnZ_ = 0.0;
  std::mt19937 rng_;
  double accumulator_ = 0.0;
};

} // namespace PrecisionFlight
