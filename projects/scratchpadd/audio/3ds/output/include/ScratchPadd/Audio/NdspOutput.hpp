#pragma once

#include <ScratchPadd/Audio/Output.hpp>

#include <3ds.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ScratchPadd::Audio {

// Counters NdspOutput keeps, readable from any thread
struct NdspStats {
  std::atomic<std::uint32_t> buffersMixed{0};
  std::atomic<std::uint32_t> buffersQueued{0}; // waiting for, or playing on, the DSP now
  std::atomic<std::uint32_t> underruns{0};     // times the DSP ran out of queued audio
  std::atomic<std::uint32_t> lastMixMicroseconds{0};
  std::atomic<std::uint32_t> maxMixMicroseconds{0};
};

// Plays through the 3DS's DSP with libctru's ndsp, on one stereo channel at
// the DSP's own rate (SYSCLOCK_SOC / 512, about 32,728 Hz), so nothing is
// resampled on the way out.
//
// Mixed audio goes into a few short wave buffers the DSP plays in turn. The
// DSP's callback (on its own thread, once every DSP frame) only wakes a
// mixing thread, which refills the buffers the DSP has finished: as SDL's
// 3DS audio does. That thread runs one priority step above the thread that
// starts the output, so mixing comes before the app's own work.
//
// ndsp needs the DSP's firmware, dumped from the console to
// sdmc:/3ds/dspfirm.cdc (with the DSP1 homebrew); without it start() says so.
class NdspOutput : public Output {
public:
  static constexpr int kBuffers = 4;
  static constexpr std::uint32_t kBufferFrames = 512; // 15.6 ms each

  NdspOutput();
  ~NdspOutput() override;

  NdspOutput(const NdspOutput &) = delete;
  NdspOutput &operator=(const NdspOutput &) = delete;

  std::uint32_t sampleRate() const override { return kRate; }
  bool start(Renderer &renderer, std::string &error) override;
  void stop() override;

  const NdspStats &stats() const { return stats_; }

private:
  static constexpr std::uint32_t kRate = 32728; // NDSP_SAMPLE_RATE, to the nearest frame
  static constexpr int kChannel = 0;

  static void dspFrame(void *self);  // the DSP's callback
  static void mixThread(void *self); // the mixing thread
  void refill() noexcept;

  Renderer *renderer_ = nullptr;
  bool ndspStarted_ = false;
  std::atomic<bool> running_{false};
  LightEvent wake_;
  Thread thread_ = nullptr;
  std::int16_t *samples_ = nullptr; // the wave buffers' audio, in linear memory
  ndspWaveBuf buffers_[kBuffers] = {};
  std::vector<float> mixed_; // one buffer, as the mixer makes it
  NdspStats stats_;
};

} // namespace ScratchPadd::Audio
