#pragma once

#include <cstddef>
#include <cstdint>

// How a voice plays: shared by the engine and the messages that drive it
// (AudioPadd's), so a padd can ask for a sound without the engine itself.
namespace ScratchPadd::Audio {

// The mix groups, each with its own volume under the master volume
enum class Group : std::uint8_t { Sounds, Music };
constexpr std::size_t kGroups = 2;

struct VoiceSettings {
  float volume = 1.0f; // 0 to anything; 1 is as recorded
  float pan = 0.0f;    // balance: -1 left only, 0 both as recorded, 1 right only
  float pitch = 1.0f;  // playback speed: 2 is an octave up and half as long
  bool loop = false;
};

using VoiceId = std::uint32_t; // 0 is never a voice

} // namespace ScratchPadd::Audio
