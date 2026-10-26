#pragma once

#include <ScratchPadd/Audio/MidiEvent.hpp>

#include <cstdint>

namespace ScratchPadd::Audio {

// An instrument that plays MIDI: the built-in one (BuiltInSynth) or a
// SoundFont (SoundFontSynth). Made on the owner's thread, which sets up
// everything it will need; after that only the audio thread uses it, so
// none of these may lock, allocate, free or wait.
class Synth {
public:
  virtual ~Synth() = default;

  virtual void handle(const MidiEvent &event) noexcept = 0;
  // Adds the next `frames` frames (interleaved stereo) to `stereo`
  virtual void render(float *stereo, std::uint32_t frames) noexcept = 0;
  // Releases every note, as a MIDI "all notes off" on every channel
  virtual void allNotesOff() noexcept = 0;
  // Notes sounding now, release tails included
  virtual std::uint32_t notesSounding() const noexcept = 0;
};

} // namespace ScratchPadd::Audio
