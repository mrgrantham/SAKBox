// FlightSimPadd.hpp - runs the simulation inside ScratchPadd.
//
// Each FrameInput from the display advances the simulation by that frame's
// time and is answered with the frame's events, then one WorldSnapshot. The
// precision lab's settings are this padd's controls, so the display's control
// panel edits them. Runs on the main thread, ahead of the display padd.
#pragma once

#include "FlightSim.hpp"

#include <ScratchPadd/Base.hpp>

#include <string>
#include <vector>

namespace PrecisionFlight {

// Command-line options, read when the padds are constructed.
struct FlightOptions {
  static inline bool autopilot = false; // --autopilot
};

class FlightSimPadd : public ScratchPadd::Base {
public:
  FlightSimPadd(ScratchPadd::System *system) : Base(system) {
    sim_.settings.autopilot = FlightOptions::autopilot;
    receive<FrameInput>([this](const FrameInput &input) { runFrame(input); });
    receive<GameCommand>([this](const GameCommand &command) { runCommand(command); });
  }

  std::string name() override { return "FlightSim"; }
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }

  // The precision lab, with today's defaults and ranges.
  ScratchPadd::ControlValueMap generateControls() override {
    namespace Controls = ScratchPadd::ControlType;
    const FlightSettings &s = sim_.settings;
    auto bits = [](int value, int lowest) {
      return Controls::Integer(value, std::make_pair(lowest, 12));
    };
    return {
        {"Flight bits", bits(s.flightBase, -2)},
        {"Hull bits", bits(s.hullBase, -2)},
        {"Render bits", bits(s.renderBase, 1)},
        {"FOCUS flight bits", bits(s.focusFlightBits, -2)},
        {"PHASE hull bits", bits(s.phaseHullBits, -2)},
        {"PHASE render bits", bits(s.phaseRenderBits, 1)},
        {"Rounding", Controls::String("truncate", std::vector<std::string>{"truncate", "nearest",
                                                                           "stochastic"})},
        {"Lossy velocity", Controls::Boolean(s.quantizeVelocity)},
        {"Lossy position", Controls::Boolean(s.lossyPosition)},
        {"Autopilot", Controls::Boolean(s.autopilot)},
        {"Rail speed", Controls::Double(s.speed, std::make_pair(5.0, 60.0))},
    };
  }

  void controlChanged(const std::string &) override { applyControls(); }

  // For tests
  const FlightSim &sim() const { return sim_; }

private:
  void runFrame(const FrameInput &input) {
    std::vector<FlightEvent> events;
    sim_.advance(input.seconds, input.inputX, input.inputY, events);
    for (const FlightEvent &event : events) {
      std::visit([this](const auto &e) { send(e); }, event);
    }
    send(sim_.snapshot());
  }

  void runCommand(const GameCommand &command) {
    switch (command.type) {
    case GameCommand::Type::TogglePause:
      sim_.paused = !sim_.paused;
      break;
    case GameCommand::Type::Restart:
      sim_.reset();
      break;
    case GameCommand::Type::ToggleAutopilot:
      // Changed by the owner itself; setControl tells the panels
      setControl("Autopilot", ScratchPadd::ControlType::Boolean(
                                  !control<ScratchPadd::ControlType::Boolean>("Autopilot").value));
      applyControls();
      break;
    }
  }

  // Copies the controls into the simulation's settings; they take effect
  // from the next step.
  void applyControls() {
    namespace Controls = ScratchPadd::ControlType;
    FlightSettings &s = sim_.settings;
    s.flightBase = control<Controls::Integer>("Flight bits").value;
    s.hullBase = control<Controls::Integer>("Hull bits").value;
    s.renderBase = control<Controls::Integer>("Render bits").value;
    s.focusFlightBits = control<Controls::Integer>("FOCUS flight bits").value;
    s.phaseHullBits = control<Controls::Integer>("PHASE hull bits").value;
    s.phaseRenderBits = control<Controls::Integer>("PHASE render bits").value;
    const std::string &rounding = control<Controls::String>("Rounding").value;
    s.rounding = rounding == "nearest"      ? Flex::Math::Rounding::Nearest
                 : rounding == "stochastic" ? Flex::Math::Rounding::Stochastic
                                            : Flex::Math::Rounding::TowardZero;
    s.quantizeVelocity = control<Controls::Boolean>("Lossy velocity").value;
    s.lossyPosition = control<Controls::Boolean>("Lossy position").value;
    s.autopilot = control<Controls::Boolean>("Autopilot").value;
    s.speed = static_cast<float>(control<Controls::Double>("Rail speed").value);
  }

  FlightSim sim_;
};

} // namespace PrecisionFlight
