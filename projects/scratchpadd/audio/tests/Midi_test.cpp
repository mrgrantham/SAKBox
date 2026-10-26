// The audio core's MIDI: events, MIDI files, the built-in instrument, the
// SoundFont instrument, and the engine playing them, heard through
// OfflineOutput.
#include <ScratchPadd/Audio/BuiltInSynth.hpp>
#include <ScratchPadd/Audio/MidiSequence.hpp>
#include <ScratchPadd/Audio/RealTime.hpp>
#include <ScratchPadd/Audio/SoundFontSynth.hpp>

#include "CountAllocations.hpp"
#include "MidiTestData.hpp"
#include "OfflineEngine.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace ScratchPadd::Audio;
using namespace OfflineEngine;

namespace {

constexpr int kOrgan = 16; // a General MIDI program that holds its notes

// The pitch of the left channel of `stereo` between two frames: the
// shortest period (50 Hz to 2 kHz) at which the sound nearly repeats
// itself, by autocorrelation (zero crossings would count harmonics too)
double frequency(const std::vector<float> &stereo, std::uint32_t from, std::uint32_t to) {
  const auto similarity = [&](std::uint32_t lag) {
    double sum = 0;
    for (std::uint32_t i = from; i + lag < to; ++i)
      sum += stereo[2 * i] * stereo[2 * (i + lag)];
    return sum / (to - from - lag);
  };
  const std::uint32_t shortest = kRate / 2000, longest = kRate / 50;
  double best = 0;
  for (std::uint32_t lag = shortest; lag <= longest; ++lag)
    best = std::max(best, similarity(lag));
  for (std::uint32_t lag = shortest; lag <= longest; ++lag) {
    if (similarity(lag) > 0.95 * best) {
      // Between this lag and its neighbours, where the peak really is
      const double before = similarity(lag - 1), at = similarity(lag), after = similarity(lag + 1);
      std::uint32_t top = lag;
      double peakValue = at, left = before, right = after;
      while (similarity(top + 1) > peakValue) {
        ++top;
        left = peakValue;
        peakValue = similarity(top);
        right = similarity(top + 1);
      }
      const double offset = 0.5 * (left - right) / (left - 2 * peakValue + right);
      return kRate / (top + offset);
    }
  }
  return 0;
}

// The loudest sample of one channel (0 left, 1 right) between two frames
float peak(const std::vector<float> &stereo, std::uint32_t from, std::uint32_t to, int side = 0) {
  float loudest = 0;
  for (std::uint32_t i = from; i < to; ++i)
    loudest = std::max(loudest, std::abs(stereo[2 * i + side]));
  return loudest;
}

// The next `frames` frames of `synth` on its own
std::vector<float> render(Synth &synth, std::uint32_t frames) {
  std::vector<float> out(2 * frames, 0.0f);
  synth.render(out.data(), frames);
  return out;
}

// A MIDI file read as a sequence at the engine's rate
std::shared_ptr<const MidiSequence> sequence(const std::vector<MidiTestData::TimedBytes> &events) {
  const MidiTestData::Bytes file = MidiTestData::midiFile(events);
  std::string error;
  auto read = readMidi(file.data(), file.size(), kRate, error);
  EXPECT_TRUE(read) << error;
  return read;
}

} // namespace

// ---------------------------------------------------------------------------
// MIDI events and files

TEST(MidiEventTest, MakesMidisBytes) {
  const MidiEvent on = MidiEvent::noteOn(2, 60, 100);
  EXPECT_EQ(on.status, 0x92);
  EXPECT_EQ(on.kind(), MidiEvent::NoteOn);
  EXPECT_EQ(on.channel(), 2);
  EXPECT_EQ(on.data1, 60);
  EXPECT_EQ(on.data2, 100);
  EXPECT_EQ(MidiEvent::programChange(9, 200).data1, 200 & 0x7F); // kept in range
  const auto bend = [](int value) {
    const MidiEvent event = MidiEvent::pitchBend(0, value);
    return event.data1 | (event.data2 << 7);
  };
  EXPECT_EQ(bend(0), 8192);
  EXPECT_EQ(bend(8191), 16383);
  EXPECT_EQ(bend(-8192), 0);
  EXPECT_EQ(bend(99999), 16383);
}

// The event in a port's bytes, failing the test if there's none
template <std::size_t N> MidiEvent eventIn(const unsigned char (&bytes)[N]) {
  const auto event = MidiEvent::fromBytes(bytes, N);
  if (!event) {
    ADD_FAILURE() << "no event";
    return {};
  }
  return *event;
}

