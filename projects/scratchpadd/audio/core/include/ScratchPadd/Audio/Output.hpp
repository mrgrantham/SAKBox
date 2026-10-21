#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

namespace ScratchPadd::Audio {

// Produces audio on demand: the engine's mixer. An Output calls render() on
// its audio thread, for the next `frames` frames of interleaved stereo
// (left, right, left, right, ...).
class Renderer {
public:
  // Must not lock, allocate, free or wait: it runs on the audio thread
  virtual void render(float *interleavedStereo, std::uint32_t frames) noexcept = 0;

protected:
  ~Renderer() = default;
};

// Where the engine's audio goes: a sound device (miniaudio on desktops, the
// 3DS's DSP), or nowhere until asked (OfflineOutput, for tests). Each one
// decides its sample rate and calls its Renderer from whatever thread it
// plays on.
class Output {
public:
  virtual ~Output() = default;

  // Frames per second the output plays, and so the rate the engine mixes at
  virtual std::uint32_t sampleRate() const = 0;

  // Starts calling `renderer`. Returns false, with the reason in `error`, if
  // the output can't start (no sound device, say).
  virtual bool start(Renderer &renderer, std::string &error) = 0;

  // Stops calling it; once this returns, render() isn't running and won't
  // be called again
  virtual void stop() = 0;
};

// An output with no device: nothing is mixed until render() asks for it, on
// the caller's thread. Tests use it to hear exactly what the engine makes,
// the same on every machine.
class OfflineOutput : public Output {
public:
  explicit OfflineOutput(std::uint32_t sampleRate = 48000) : sampleRate_(sampleRate) {}

  std::uint32_t sampleRate() const override { return sampleRate_; }
  bool start(Renderer &renderer, std::string &) override {
    renderer_.store(&renderer, std::memory_order_release);
    return true;
  }
  // (The caller makes sure render() isn't running on another thread.)
  void stop() override { renderer_.store(nullptr, std::memory_order_release); }

  // Mixes the next `frames` frames into `out` (2 floats a frame); silence if
  // not started. The thread that calls this is the audio thread, and may be
  // another than the one that started it.
  void render(float *out, std::uint32_t frames) {
    if (Renderer *renderer = renderer_.load(std::memory_order_acquire)) {
      renderer->render(out, frames);
    } else {
      std::fill(out, out + 2 * frames, 0.0f);
    }
  }

private:
  std::uint32_t sampleRate_;
  std::atomic<Renderer *> renderer_{nullptr};
};

} // namespace ScratchPadd::Audio
