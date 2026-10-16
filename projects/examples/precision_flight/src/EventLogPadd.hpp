// EventLogPadd.hpp - logs the simulation's events as "[distance] text" lines,
// on its own thread so terminal output never stalls a frame.
#pragma once

#include "FlightMessages.hpp"

#include <ScratchPadd/Base.hpp>

#include <cstdio>
#include <functional>
#include <string>

namespace PrecisionFlight {

class EventLogPadd : public ScratchPadd::Base {
public:
  // Where lines go: stdout by default; tests replace it to capture them.
  static inline std::function<void(const std::string &)> output = [](const std::string &line) {
    std::printf("%s\n", line.c_str());
    std::fflush(stdout);
  };

  EventLogPadd(ScratchPadd::System *system) : Base(system) {
    logEvents<GatePassed, Crashed, PowerupCollected, WallPhased, GameOver>();
  }

  std::string name() override { return "EventLog"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

private:
  template <typename... Events> void logEvents() {
    (receive<Events>([](const Events &event) { output(logLine(event)); }), ...);
  }
};

} // namespace PrecisionFlight