// What a MIDI port delivers, as events: channel messages only
TEST(MidiEventTest, ReadsAPortsBytes) {
  const unsigned char note[] = {0x93, 64, 100}, program[] = {0xC2, 5}, clock[] = {0xF8},
                      sysex[] = {0xF0, 1, 2, 0xF7}, data[] = {0x40, 0x40}, cut[] = {0x90, 60};
  const MidiEvent on = eventIn(note);
  EXPECT_EQ(on.kind(), MidiEvent::NoteOn);
  EXPECT_EQ(on.channel(), 3);
  EXPECT_EQ(on.data1, 64);
  EXPECT_EQ(on.data2, 100);
  const MidiEvent change = eventIn(program);
  EXPECT_EQ(change.kind(), MidiEvent::ProgramChange);
  EXPECT_EQ(change.data1, 5);
  EXPECT_FALSE(MidiEvent::fromBytes(clock, sizeof clock));
  EXPECT_FALSE(MidiEvent::fromBytes(sysex, sizeof sysex));
  EXPECT_FALSE(MidiEvent::fromBytes(data, sizeof data)); // no status byte
  EXPECT_FALSE(MidiEvent::fromBytes(cut, sizeof cut));   // too short
  EXPECT_FALSE(MidiEvent::fromBytes(note, 0));
}

// A MIDI file's events, timed in frames: 960 ticks a second here
TEST(MidiSequenceTest, ReadsTimesInFrames) {
  const auto read = sequence({{0, 0x90, 60, 100}, {480, 0x80, 60, 0}, {960, 0xC1, 5, 0}});
  ASSERT_TRUE(read);
  ASSERT_EQ(read->events.size(), 3u);
  EXPECT_EQ(read->events[0].frame, 0u);
  EXPECT_EQ(read->events[1].frame, kRate / 2);
  EXPECT_EQ(read->events[2].frame, kRate);
  EXPECT_EQ(read->events[0].event.status, 0x90);
  EXPECT_EQ(read->events[0].event.data1, 60);
  EXPECT_EQ(read->events[0].event.data2, 100);
  EXPECT_EQ(read->events[2].event.status, 0xC1);
  EXPECT_EQ(read->events[2].event.data1, 5);
  EXPECT_EQ(read->frames, kRate + 1);
}

TEST(MidiSequenceTest, NotMidiIsAnError) {
  std::string error;
  const std::string text = "not a MIDI file at all";
  EXPECT_FALSE(readMidi(text.data(), text.size(), kRate, error));
  EXPECT_EQ(error, "not a MIDI file");
  EXPECT_FALSE(readMidiFile(testing::TempDir() + "missing.mid", kRate, error));
  EXPECT_NE(error.find("can't read"), std::string::npos) << error;
}

// ---------------------------------------------------------------------------
// The built-in instrument

TEST(BuiltInSynthTest, PlaysEachKeysPitch) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::noteOn(0, 69, 127)); // A4
  EXPECT_NEAR(frequency(render(synth, kRate / 4), 480, kRate / 4), 440.0, 8.0);
  synth.handle(MidiEvent::noteOff(0, 69));
  render(synth, kRate);
  synth.handle(MidiEvent::noteOn(0, 81, 127)); // an octave up
  EXPECT_NEAR(frequency(render(synth, kRate / 4), 480, kRate / 4), 880.0, 12.0);
}

// An organ holds its note until let go, then dies away; a piano dies away
// while held
TEST(BuiltInSynthTest, NotesHoldOrDieAway) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::noteOn(0, 60, 127));
  const std::vector<float> held = render(synth, kRate);
  EXPECT_NEAR(peak(held, kRate - 4800, kRate), peak(held, 4800, 9600), 0.01f);
  synth.handle(MidiEvent::noteOff(0, 60));
  EXPECT_LT(peak(render(synth, kRate), kRate - 4800, kRate), 1e-3f);
  EXPECT_EQ(synth.notesSounding(), 0u);

  synth.handle(MidiEvent::programChange(0, 0)); // piano
  synth.handle(MidiEvent::noteOn(0, 60, 127));
  const std::vector<float> piano = render(synth, 3 * kRate);
  EXPECT_LT(peak(piano, 2 * kRate, 3 * kRate), 0.2f * peak(piano, 0, 4800));
}

