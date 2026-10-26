#pragma once

// The messages AudioPadd handles, and its answers. A padd that plays sounds
// needs only these (the :audio_messages library), not the engine.
//
// Answers go back to the padd that asked, with reply(): handle the answer's
// type to hear it, or don't. Answers come back in the order things were
// asked, so a padd that asks twice can tell its answers apart.

#include <ScratchPadd/Audio/MidiEvent.hpp>
#include <ScratchPadd/Audio/Voice.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ScratchPadd::Audio {

// Loads a WAV, FLAC or MP3 file as sound `name` (replacing any sound of that
// name). Answered with SoundLoaded.
struct LoadSound {
  std::string name;
  std::string path;
};

// Loads samples made in code as sound `name`: interleaved, `channels` per
// frame, `rate` frames a second. Shared, so sending them copies nothing.
// Answered with SoundLoaded.
struct LoadSoundSamples {
  std::string name;
  std::shared_ptr<const std::vector<float>> samples;
  std::uint32_t rate = 48000;
  std::uint32_t channels = 1;
};

// The same with 16-bit samples, such as MadeSoundsPcm16's, rendered when the
// program is built: where float is slow (the DS, with no floating-point
// hardware) this loads with no float work at all. Answered with SoundLoaded.
struct LoadSoundPcm16 {
  std::string name;
  std::shared_ptr<const std::vector<std::int16_t>> samples;
  std::uint32_t rate = 48000;
  std::uint32_t channels = 1;
};

struct SoundLoaded {
  std::string name;
  bool ok = false;
  std::string error; // why not, if not ok
};

// Unloads a sound; its voices stop at once
struct UnloadSound {
  std::string name;
};

// Plays a loaded sound. Answered with Playing; when it ends (or is
// stopped), the padd that asked gets VoiceFinished.
struct PlaySound {
  std::string name;
  VoiceSettings settings;
};

// Streams a music file, in the Music group. Answered like PlaySound, with
// the path as the name.
struct PlayMusic {
  std::string path;
  VoiceSettings settings;
};

struct Playing {
  std::string name;
  VoiceId voice = 0; // 0 if not ok
  bool ok = false;
  std::string error;
};

struct VoiceFinished {
  std::string name;
  VoiceId voice = 0;
};

// Fades one voice out
struct StopVoice {
  VoiceId voice = 0;
};

// Changes how a voice plays (volume and pan ramp to the new values)
struct SetVoice {
  VoiceId voice = 0;
  VoiceSettings settings;
};

// MIDI plays on one instrument: the built-in one until a SoundFont is
// loaded. LoadSoundFont makes MIDI play with a SoundFont's instruments
// (.sf2; .sf3 too on desktops). Answered with SoundFontLoaded; if it can't
// be loaded, the instrument stays as it was.
struct LoadSoundFont {
  std::string path;
};

struct SoundFontLoaded {
  std::string path;
  bool ok = false;
  std::string error;
};

struct UseBuiltInInstrument {};

// Plays a MIDI event on the instrument now: a note from a keyboard, say
struct PlayMidiEvent {
  MidiEvent event;
};

// Plays a MIDI file on the instrument, in the Music group. One plays at a
// time: starting one stops the last. Pitch is speed. Answered like
// PlaySound, with the path as the name.
struct PlayMidi {
  std::string path;
  VoiceSettings settings;
};

// Fades out all music (MIDI files included), or everything
struct StopMusic {};
struct StopAll {};

} // namespace ScratchPadd::Audio
