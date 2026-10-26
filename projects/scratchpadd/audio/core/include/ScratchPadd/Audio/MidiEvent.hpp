#pragma once

#include <algorithm>
#include <cstdint>

// One MIDI message: what a MIDI file, a MIDI keyboard or a program tells an
// instrument. Shared by the engine and the messages that drive it.
namespace ScratchPadd::Audio {

struct MidiEvent {
  // The kinds of message, as MIDI numbers them (the status byte's high half)
  enum Kind : std::uint8_t {
    NoteOff = 0x80,
    NoteOn = 0x90,
    KeyPressure = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    ChannelPressure = 0xD0,
    PitchBend = 0xE0,
  };

  std::uint8_t status = 0; // the kind, plus the channel (0-15) in the low half
  std::uint8_t data1 = 0;  // key, controller or program (0-127)
  std::uint8_t data2 = 0;  // velocity or value (0-127)

  std::uint8_t kind() const { return status & 0xF0; }
  std::uint8_t channel() const { return status & 0x0F; }

  // `channel` 0-15 (General MIDI's drums are channel 9), key 0-127 (60 is
  // middle C), velocity 1-127
  static MidiEvent noteOn(int channel, int key, int velocity) {
    return make(NoteOn, channel, key, velocity);
  }
  static MidiEvent noteOff(int channel, int key) { return make(NoteOff, channel, key, 0); }
  static MidiEvent controlChange(int channel, int controller, int value) {
    return make(ControlChange, channel, controller, value);
  }
  // A General MIDI instrument, 0-127 (0 is a piano)
  static MidiEvent programChange(int channel, int program) {
    return make(ProgramChange, channel, program, 0);
  }
  // -8192 to 8191; 0 is in tune
  static MidiEvent pitchBend(int channel, int bend) {
    const int value = std::clamp(bend + 8192, 0, 16383);
    return make(PitchBend, channel, value & 0x7F, value >> 7);
  }

private:
  static MidiEvent make(Kind kind, int channel, int data1, int data2) {
    return {.status = static_cast<std::uint8_t>(kind | (channel & 0x0F)),
            .data1 = static_cast<std::uint8_t>(data1 & 0x7F),
            .data2 = static_cast<std::uint8_t>(data2 & 0x7F)};
  }
};

} // namespace ScratchPadd::Audio
