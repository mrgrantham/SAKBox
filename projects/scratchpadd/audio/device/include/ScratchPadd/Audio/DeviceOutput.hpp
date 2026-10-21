#pragma once

#include <ScratchPadd/Audio/Output.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace ScratchPadd::Audio {

// Plays through the system's default sound device with miniaudio: Core
// Audio on macOS, PulseAudio, ALSA or JACK on Linux, WASAPI on Windows. If
// there's no sound device, miniaudio's null backend plays silently in real
// time instead, so the engine still runs (voices finish, music streams).
class DeviceOutput : public Output {
public:
  explicit DeviceOutput(std::uint32_t sampleRate = 48000);
  ~DeviceOutput() override;

  std::uint32_t sampleRate() const override { return sampleRate_; }
  bool start(Renderer &renderer, std::string &error) override;
  void stop() override;

  // The backend in use (e.g. "Core Audio", "Null"), once started
  std::string backendName() const;

private:
  struct Device; // miniaudio's device (DeviceOutput.cpp)
  std::uint32_t sampleRate_;
  std::unique_ptr<Device> device_;
};

} // namespace ScratchPadd::Audio
