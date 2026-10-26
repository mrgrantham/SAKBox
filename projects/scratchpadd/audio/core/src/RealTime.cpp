#include <ScratchPadd/Audio/RealTime.hpp>

#include <atomic>

namespace ScratchPadd::Audio {

namespace {
thread_local bool mixing = false;
std::atomic<std::uint32_t> counted{0};
} // namespace

MixingScope::MixingScope() noexcept : wasMixing_(mixing) { mixing = true; }

MixingScope::~MixingScope() { mixing = wasMixing_; }

void noteAllocation() noexcept {
  if (mixing) {
    counted.fetch_add(1, std::memory_order_relaxed);
  }
}

std::uint32_t allocationsWhileMixing() noexcept { return counted.load(std::memory_order_relaxed); }

} // namespace ScratchPadd::Audio
