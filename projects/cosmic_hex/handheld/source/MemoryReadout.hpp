// MemoryReadout.hpp - memory, measured (measuring before optimising,
// Gregory 10.9): with the counting allocator in the build, every 2 seconds
// the heap and the allocations a second, logged (a debug build's log) and,
// in the -watch builds, shown on the stage
#pragma once

#include "AppBasics.hpp"

#include <ScratchPadd/MemoryStats.hpp>

#include <optional>
#include <string>

namespace CosmicHex::Handheld::App {

class MemoryReadout {
public:
  // Each frame: measured again once 2 seconds have passed
  void update();
  // The heap: used, free and in how many pieces, the room left, and the
  // allocations a second
  const std::string &line() const { return line_; }
  // The three padds allocating most, each with its allocations a second
  const std::string &busiest() const { return busiest_; }

private:
  std::optional<ScratchPadd::MemoryStats::Reader> reader_;
  Clock::time_point at_;
  std::string line_, busiest_;
};

} // namespace CosmicHex::Handheld::App