// With the sustain pedal down, let-go notes ring on until it's let up
TEST(BuiltInSynthTest, SustainPedal) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::controlChange(0, 64, 127));
  synth.handle(MidiEvent::noteOn(0, 60, 127));
  synth.handle(MidiEvent::noteOff(0, 60));
  EXPECT_GT(peak(render(synth, kRate / 2), kRate / 4, kRate / 2), 0.05f);
  synth.handle(MidiEvent::controlChange(0, 64, 0));
  render(synth, kRate);
  EXPECT_EQ(synth.notesSounding(), 0u);
}

// Pitch bend: up to two semitones either way
TEST(BuiltInSynthTest, PitchBend) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::pitchBend(0, 8191));
  synth.handle(MidiEvent::noteOn(0, 69, 127));
  EXPECT_NEAR(frequency(render(synth, kRate / 4), 480, kRate / 4), 440.0 * std::exp2(2.0 / 12),
              8.0);
}

// Volume (controller 7) scales by its square, as General MIDI suggests;
// pan (10) is a balance
TEST(BuiltInSynthTest, VolumeAndPan) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::noteOn(0, 60, 127));
  const float normal = peak(render(synth, kRate / 4), kRate / 8, kRate / 4);
  synth.handle(MidiEvent::controlChange(0, 7, 50)); // from 100
  render(synth, 64);
  const float quieter = peak(render(synth, kRate / 4), 0, kRate / 4);
  EXPECT_NEAR(quieter / normal, 0.25f, 0.02f);
  synth.handle(MidiEvent::controlChange(0, 10, 127)); // right
  const std::vector<float> right = render(synth, kRate / 4);
  EXPECT_EQ(peak(right, 0, kRate / 4, 0), 0.0f);
  EXPECT_NEAR(peak(right, 0, kRate / 4, 1), quieter, 0.01f);
}

// Channel 10 plays drums, which ring out by themselves
TEST(BuiltInSynthTest, DrumsOnChannelTen) {
  BuiltInSynth synth(kRate);
  for (int key : {36, 38, 42, 46, 49, 45, 60}) {
    synth.handle(MidiEvent::noteOn(9, key, 127));
    EXPECT_GT(peak(render(synth, 2400), 0, 2400), 0.01f) << "key " << key;
  }
  render(synth, 8 * kRate); // a crash cymbal rings for six seconds
  EXPECT_EQ(synth.notesSounding(), 0u);
}

// All sound off silences a channel at once, without a click: within 50 ms
TEST(BuiltInSynthTest, AllSoundOff) {
  BuiltInSynth synth(kRate);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  synth.handle(MidiEvent::noteOn(0, 60, 127));
  render(synth, 4800);
  synth.handle(MidiEvent::controlChange(0, 120, 0));
  const std::vector<float> fade = render(synth, 2400);
  EXPECT_GT(peak(fade, 0, 48), 0.01f); // not cut off
  EXPECT_EQ(synth.notesSounding(), 0u);
}

// More notes than it has room for: the oldest make way
TEST(BuiltInSynthTest, StealsWhenFull) {
  BuiltInSynth synth(kRate, 4);
  synth.handle(MidiEvent::programChange(0, kOrgan));
  for (int key = 60; key < 66; ++key)
    synth.handle(MidiEvent::noteOn(0, key, 100));
  EXPECT_EQ(synth.notesSounding(), 4u);
}

TEST(BuiltInSynthTest, PlaysWithoutAllocating) {
  BuiltInSynth synth(kRate, 8);
  std::vector<float> out(2 * 256);
  const std::uint32_t before = allocationsWhileMixing();
  {
    const MixingScope mixing;
    for (int key = 50; key < 70; ++key) {
      synth.handle(MidiEvent::noteOn(key % 2 ? 9 : 0, key, 100));
      synth.handle(MidiEvent::controlChange(0, 7, key));
      synth.handle(MidiEvent::pitchBend(0, key * 100));
      synth.render(out.data(), 256);
    }
    synth.allNotesOff();
  }
  EXPECT_EQ(allocationsWhileMixing(), before);
}

// ---------------------------------------------------------------------------
// The SoundFont instrument (with a SoundFont made in code: a sine at 440 Hz
// on key 69)

TEST(SoundFontSynthTest, PlaysTheSoundFont) {
  const MidiTestData::Bytes font = MidiTestData::soundFont();
  std::string error;
  auto synth = SoundFontSynth::load(font.data(), font.size(), kRate, 16, error);
  ASSERT_TRUE(synth) << error;
  EXPECT_EQ(synth->presetCount(), 1);
  synth->handle(MidiEvent::noteOn(0, 69, 127));
  EXPECT_NEAR(frequency(render(*synth, kRate / 4), 480, kRate / 4), 440.0, 8.0);
  synth->handle(MidiEvent::noteOn(0, 81, 127));
  EXPECT_EQ(synth->notesSounding(), 2u);
  synth->handle(MidiEvent::noteOn(0, 69, 0)); // velocity 0: off
  synth->handle(MidiEvent::noteOff(0, 81));
  EXPECT_LT(peak(render(*synth, kRate / 2), kRate / 4, kRate / 2), 1e-3f);
  EXPECT_EQ(synth->notesSounding(), 0u);
}

