#include <ScratchPadd/Audio/NdspOutput.hpp>

#include <algorithm>
#include <cstring>

namespace ScratchPadd::Audio {

namespace {
std::uint32_t microseconds(u64 ticks) {
  return static_cast<std::uint32_t>(ticks * 1000000ull / SYSCLOCK_ARM11);
}
} // namespace

NdspOutput::NdspOutput() : mixed_(2 * kBufferFrames) {}

NdspOutput::~NdspOutput() { stop(); }

bool NdspOutput::start(Renderer &renderer, std::string &error) {
  stop();
  const Result started = ndspInit();
  if (R_FAILED(started)) {
    if (R_MODULE(started) == RM_DSP && R_SUMMARY(started) == RS_NOTFOUND) {
      error = "the DSP firmware is missing: dump it to sdmc:/3ds/dspfirm.cdc with DSP1";
    } else {
      char code[16];
      std::snprintf(code, sizeof code, "%08lX", static_cast<unsigned long>(started));
      error = std::string("can't start the DSP (ndspInit: 0x") + code + ")";
    }
    return false;
  }
  ndspStarted_ = true;
  renderer_ = &renderer;

  // The wave buffers' audio must be in linear memory, which the DSP can read
  samples_ =
      static_cast<std::int16_t *>(linearAlloc(kBuffers * kBufferFrames * 2 * sizeof(std::int16_t)));
  if (!samples_) {
    error = "no linear memory for the audio buffers";
    stop();
    return false;
  }
  std::memset(samples_, 0, kBuffers * kBufferFrames * 2 * sizeof(std::int16_t));

  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  ndspChnReset(kChannel);
  ndspChnSetInterp(kChannel, NDSP_INTERP_NONE); // played at the rate it's mixed at
  ndspChnSetRate(kChannel, NDSP_SAMPLE_RATE);
  ndspChnSetFormat(kChannel, NDSP_FORMAT_STEREO_PCM16);
  float mix[12] = {1.0f, 1.0f}; // left to left, right to right; nothing else
  ndspChnSetMix(kChannel, mix);
  for (int i = 0; i < kBuffers; ++i) {
    buffers_[i] = {};
    buffers_[i].data_pcm16 = samples_ + i * kBufferFrames * 2;
    buffers_[i].nsamples = kBufferFrames;
    buffers_[i].status = NDSP_WBUF_DONE; // ready to be filled
  }

  LightEvent_Init(&wake_, RESET_ONESHOT);
  running_ = true;
  refill(); // queue every buffer before the DSP asks
  s32 priority = 0x30;
  svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
  thread_ = threadCreate(&NdspOutput::mixThread, this, 32 * 1024,
                         std::clamp<s32>(priority - 1, 0x18, 0x3F), -2, false);
  if (!thread_) {
    error = "can't start the mixing thread";
    stop();
    return false;
  }
  ndspSetCallback(&NdspOutput::dspFrame, this);
  return true;
}

void NdspOutput::stop() {
  if (thread_) {
    running_ = false;
    LightEvent_Signal(&wake_);
    threadJoin(thread_, U64_MAX);
    threadFree(thread_);
    thread_ = nullptr;
  }
  running_ = false;
  if (ndspStarted_) {
    ndspSetCallback(nullptr, nullptr);
    ndspChnWaveBufClear(kChannel);
    ndspExit();
    ndspStarted_ = false;
  }
  if (samples_) {
    linearFree(samples_);
    samples_ = nullptr;
  }
  renderer_ = nullptr;
}

void NdspOutput::dspFrame(void *self) {
  LightEvent_Signal(&static_cast<NdspOutput *>(self)->wake_);
}

void NdspOutput::mixThread(void *self) {
  auto &output = *static_cast<NdspOutput *>(self);
  while (output.running_) {
    LightEvent_Wait(&output.wake_);
    output.refill();
  }
}

void NdspOutput::refill() noexcept {
  int done = 0;
  for (ndspWaveBuf &buffer : buffers_) {
    done += buffer.status == NDSP_WBUF_DONE || buffer.status == NDSP_WBUF_FREE;
  }
  // Every buffer played and none left queued: the DSP ran out (not counted
  // the first time, before anything was queued)
  if (done == kBuffers && stats_.buffersMixed.load(std::memory_order_relaxed) > 0) {
    stats_.underruns.fetch_add(1, std::memory_order_relaxed);
  }
  for (ndspWaveBuf &buffer : buffers_) {
    if (!running_ || (buffer.status != NDSP_WBUF_DONE && buffer.status != NDSP_WBUF_FREE)) {
      continue;
    }
    const u64 began = svcGetSystemTick();
    renderer_->render(mixed_.data(), kBufferFrames);
    for (std::uint32_t i = 0; i < 2 * kBufferFrames; ++i) {
      const float sample = std::clamp(mixed_[i], -1.0f, 1.0f);
      buffer.data_pcm16[i] = static_cast<std::int16_t>(sample * 32767.0f);
    }
    const std::uint32_t took = microseconds(svcGetSystemTick() - began);
    stats_.lastMixMicroseconds.store(took, std::memory_order_relaxed);
    if (took > stats_.maxMixMicroseconds.load(std::memory_order_relaxed)) {
      stats_.maxMixMicroseconds.store(took, std::memory_order_relaxed);
    }
    // The DSP reads memory, not the CPU's cache
    DSP_FlushDataCache(buffer.data_pcm16, kBufferFrames * 2 * sizeof(std::int16_t));
    ndspChnWaveBufAdd(kChannel, &buffer);
    stats_.buffersMixed.fetch_add(1, std::memory_order_relaxed);
  }
  int queued = 0;
  for (const ndspWaveBuf &buffer : buffers_) {
    queued += buffer.status == NDSP_WBUF_QUEUED || buffer.status == NDSP_WBUF_PLAYING;
  }
  stats_.buffersQueued.store(static_cast<std::uint32_t>(queued), std::memory_order_relaxed);
}

} // namespace ScratchPadd::Audio
