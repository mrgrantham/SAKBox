#include "MemoryReadout.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdio>

namespace CosmicHex::Handheld::App {

void MemoryReadout::update() {
  if (!ScratchPadd::MemoryStats::linked())
    return;
  if (!reader_) {
    reader_.emplace();
    at_ = Clock::now();
    return;
  }
  const double seconds = secondsSince(at_);
  if (seconds < 2)
    return;
  at_ = Clock::now();
  const ScratchPadd::MemoryStats::Snapshot counts = reader_->take();
  const ScratchPadd::MemoryStats::Heap heap = ScratchPadd::MemoryStats::heap();
  char line[96];
  // (used, free in the heap and in how many pieces, and on a DS the room
  // left to allocate)
  std::snprintf(line, sizeof line, "%zuK used %zuK free/%zu room %zuK %.0f/s", heap.inUse / 1024,
                heap.free / 1024, heap.freeChunks, heap.roomKnown ? heap.room / 1024 : 0,
                static_cast<double>(counts.all.allocations) / seconds);
  line_ = line;
  std::string busiest;
  for (std::size_t i = 0; i < counts.padds.size() && i < 3; ++i) {
    char padd[48];
    std::snprintf(padd, sizeof padd, " %s %.0f/s", counts.padds[i].name.c_str(),
                  static_cast<double>(counts.padds[i].counts.allocations) / seconds);
    busiest += padd;
  }
  spdlog::info("memory: {};{}", line_, busiest);
  busiest_ = busiest;
}

} // namespace CosmicHex::Handheld::App