TEST(SoundFontSynthTest, NotASoundFontIsAnError) {
  std::string error;
  const std::string text = "not a SoundFont";
  EXPECT_FALSE(SoundFontSynth::load(text.data(), text.size(), kRate, 16, error));
  EXPECT_EQ(error, "can't read those bytes as a SoundFont");
  EXPECT_FALSE(SoundFontSynth::load(testing::TempDir() + "missing.sf2", kRate, 16, error));
  EXPECT_NE(error.find("can't read"), std::string::npos) << error;
}

// Everything TinySoundFont needs is allocated when loaded: playing (more
// notes than it has room for, every channel, controllers) allocates nothing
TEST(SoundFontSynthTest, PlaysWithoutAllocating) {
  const MidiTestData::Bytes font = MidiTestData::soundFont();
  std::string error;
  auto synth = SoundFontSynth::load(font.data(), font.size(), kRate, 8, error);
  ASSERT_TRUE(synth) << error;
  std::vector<float> out(2 * 256);
  const std::uint32_t before = allocationsWhileMixing();
  {
    const MixingScope mixing;
    for (int key = 40; key < 80; ++key) {
      synth->handle(MidiEvent::programChange(key % 16, 0));
      synth->handle(MidiEvent::noteOn(key % 16, key, 100));
      synth->handle(MidiEvent::controlChange(key % 16, 7, key));
      synth->handle(MidiEvent::pitchBend(key % 16, key * 100));
      synth->render(out.data(), 256);
    }
    synth->allNotesOff();
    synth->render(out.data(), 256);
  }
  EXPECT_EQ(allocationsWhileMixing(), before);
}

// ---------------------------------------------------------------------------
// The engine playing MIDI

// Events played now go to the instrument, in the Music group
TEST(EngineMidiTest, PlaysEventsNow) {
  Offline offline;
  offline.engine.midi(MidiEvent::programChange(0, kOrgan));
  offline.engine.midi(MidiEvent::noteOn(0, 69, 127));
  const std::vector<float> out = offline.render(kRate / 4);
  EXPECT_NEAR(frequency(out, 480, kRate / 4), 440.0, 8.0);
  EXPECT_EQ(offline.engine.stats().notesSounding.load(), 1u);
  offline.engine.setGroupVolume(Group::Music, 0.0f);
  offline.render(1024);
  EXPECT_EQ(peak(offline.render(1024), 0, 1024), 0.0f);
}

// A sequence's notes start on time (within the 32-frame steps events are
// played in), and its voice finishes after its last event
TEST(EngineMidiTest, PlaysASequenceOnTime) {
  Offline offline;
  std::string error;
  // Tick 96 is 0.1 s: frame 4800
  const VoiceId voice = started(
      offline.engine.playMidi(
          sequence({{0, 0xC0, kOrgan, 0}, {96, 0x90, 69, 127}, {192, 0x80, 69, 0}}), {}, error),
      error);
  const std::vector<float> out = offline.render(kRate / 2);
  std::uint32_t first = 0;
  while (first < kRate / 2 && out[2 * first] == 0.0f)
    ++first;
  EXPECT_GE(first, 4800u - 32);
  EXPECT_LE(first, 4801u); // (the organ's wave starts at zero)
  EXPECT_EQ(offline.engine.update(), std::vector<VoiceId>{voice});
}

// A looping sequence goes round until stopped; stopping lets its notes go
TEST(EngineMidiTest, LoopsUntilStopped) {
  Offline offline;
  std::string error;
  // A note held for the first 0.1 s of each 0.2 s
  const auto notes =
      sequence({{0, 0xC0, kOrgan, 0}, {0, 0x90, 69, 127}, {96, 0x80, 69, 0}, {192, 0xB0, 7, 100}});
  const VoiceId voice = started(offline.engine.playMidi(notes, {.loop = true}, error), error);
  const std::vector<float> out = offline.render(kRate);
  // 0.85 s is 0.05 s into the fifth round: the note is held
  EXPECT_GT(peak(out, 40800, 43200), 0.05f);
  EXPECT_TRUE(offline.engine.update().empty());
  offline.engine.stop(voice);
  offline.render(1024);
  EXPECT_EQ(offline.engine.update(), std::vector<VoiceId>{voice});
  offline.render(kRate);
  EXPECT_EQ(offline.engine.stats().notesSounding.load(), 0u);
}

