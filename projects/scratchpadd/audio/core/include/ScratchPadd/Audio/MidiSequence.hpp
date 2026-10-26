#pragma once

#include <ScratchPadd/Audio/MidiEvent.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ScratchPadd::Audio {

// A piece of MIDI ready to play: its events, timed in frames at the
// engine's rate, in order
struct MidiSequence {
  struct Timed {
    std::uint32_t frame;
    MidiEvent event;
  };
  std::vector<Timed> events;
  std::uint32_t frames = 0; // its length: past the last event
};

// Reads a standard MIDI file (.mid) as a sequence at `sampleRate`, or
// nothing, with the reason in `error`
std::shared_ptr<const MidiSequence> readMidiFile(const std::string &path, std::uint32_t sampleRate,
                                                 std::string &error);
// The same, from a MIDI file's bytes
std::shared_ptr<const MidiSequence> readMidi(const void *data, std::size_t size,
                                             std::uint32_t sampleRate, std::string &error);

} // namespace ScratchPadd::Audio
