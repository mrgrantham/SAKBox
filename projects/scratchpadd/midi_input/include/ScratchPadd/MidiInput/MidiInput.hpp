#pragma once

#include <ScratchPadd/Audio/MidiEvent.hpp>

#include <string>

namespace ScratchPadd::MidiInput {

// A MIDI event from a keyboard or controller, broadcast by MidiInputPadd to
// every padd that handles it
struct MidiInput {
  Audio::MidiEvent event;
  std::string port; // the MIDI port it came in on, by name
};

} // namespace ScratchPadd::MidiInput