// One sequence plays at a time: starting another stops the last
TEST(EngineMidiTest, OneSequenceAtATime) {
  Offline offline;
  std::string error;
  const auto held = sequence({{0, 0xC0, kOrgan, 0}, {0, 0x90, 60, 127}, {9600, 0x80, 60, 0}});
  const VoiceId first = started(offline.engine.playMidi(held, {}, error), error);
  offline.render(1024);
  const VoiceId second = started(offline.engine.playMidi(held, {}, error), error);
  offline.render(1024);
  EXPECT_EQ(offline.engine.update(), std::vector<VoiceId>{first});
  EXPECT_TRUE(offline.engine.isPlaying(second));
}

TEST(EngineMidiTest, MissingMidiFileIsAnError) {
  Offline offline;
  std::string error;
  EXPECT_FALSE(offline.engine.playMidi(testing::TempDir() + "missing.mid", {}, error));
  EXPECT_NE(error.find("can't read"), std::string::npos) << error;
}

// A SoundFont replaces the built-in instrument; one that can't be read
// leaves it playing
TEST(EngineMidiTest, ChoosesTheInstrument) {
  Offline offline;
  const std::string text = "not a SoundFont";
  const LoadResult bad = offline.engine.loadSoundFont(text.data(), text.size());
  EXPECT_FALSE(bad.ok);
  EXPECT_EQ(bad.error, "can't read those bytes as a SoundFont");
  offline.engine.midi(MidiEvent::noteOn(0, 69, 127));
  EXPECT_GT(peak(offline.render(4800), 0, 4800), 0.01f);

  const MidiTestData::Bytes font = MidiTestData::soundFont();
  const LoadResult good = offline.engine.loadSoundFont(font.data(), font.size());
  ASSERT_TRUE(good.ok) << good.error;
  offline.engine.midi(MidiEvent::noteOn(0, 69, 127));
  const std::vector<float> out = offline.render(kRate / 4);
  EXPECT_NEAR(frequency(out, 480, kRate / 4), 440.0, 8.0);
  EXPECT_EQ(offline.engine.stats().notesSounding.load(), 1u); // the new instrument's
  offline.engine.useBuiltInInstrument();
  offline.render(64);
  EXPECT_EQ(offline.engine.stats().notesSounding.load(), 0u);
  offline.engine.update(); // frees the instruments the audio thread let go of
}

// Mixing MIDI allocates nothing: sequences, events now, and either
// instrument
TEST(EngineMidiTest, MixingAllocatesNothing) {
  Offline offline;
  std::string error;
  const auto notes = sequence({{0, 0x90, 60, 100},
                               {10, 0x99, 36, 100},
                               {100, 0x80, 60, 0},
                               {200, 0xE0, 0, 80},
                               {300, 0xB0, 7, 90}});
  started(offline.engine.playMidi(notes, {.loop = true}, error), error);
  const MidiTestData::Bytes font = MidiTestData::soundFont();
  std::vector<float> out(2 * 512);
  const std::uint32_t before = allocationsWhileMixing();
  for (int block = 0; block < 40; ++block) {
    offline.engine.midi(MidiEvent::noteOn(1, 40 + block, 90));
    if (block == 20) {
      ASSERT_TRUE(offline.engine.loadSoundFont(font.data(), font.size()).ok);
    }
    offline.output.render(out.data(), 512);
  }
  EXPECT_EQ(allocationsWhileMixing(), before);
}

// A second of 32 built-in notes (the most the engine gives it by default,
// less a few) mixes well within a second. The figure is printed, to
// compare machines.
TEST(SpeedTest, ASecondOfABusyInstrument) {
  Offline offline;
  for (int note = 0; note < 32; ++note) {
    offline.engine.midi(MidiEvent::programChange(note % 8, note * 4));
    offline.engine.midi(MidiEvent::noteOn(note % 8, 40 + note, 100));
  }
  std::vector<float> out(2 * 512);
  const auto start = std::chrono::steady_clock::now();
  for (std::uint32_t frame = 0; frame < kRate; frame += 512)
    offline.output.render(out.data(), 512);
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("32 built-in notes, 1 s at %u Hz: mixed in %.2f ms\n", kRate, seconds * 1000);
  EXPECT_LT(seconds, 0.5);
}
