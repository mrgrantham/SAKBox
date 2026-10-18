// FlightSim runs headlessly, so the game's rules can be checked without a
// window. The golden logs in tests/data were recorded from the original
// single-file game (before the simulation was extracted) by flying 120 s on
// autopilot; the extracted simulation must reproduce them exactly.
#include "FlightSim.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace PrecisionFlight;

namespace {

constexpr int kStepsIn120Seconds = 120 * 60;

std::string readFile(const std::string &path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.good()) << "missing " << path;
  std::stringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

// The original game's settings for the "variant" golden log.
void useVariantSettings(FlightSim &sim) {
  sim.settings.rounding = Flex::Math::Rounding::Nearest;
  sim.settings.lossyPosition = true;
  sim.settings.quantizeVelocity = true;
  sim.settings.flightBase = 3;
  sim.settings.hullBase = 1;
  sim.settings.speed = 40.0f;
}

// Flies 120 s on autopilot and returns the log in the golden files' format.
std::string autopilotLog(FlightSim &sim) {
  sim.settings.autopilot = true;
  std::vector<FlightEvent> events;
  for (int i = 0; i < kStepsIn120Seconds; ++i) {
    sim.step(0.0f, 0.0f, events);
  }
  std::string log;
  for (const FlightEvent &event : events) {
    log += logLine(event) + "\n";
  }
  const WorldSnapshot s = sim.snapshot();
  char end[128];
  std::snprintf(end, sizeof(end), "END score=%d shield=%.1f gates=%d gold=%d phased=%d over=%d z=%.3f\n",
                s.score, s.shield, s.gates, s.goldGates, s.phasedWalls, s.over ? 1 : 0, s.z);
  return log + end;
}

// Events from flying `seconds` on autopilot with the given frame lengths
// (cycled), as the display would send them.
std::vector<std::string> autopilotEventsWithFrames(const std::vector<double> &frames, double seconds) {
  FlightSim sim;
  sim.settings.autopilot = true;
  std::vector<FlightEvent> events;
  double elapsed = 0.0;
  for (size_t i = 0; elapsed < seconds; ++i) {
    const double frame = frames[i % frames.size()];
    sim.advance(frame, 0.0f, 0.0f, events);
    elapsed += frame;
  }
  std::vector<std::string> lines;
  lines.reserve(events.size());
  for (const FlightEvent &event : events) {
    lines.push_back(logLine(event));
  }
  return lines;
}

} // namespace

TEST(FlightSimTest, MatchesOriginalGameOnAutopilot) {
  FlightSim sim;
  EXPECT_EQ(autopilotLog(sim),
            readFile("projects/examples/precision_flight/tests/data/autopilot_events.txt"));
}

TEST(FlightSimTest, MatchesOriginalGameWithVariantSettings) {
  FlightSim sim;
  useVariantSettings(sim);
  EXPECT_EQ(autopilotLog(sim),
            readFile("projects/examples/precision_flight/tests/data/autopilot_events_variant.txt"));
}

// The simulation always takes 1/60 s steps, so how frames slice time doesn't
// change what happens (the last frame may hold one step more or less).
TEST(FlightSimTest, FrameLengthsDontChangeEvents) {
  const auto oneStepFrames = autopilotEventsWithFrames({kStepSeconds}, 60.0);
  const auto fourStepFrames = autopilotEventsWithFrames({4 * kStepSeconds}, 60.0);
  const auto unevenFrames = autopilotEventsWithFrames({0.007, 0.031, 0.016, 0.050}, 60.0);
  ASSERT_GT(oneStepFrames.size(), 10u);
  for (const auto *other : {&fourStepFrames, &unevenFrames}) {
    const size_t common = std::min(oneStepFrames.size(), other->size());
    EXPECT_LE(std::max(oneStepFrames.size(), other->size()) - common, 1u);
    EXPECT_TRUE(std::equal(oneStepFrames.begin(), oneStepFrames.begin() + common, other->begin()));
  }
}

// FOCUS raises Flight precision while it lasts; PHASE lowers Hull and Render.
TEST(FlightSimTest, PowerupsOverrideGroupPrecision) {
  FlightSim sim;
  sim.settings.autopilot = true;
  bool sawFocus = false, sawPhase = false;
  std::vector<FlightEvent> events;
  for (int i = 0; i < kStepsIn120Seconds && !(sawFocus && sawPhase); ++i) {
    events.clear();
    sim.step(0.0f, 0.0f, events);
    for (const FlightEvent &event : events) {
      const auto *powerup = std::get_if<PowerupCollected>(&event);
      if (!powerup) {
        continue;
      }
      // Overrides apply from the next step
      std::vector<FlightEvent> next;
      sim.step(0.0f, 0.0f, next);
      const WorldSnapshot s = sim.snapshot();
      if (powerup->kind == Kind::FocusPickup) {
        sawFocus = true;
        EXPECT_TRUE(s.focus.active());
        EXPECT_EQ(s.groups[0].fracBits, sim.settings.focusFlightBits);
      } else {
        sawPhase = true;
        EXPECT_TRUE(s.phase.active());
        EXPECT_EQ(s.groups[1].fracBits, sim.settings.phaseHullBits);
        EXPECT_EQ(s.renderBits, sim.settings.phaseRenderBits);
      }
    }
  }
  EXPECT_TRUE(sawFocus);
  EXPECT_TRUE(sawPhase);
}

// Without powerups the groups use the lab's base precision, and settings
// changes apply from the next step.
TEST(FlightSimTest, SettingsApplyToGroups) {
  FlightSim sim;
  std::vector<FlightEvent> events;
  sim.step(0.0f, 0.0f, events);
  EXPECT_EQ(sim.snapshot().groups[0].fracBits, 1);
  EXPECT_EQ(sim.snapshot().groups[1].fracBits, 2);
  EXPECT_EQ(sim.snapshot().renderBits, 9);

  sim.settings.flightBase = 7;
  sim.step(0.0f, 0.0f, events);
  EXPECT_EQ(sim.snapshot().groups[0].fracBits, 7);
}

// Pausing stops the clock; reset starts the same run over.
TEST(FlightSimTest, PauseAndReset) {
  FlightSim sim;
  std::vector<FlightEvent> events;
  const WorldSnapshot start = sim.snapshot();

  sim.paused = true;
  sim.advance(1.0, 1.0f, 0.0f, events);
  EXPECT_EQ(sim.snapshot().z, start.z);

  sim.paused = false;
  sim.advance(1.0, 1.0f, 0.0f, events);
  EXPECT_LT(sim.snapshot().z, start.z);

  sim.reset();
  const WorldSnapshot again = sim.snapshot();
  EXPECT_EQ(again.z, 0.0);
  EXPECT_EQ(again.score, 0);
  ASSERT_EQ(again.obstacles.size(), start.obstacles.size());
  for (size_t i = 0; i < again.obstacles.size(); ++i) {
    EXPECT_EQ(again.obstacles[i].z, start.obstacles[i].z);
    EXPECT_EQ(again.obstacles[i].cx, start.obstacles[i].cx);
  }
}
