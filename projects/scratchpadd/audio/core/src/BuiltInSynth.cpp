#include <ScratchPadd/Audio/BuiltInSynth.hpp>

#include <algorithm>
#include <cmath>

namespace ScratchPadd::Audio {

namespace {

constexpr int kDrumChannel = 9;      // General MIDI's channel 10
constexpr float kOutputGain = 0.25f; // room for a few notes at full velocity
constexpr float kSilent = 1e-4f;     // a note this quiet is over
constexpr float kQuickFade = 0.005f; // seconds: all-sound-off, without a click
constexpr double kTwoPi = 6.283185307179586;

// A General MIDI instrument family: the strengths of its first six
// harmonics, and its envelope in seconds (attack; decay and release as time
// constants; the level held while the key is down)
struct Family {
  float harmonics[6];
  float attack, decay, sustain, release;
};
constexpr Family kFamilyTones[16] = {
    {{1, 0.5f, 0.3f, 0.15f, 0.08f, 0.04f}, 0.002f, 1.2f, 0.0f, 0.25f}, // piano
    {{1, 0, 0.35f, 0, 0.12f, 0}, 0.001f, 0.9f, 0.0f, 0.4f},            // chromatic percussion
    {{1, 0.7f, 0.5f, 0.35f, 0, 0.25f}, 0.005f, 0.1f, 1.0f, 0.05f},     // organ
    {{1, 0.6f, 0.35f, 0.2f, 0.1f, 0.05f}, 0.002f, 0.6f, 0.0f, 0.15f},  // guitar
    {{1, 0.5f, 0.15f, 0.05f, 0, 0}, 0.004f, 0.5f, 0.5f, 0.08f},        // bass
    {{1, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f}, 0.08f, 0.3f, 0.9f, 0.3f},   // strings
    {{1, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f}, 0.2f, 0.4f, 0.9f, 0.5f},    // ensemble
    {{1, 0.8f, 0.6f, 0.45f, 0.3f, 0.2f}, 0.04f, 0.2f, 0.8f, 0.12f},    // brass
    {{1, 0.1f, 0.6f, 0.05f, 0.35f, 0.03f}, 0.03f, 0.2f, 0.85f, 0.1f},  // reed
    {{1, 0.15f, 0.05f, 0, 0, 0}, 0.05f, 0.2f, 0.9f, 0.12f},            // pipe
    {{1, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f}, 0.005f, 0.2f, 0.8f, 0.1f},  // synth lead
    {{1, 0.3f, 0.2f, 0.1f, 0.05f, 0}, 0.3f, 0.5f, 0.9f, 0.8f},         // synth pad
    {{1, 0, 0.4f, 0, 0.3f, 0}, 0.1f, 1.0f, 0.6f, 1.0f},                // synth effects
    {{1, 0.4f, 0.3f, 0.2f, 0.1f, 0.05f}, 0.002f, 0.7f, 0.0f, 0.2f},    // ethnic
    {{1, 0, 0.3f, 0, 0.1f, 0}, 0.001f, 0.25f, 0.0f, 0.1f},             // percussive
    {{1, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f}, 0.01f, 0.3f, 0.3f, 0.3f},      // sound effects
};

// The drum sounds, and which General MIDI drum keys play each
enum DrumSound { Kick, Snare, Tom, ClosedHat, OpenHat, Crash, Ride, Click, kDrumSounds };

DrumSound drumFor(int key) {
  switch (key) {
  case 35:
  case 36:
    return Kick;
  case 38:
  case 39:
  case 40:
    return Snare;
  case 41:
  case 43:
  case 45:
  case 47:
  case 48:
  case 50:
    return Tom;
  case 42:
  case 44:
    return ClosedHat;
  case 46:
    return OpenHat;
  case 49:
  case 52:
  case 55:
  case 57:
    return Crash;
  case 51:
  case 53:
  case 59:
    return Ride;
  default:
    return Click;
  }
}

// A factor that shrinks a value by e every `seconds`
float timeConstant(float seconds, float sampleRate) {
  return std::exp(-1.0f / (seconds * sampleRate));
}

} // namespace

BuiltInSynth::BuiltInSynth(std::uint32_t sampleRate, std::uint32_t maxNotes)
    : sampleRate_(static_cast<float>(sampleRate)),
      quickFade_(timeConstant(kQuickFade, sampleRate_)), notes_(maxNotes) {
  // One cycle of each family's tone, then a sine for the drums, each with
  // its first sample repeated at the end so reading between samples never
  // wraps
  tables_.assign((kFamilies + 1) * (kTableSize + 1), 0.0f);
  for (int table = 0; table <= kFamilies; ++table) {
    float *cycle = &tables_[table * (kTableSize + 1)];
    float peak = 0;
    for (std::uint32_t i = 0; i < kTableSize; ++i) {
      const double angle = kTwoPi * i / kTableSize;
      double value = 0;
      for (int harmonic = 0; harmonic < 6; ++harmonic) {
        const float strength = table < kFamilies ? kFamilyTones[table].harmonics[harmonic]
                                                 : (harmonic == 0 ? 1.0f : 0.0f);
        value += strength * std::sin(angle * (harmonic + 1));
      }
      cycle[i] = static_cast<float>(value);
      peak = std::max(peak, std::abs(cycle[i]));
    }
    for (std::uint32_t i = 0; i < kTableSize; ++i) {
      cycle[i] /= peak;
    }
    cycle[kTableSize] = cycle[0];
  }
  for (int family = 0; family < kFamilies; ++family) {
    const Family &f = kFamilyTones[family];
    envelopes_[family] = {.attackStep = 1.0f / (f.attack * sampleRate_),
                          .decayFactor = timeConstant(f.decay, sampleRate_),
                          .sustain = f.sustain,
                          .releaseFactor = timeConstant(f.release, sampleRate_)};
  }
  const auto drum = [this](float startHz, float endHz, float sweep, float tone, float noise,
                           bool bright, float decay) {
    return Drum{.startHz = startHz,
                .endHz = endHz,
                .sweepFactor = timeConstant(sweep, sampleRate_),
                .tone = tone,
                .noise = noise,
                .bright = bright,
                .decayFactor = timeConstant(decay, sampleRate_)};
  };
  drums_.resize(kDrumSounds);
  drums_[Kick] = drum(160, 45, 0.04f, 1.0f, 0.05f, false, 0.18f);
  drums_[Snare] = drum(220, 180, 0.02f, 0.4f, 0.7f, true, 0.07f);
  drums_[Tom] = drum(160, 100, 0.05f, 0.9f, 0.1f, false, 0.2f); // scaled by key
  drums_[ClosedHat] = drum(0, 0, 1.0f, 0.0f, 0.5f, true, 0.025f);
  drums_[OpenHat] = drum(0, 0, 1.0f, 0.0f, 0.5f, true, 0.25f);
  drums_[Crash] = drum(0, 0, 1.0f, 0.0f, 0.45f, true, 0.7f);
  drums_[Ride] = drum(3000, 3000, 1.0f, 0.1f, 0.3f, true, 0.45f);
  drums_[Click] = drum(0, 0, 1.0f, 0.0f, 0.6f, false, 0.03f);
}

void BuiltInSynth::handle(const MidiEvent &event) noexcept {
  const int channel = event.channel();
  Channel &c = channels_[channel];
  switch (event.kind()) {
  case MidiEvent::NoteOn:
    if (event.data2 > 0) {
      Note &note = *freeNote();
      if (channel == kDrumChannel) {
        startDrum(note, event.data1, event.data2);
      } else {
        startTone(note, channel, event.data1, event.data2);
      }
      return;
    }
    [[fallthrough]]; // velocity 0 is a note off
  case MidiEvent::NoteOff:
    for (Note &note : notes_) {
      if (note.stage != Stage::Off && note.stage != Stage::Release && !note.drum &&
          note.channel == channel && note.key == event.data1) {
        if (c.sustain) {
          note.sustained = true;
        } else {
          release(note);
        }
      }
    }
    return;
  case MidiEvent::ControlChange:
    controlChange(channel, event.data1, event.data2);
    return;
  case MidiEvent::ProgramChange:
    c.program = event.data1;
    return;
  case MidiEvent::PitchBend: {
    const float semitones = static_cast<float>((event.data1 | (event.data2 << 7)) - 8192) / 4096.0f;
    c.bend = std::exp2(semitones / 12.0f);
    return;
  }
  default: // pressure isn't used
    return;
  }
}

void BuiltInSynth::controlChange(int channel, int controller, int value) noexcept {
  Channel &c = channels_[channel];
  const float fraction = static_cast<float>(value) / 127.0f;
  switch (controller) {
  case 7:
    c.volume = fraction;
    return;
  case 10:
    c.pan = std::clamp((static_cast<float>(value) - 64.0f) / 63.0f, -1.0f, 1.0f);
    return;
  case 11:
    c.expression = fraction;
    return;
  case 64: // sustain pedal
    c.sustain = value >= 64;
    if (!c.sustain) {
      for (Note &note : notes_) {
        if (note.channel == channel && note.sustained) {
          release(note);
        }
      }
    }
    return;
  case 120: // all sound off: quickly, but without a click
    for (Note &note : notes_) {
      if (note.stage != Stage::Off && note.channel == channel) {
        note.stage = Stage::Release;
        note.envelope = nullptr; // (see renderNote)
      }
    }
    return;
  case 121: // reset controllers
    c.expression = 1.0f;
    c.bend = 1.0f;
    controlChange(channel, 64, 0);
    return;
  case 123: // all notes off
    for (Note &note : notes_) {
      if (note.channel == channel && !note.drum && note.stage != Stage::Off) {
        release(note);
      }
    }
    return;
  default:
    return;
  }
}

void BuiltInSynth::allNotesOff() noexcept {
  for (int channel = 0; channel < 16; ++channel) {
    controlChange(channel, 123, 0);
  }
}

std::uint32_t BuiltInSynth::notesSounding() const noexcept {
  return static_cast<std::uint32_t>(std::count_if(
      notes_.begin(), notes_.end(), [](const Note &note) { return note.stage != Stage::Off; }));
}

BuiltInSynth::Note *BuiltInSynth::freeNote() noexcept {
  // A silent note, or else the quietest one dying away, or else the oldest
  Note *quietest = nullptr, *oldest = &notes_.front();
  for (Note &note : notes_) {
    if (note.stage == Stage::Off) {
      return &note;
    }
    if (note.stage == Stage::Release && (!quietest || note.level < quietest->level)) {
      quietest = &note;
    }
    if (note.started < oldest->started) {
      oldest = &note;
    }
  }
  return quietest ? quietest : oldest;
}

void BuiltInSynth::startTone(Note &note, int channel, int key, int velocity) noexcept {
  const int family = channels_[channel].program / 8;
  const float v = static_cast<float>(velocity) / 127.0f;
  note = Note{.stage = Stage::Attack,
              .channel = static_cast<std::uint8_t>(channel),
              .key = static_cast<std::uint8_t>(key),
              .started = clock_++,
              .velocity = v * v,
              .table = &tables_[family * (kTableSize + 1)],
              .envelope = &envelopes_[family],
              .hz = 440.0f * std::exp2(static_cast<float>(key - 69) / 12.0f)};
}

void BuiltInSynth::startDrum(Note &note, int key, int velocity) noexcept {
  const Drum &drum = drums_[drumFor(key)];
  // Toms are tuned by key: low, mid and high
  const float tuning = drumFor(key) == Tom ? std::exp2(static_cast<float>(key - 45) / 12.0f) : 1.0f;
  const float v = static_cast<float>(velocity) / 127.0f;
  note = Note{.stage = Stage::Decay,
              .channel = kDrumChannel,
              .key = static_cast<std::uint8_t>(key),
              .drum = true,
              .started = clock_++,
              .velocity = v * v,
              .level = 1.0f,
              .table = &tables_[kFamilies * (kTableSize + 1)], // the sine
              .hz = drum.endHz * tuning,
              .sound = &drum,
              .sweepHz = (drum.startHz - drum.endHz) * tuning};
}

void BuiltInSynth::release(Note &note) noexcept {
  note.sustained = false;
  if (!note.drum) { // drums ring out
    note.stage = Stage::Release;
  }
}

float BuiltInSynth::noise() noexcept {
  noiseState_ ^= noiseState_ << 13;
  noiseState_ ^= noiseState_ >> 17;
  noiseState_ ^= noiseState_ << 5;
  return static_cast<float>(noiseState_) / 2147483648.0f - 1.0f;
}

void BuiltInSynth::render(float *stereo, std::uint32_t frames) noexcept {
  for (Note &note : notes_) {
    if (note.stage != Stage::Off) {
      renderNote(note, stereo, frames);
    }
  }
}

void BuiltInSynth::renderNote(Note &note, float *stereo, std::uint32_t frames) noexcept {
  const Channel &c = channels_[note.channel];
  const float gain =
      kOutputGain * note.velocity * c.volume * c.volume * c.expression * c.expression;
  const float left = gain * (c.pan > 0.0f ? 1.0f - c.pan : 1.0f);
  const float right = gain * (c.pan < 0.0f ? 1.0f + c.pan : 1.0f);
  const float toTable = static_cast<float>(kTableSize) / sampleRate_;
  const float *table = note.table;
  float level = note.level;
  double phase = note.phase;
  for (std::uint32_t i = 0; i < frames; ++i) {
    float sample;
    if (note.drum) {
      const float hz = note.hz + note.sweepHz;
      note.sweepHz *= note.sound->sweepFactor;
      const auto index = static_cast<std::uint32_t>(phase);
      const float fraction = static_cast<float>(phase - index);
      const float tone = table[index] + (table[index + 1] - table[index]) * fraction;
      float hiss = noise();
      if (note.sound->bright) { // the change since the last sample: the highs
        const float change = 0.5f * (hiss - note.lastNoise);
        note.lastNoise = hiss;
        hiss = change;
      }
      sample = note.sound->tone * tone + note.sound->noise * hiss;
      phase += hz * toTable;
      level *= note.stage == Stage::Release ? quickFade_ : note.sound->decayFactor;
    } else {
      const Envelope *envelope = note.envelope;
      switch (note.stage) {
      case Stage::Attack:
        level += envelope->attackStep;
        if (level >= 1.0f) {
          level = 1.0f;
          note.stage = Stage::Decay;
        }
        break;
      case Stage::Decay:
        level = envelope->sustain + (level - envelope->sustain) * envelope->decayFactor;
        break;
      default: // all-sound-off clears the envelope, for a quick fade
        level *= envelope ? envelope->releaseFactor : quickFade_;
        break;
      }
      const auto index = static_cast<std::uint32_t>(phase);
      const float fraction = static_cast<float>(phase - index);
      sample = table[index] + (table[index + 1] - table[index]) * fraction;
      phase += note.hz * c.bend * toTable;
    }
    if (phase >= kTableSize) {
      phase -= kTableSize;
    }
    stereo[2 * i] += sample * level * left;
    stereo[2 * i + 1] += sample * level * right;
  }
  note.level = level;
  note.phase = phase;
  // Over once it's silent and can't come back (released, or dying away)
  const bool fading = note.stage == Stage::Release || note.drum ||
                      (note.stage == Stage::Decay && note.envelope->sustain == 0.0f);
  if (fading && level < kSilent) {
    note.stage = Stage::Off;
  }
}

} // namespace ScratchPadd::Audio
