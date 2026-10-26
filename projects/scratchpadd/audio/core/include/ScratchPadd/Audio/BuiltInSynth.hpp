#pragma once

#include <ScratchPadd/Audio/Synth.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace ScratchPadd::Audio {

// The instrument that needs no files: plays any MIDI with simple synthesized
// sounds, so MIDI works before (or without) a SoundFont, on every platform.
//
// Each of General MIDI's 16 instrument families (pianos, organs, guitars,
// strings, ...) has its own tone, a few harmonics in a wavetable, and its
// own envelope (a piano dies away, an organ holds). Channel 10 plays drums:
// kick, snare, toms, hi-hats and cymbals, made from swept tones and noise.
// Channels follow volume, expression, pan, sustain, pitch bend (two
// semitones), all-notes-off and all-sound-off.
class BuiltInSynth final : public Synth {
public:
  BuiltInSynth(std::uint32_t sampleRate, std::uint32_t maxNotes = 32);

  void handle(const MidiEvent &event) noexcept override;
  void render(float *stereo, std::uint32_t frames) noexcept override;
  void allNotesOff() noexcept override;
  std::uint32_t notesSounding() const noexcept override;

private:
  static constexpr std::uint32_t kTableSize = 1024; // samples in one cycle
  static constexpr int kFamilies = 16;

  // An instrument family's envelope, as rates per sample
  struct Envelope {
    float attackStep;  // added each sample until full
    float decayFactor; // toward `sustain`, each sample
    float sustain;     // level held while the key is down (0: dies away)
    float releaseFactor;
  };
  // A drum sound: a tone sweeping down and noise, dying away together
  struct Drum {
    float startHz, endHz, sweepFactor; // the tone's pitch, and how fast it falls
    float tone, noise;                 // how much of each
    bool bright;                       // noise with its lows taken out (cymbals, hats)
    float decayFactor;
  };
  struct Channel {
    std::uint8_t program = 0;
    float volume = 100.0f / 127.0f; // controller 7 (MIDI's default 100)
    float expression = 1.0f;        // controller 11
    float pan = 0.0f;               // controller 10, as a balance from -1 to 1
    float bend = 1.0f;              // pitch bend, as a frequency ratio
    bool sustain = false;           // controller 64
  };
  enum class Stage : std::uint8_t { Off, Attack, Decay, Release };
  struct Note {
    Stage stage = Stage::Off;
    std::uint8_t channel = 0, key = 0;
    bool sustained = false; // let go while the sustain pedal was down
    bool drum = false;
    std::uint32_t started = 0; // when, for stealing the oldest
    float velocity = 0, level = 0;
    // Tone
    const float *table = nullptr;
    const Envelope *envelope = nullptr;
    float hz = 0;
    // Drum
    const Drum *sound = nullptr;
    float sweepHz = 0;   // the falling part of the drum's pitch
    float lastNoise = 0; // for the brightened noise
    double phase = 0;    // in table samples
  };

  Note *freeNote() noexcept;
  void startTone(Note &note, int channel, int key, int velocity) noexcept;
  void startDrum(Note &note, int key, int velocity) noexcept;
  void release(Note &note) noexcept;
  void controlChange(int channel, int controller, int value) noexcept;
  float noise() noexcept;
  void renderNote(Note &note, float *stereo, std::uint32_t frames) noexcept;

  float sampleRate_;
  float quickFade_;           // all-sound-off's release, per sample
  std::vector<float> tables_; // one cycle per family (plus a sine), each kTableSize + 1
  std::array<Envelope, kFamilies> envelopes_;
  std::vector<Drum> drums_;
  std::array<Channel, 16> channels_;
  std::vector<Note> notes_;
  std::uint32_t clock_ = 0;      // notes started, for stealing the oldest
  std::uint32_t noiseState_ = 1; // xorshift
};

} // namespace ScratchPadd::Audio
