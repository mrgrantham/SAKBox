#pragma once

#include <cstdint>

// The audio thread's rule: nothing that allocates while mixing (see Mixer).
// Mixer::render marks the thread it mixes on, and allocations that report
// themselves with noteAllocation() while it's marked are counted:
// TinySoundFont's do (SoundFontSynth.cpp), and tests report every `new`.
namespace ScratchPadd::Audio {

// Marks this thread as mixing while it exists
class MixingScope {
public:
  MixingScope() noexcept;
  ~MixingScope();
  MixingScope(const MixingScope &) = delete;
  MixingScope &operator=(const MixingScope &) = delete;

private:
  bool wasMixing_;
};

// Counts an allocation, if this thread is mixing
void noteAllocation() noexcept;

// Allocations counted so far, on any thread
std::uint32_t allocationsWhileMixing() noexcept;

} // namespace ScratchPadd::Audio
