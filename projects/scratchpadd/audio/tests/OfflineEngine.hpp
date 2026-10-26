#pragma once

// An audio engine playing into an OfflineOutput, which a test renders: it
// hears exactly what the engine mixes, on the test's own thread.

#include <ScratchPadd/Audio/Engine.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace OfflineEngine {

using namespace ScratchPadd::Audio;

inline constexpr std::uint32_t kRate = 48000;

// An engine playing into an offline output, which the test renders
struct Offline {
  OfflineOutput output{kRate};
  Engine engine;
  explicit Offline(const EngineConfig &config = {}) : engine(output, config) {
    std::string error;
    EXPECT_TRUE(engine.start(error)) << error;
  }
  // The next `frames` frames, interleaved stereo
  std::vector<float> render(std::uint32_t frames) {
    std::vector<float> out(2 * frames);
    output.render(out.data(), frames);
    return out;
  }
  // Loads stereo samples at the engine's rate (copied as they are)
  void load(const std::string &name, const std::vector<float> &stereo) {
    const LoadResult result = engine.loadSamples(name, stereo.data(), stereo.size(), kRate, 2);
    ASSERT_TRUE(result.ok) << result.error;
  }
};

// The voice a play call started, failing the test (with `why`) if it
// started none
inline VoiceId started(std::optional<VoiceId> voice, const std::string &why = "no voice started") {
  if (!voice) {
    ADD_FAILURE() << why;
    return 0;
  }
  return *voice;
}

} // namespace OfflineEngine
