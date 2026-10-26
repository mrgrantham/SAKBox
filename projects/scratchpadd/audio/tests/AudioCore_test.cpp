// The audio core, heard through OfflineOutput: every test mixes on its own
// thread and checks the samples exactly, the same on every machine.
#include <ScratchPadd/Audio/Engine.hpp>
#include <ScratchPadd/Audio/RealTime.hpp>
#include <ScratchPadd/Audio/SpscQueue.hpp>

#include "CountAllocations.hpp"
#include "OfflineEngine.hpp"

#include <gtest/gtest.h>
#include <miniaudio/miniaudio.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace ScratchPadd::Audio;
using namespace OfflineEngine;

namespace {

// `frames` frames: left counts up from 0.01, right is minus that
std::vector<float> ramp(std::uint32_t frames) {
  std::vector<float> stereo;
  for (std::uint32_t i = 0; i < frames; ++i) {
    stereo.push_back(0.01f * static_cast<float>(i + 1));
    stereo.push_back(-0.01f * static_cast<float>(i + 1));
  }
  return stereo;
}

std::vector<float> constant(std::uint32_t frames, float value = 0.5f) {
  return std::vector<float>(2 * frames, value);
}

// Writes `frames` frames from `sample(frame, channel)` to a WAV file
template <typename Sample>
std::string writeWav(const std::string &name, std::uint32_t rate, std::uint32_t channels,
                     std::uint32_t frames, Sample sample) {
  const std::string path = testing::TempDir() + name;
  ma_encoder_config config =
      ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, channels, rate);
  ma_encoder encoder;
  EXPECT_EQ(ma_encoder_init_file(path.c_str(), &config, &encoder), MA_SUCCESS);
  std::vector<float> data;
  for (std::uint32_t i = 0; i < frames; ++i)
    for (std::uint32_t c = 0; c < channels; ++c)
      data.push_back(sample(i, c));
  ma_uint64 written = 0;
  ma_encoder_write_pcm_frames(&encoder, data.data(), frames, &written);
  ma_encoder_uninit(&encoder);
  return path;
}

} // namespace

// ---------------------------------------------------------------------------
// The queues between the threads

TEST(SpscQueueTest, KeepsOrderAndItsLimit) {
  SpscQueue<int> queue(3);
  int value = 0;
  EXPECT_FALSE(queue.pop(value));
  for (int i = 1; i <= 3; ++i)
    EXPECT_TRUE(queue.push(i));
  EXPECT_FALSE(queue.push(4));               // full
  for (int round = 0; round < 10; ++round) { // wraps around many times
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, round + 1);
    EXPECT_TRUE(queue.push(round + 4));
  }
}

// One thread pushing, another popping, at once: everything arrives once,
// in order. (Run under ThreadSanitizer too.)
TEST(SpscQueueTest, TwoThreadsKeepOrder) {
  SpscQueue<std::uint32_t> queue(64);
  constexpr std::uint32_t kItems = 200000;
  std::thread producer([&] {
    for (std::uint32_t i = 0; i < kItems;)
      if (queue.push(i))
        ++i;
  });
  std::uint32_t expected = 0, value = 0;
  while (expected < kItems)
    if (queue.pop(value)) {
      ASSERT_EQ(value, expected++);
    }
  producer.join();
}

TEST(SampleRingTest, CopiesBlocksAcrossTheWrap) {
  SampleRing ring(10);
  float in[8] = {1, 2, 3, 4, 5, 6, 7, 8}, out[8] = {};
  EXPECT_EQ(ring.write(in, 8), 8u);
  EXPECT_EQ(ring.read(out, 6), 6u);
  EXPECT_EQ(ring.write(in, 8), 8u); // wraps
  EXPECT_EQ(ring.write(in, 8), 0u); // full (10)
  EXPECT_EQ(ring.available(), 10u);
  EXPECT_EQ(ring.read(out, 8), 8u);
  EXPECT_EQ(std::vector<float>(out, out + 8), (std::vector<float>{7, 8, 1, 2, 3, 4, 5, 6}));
}

// ---------------------------------------------------------------------------
// Mixing

// A sound plays sample for sample as loaded, then silence
TEST(MixerTest, PlaysASoundAsLoaded) {
  Offline offline;
  offline.load("ramp", ramp(100));
  ASSERT_TRUE(offline.engine.play("ramp"));
  const std::vector<float> out = offline.render(128);
  const std::vector<float> expected = ramp(100);
  for (std::size_t i = 0; i < expected.size(); ++i)
    ASSERT_FLOAT_EQ(out[i], expected[i]) << "sample " << i;
  for (std::size_t i = expected.size(); i < out.size(); ++i)
    ASSERT_EQ(out[i], 0.0f) << "sample " << i;
}

// Two voices add up
TEST(MixerTest, VoicesAddUp) {
  Offline offline;
  offline.load("half", constant(64, 0.25f));
  offline.engine.play("half");
  offline.engine.play("half");
  EXPECT_FLOAT_EQ(offline.render(32)[10], 0.5f);
}

// A voice's volume times its group's times the master volume
TEST(MixerTest, VolumesMultiply) {
  Offline offline;
  offline.load("one", constant(64, 1.0f));
  offline.engine.setMasterVolume(0.5f);
  offline.engine.setGroupVolume(Group::Sounds, 0.5f);
  offline.engine.play("one", {.volume = 2.0f});
  EXPECT_FLOAT_EQ(offline.render(32)[0], 0.5f);
}

// Balance: the centre leaves both sides as loaded; to one side, the other
// side is turned down, to silence at the end
TEST(MixerTest, PanIsBalance) {
  Offline offline;
  offline.load("one", constant(64, 1.0f));
  offline.engine.play("one", {.pan = 1.0f});
  auto out = offline.render(16);
  EXPECT_FLOAT_EQ(out[0], 0.0f);
  EXPECT_FLOAT_EQ(out[1], 1.0f);
  offline.engine.update();
  Offline other;
  other.load("one", constant(64, 1.0f));
  other.engine.play("one", {.pan = -0.25f});
  out = other.render(16);
  EXPECT_FLOAT_EQ(out[0], 1.0f);
  EXPECT_FLOAT_EQ(out[1], 0.75f);
}

// Pitch is speed: twice as fast is half as long; half as fast fills the
// gaps between samples by straight lines
TEST(MixerTest, PitchIsSpeed) {
  Offline offline;
  offline.load("ramp", ramp(100));
  offline.engine.play("ramp", {.pitch = 2.0f});
  std::vector<float> out = offline.render(64);
  EXPECT_FLOAT_EQ(out[0], 0.01f);
  EXPECT_FLOAT_EQ(out[2], 0.03f); // every other frame
  EXPECT_EQ(out[2 * 50], 0.0f);   // over after 50 frames
  Offline slow;
  slow.load("ramp", ramp(100));
  slow.engine.play("ramp", {.pitch = 0.5f});
  out = slow.render(4);
  EXPECT_FLOAT_EQ(out[0], 0.01f);
  EXPECT_FLOAT_EQ(out[2], 0.015f); // halfway between the first two
  EXPECT_FLOAT_EQ(out[4], 0.02f);
}

// A looping sound goes round until stopped; stopping fades it out over one
// block (no click), and then the voice is finished
TEST(MixerTest, LoopsUntilStoppedThenFades) {
  Offline offline;
  offline.load("ramp", ramp(10));
  const VoiceId voice = started(offline.engine.play("ramp", {.loop = true}));
  std::vector<float> out = offline.render(25);
  EXPECT_FLOAT_EQ(out[2 * 10], 0.01f); // round again
  EXPECT_FLOAT_EQ(out[2 * 24], 0.05f);
  offline.engine.stop(voice);
  out = offline.render(100);                      // the fade
  EXPECT_NEAR(out[0], 0.06f, 1e-6);               // starts at full volume...
  EXPECT_LT(std::abs(out[2 * 99]), 0.01f * 0.1f); // ...ends near silence
  for (int i = 1; i < 100; ++i)
    EXPECT_LE(std::abs(out[2 * i]) / (0.01f * (1 + (i + 5) % 10)), // its gain falls steadily
              std::abs(out[2 * (i - 1)]) / (0.01f * (1 + (i + 4) % 10)) + 1e-5f);
  EXPECT_EQ(offline.engine.update(), std::vector<VoiceId>{voice});
  out = offline.render(10);
  EXPECT_EQ(out[0], 0.0f);
}

// When a sound ends, update() reports its voice, once
TEST(MixerTest, FinishedVoicesAreReported) {
  Offline offline;
  offline.load("short", constant(10));
  const VoiceId a = started(offline.engine.play("short"));
  const VoiceId b = started(offline.engine.play("short"));
  EXPECT_TRUE(offline.engine.update().empty());
  offline.render(64);
  EXPECT_EQ(offline.engine.update(), (std::vector<VoiceId>{a, b}));
  EXPECT_FALSE(offline.engine.isPlaying(a));
  EXPECT_TRUE(offline.engine.update().empty());
}

// Unloading a sound that's playing stops it at once; it's freed only after
// the audio thread has let go of it
TEST(MixerTest, UnloadWhilePlaying) {
  Offline offline;
  offline.load("long", constant(10000));
  const VoiceId voice = started(offline.engine.play("long", {.loop = true}));
  offline.render(64);
  EXPECT_TRUE(offline.engine.unload("long"));
  EXPECT_FALSE(offline.engine.unload("long"));
  EXPECT_FALSE(offline.engine.play("long"));
  const std::vector<float> out = offline.render(64);
  EXPECT_EQ(out[0], 0.0f);
  EXPECT_EQ(offline.engine.update(), std::vector<VoiceId>{voice});
}

// No more voices than the limit; a finished one frees its place
TEST(MixerTest, VoiceLimit) {
  Offline offline({.maxVoices = 2});
  offline.load("short", constant(10));
  EXPECT_TRUE(offline.engine.play("short"));
  EXPECT_TRUE(offline.engine.play("short"));
  EXPECT_FALSE(offline.engine.play("short"));
  EXPECT_FALSE(offline.engine.play("missing"));
  offline.render(32);
  offline.engine.update();
  EXPECT_TRUE(offline.engine.play("short"));
}

// Volume changes ramp across a block rather than jump
TEST(MixerTest, VolumeChangesRamp) {
  Offline offline;
  offline.load("one", constant(1000, 1.0f));
  const VoiceId voice = started(offline.engine.play("one"));
  offline.render(10);
  offline.engine.set(voice, {.volume = 0.0f});
  const std::vector<float> out = offline.render(100);
  EXPECT_FLOAT_EQ(out[0], 1.0f);
  EXPECT_NEAR(out[2 * 50], 0.5f, 1e-5);
  EXPECT_NEAR(out[2 * 99], 0.01f, 1e-5);
  EXPECT_EQ(offline.render(10)[0], 0.0f);
}

// ---------------------------------------------------------------------------
// Files and music

// A file at another rate and in mono is loaded as stereo at the engine's
// rate
TEST(FileTest, LoadsAndResamples) {
  const std::string path = writeWav("tone.wav", 24000, 1, 2400, [](std::uint32_t i, std::uint32_t) {
    return 0.5f * std::sin(2 * 3.14159265f * 440.0f * static_cast<float>(i) / 24000.0f);
  });
  Offline offline;
  const LoadResult result = offline.engine.loadFile("tone", path);
  ASSERT_TRUE(result.ok) << result.error;
  offline.engine.play("tone");
  const std::vector<float> out = offline.render(6000);
  // 0.1 s: 4800 frames at 48 kHz, then silence; a 440 Hz sine of 0.5,
  // with the same on both sides
  double power = 0;
  int crossings = 0;
  for (int i = 200; i < 4600; ++i) {
    power += out[2 * i] * out[2 * i];
    EXPECT_FLOAT_EQ(out[2 * i], out[2 * i + 1]);
    crossings += (out[2 * i] < 0) != (out[2 * i - 2] < 0);
  }
  EXPECT_NEAR(std::sqrt(power / 4400), 0.5 / std::sqrt(2.0), 0.01); // RMS of a sine
  EXPECT_NEAR(crossings, 2.0 * 440 * 4400 / 48000, 2);
  EXPECT_EQ(out[2 * 5000], 0.0f);
}

TEST(FileTest, MissingFileIsAnError) {
  Offline offline;
  const LoadResult result = offline.engine.loadFile("nope", testing::TempDir() + "missing.wav");
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.error.find("can't open"), std::string::npos) << result.error;
  EXPECT_FALSE(offline.engine.hasSound("nope"));
}

namespace {
// A stereo WAV at the engine's rate whose left channel counts frames (so a
// test can see exactly which frame plays), right is minus that
std::string countingWav(const std::string &name, std::uint32_t frames) {
  return writeWav(name, kRate, 2, frames, [](std::uint32_t i, std::uint32_t c) {
    const float value = static_cast<float>(i % 1000) / 1000.0f;
    return c == 0 ? value : -value;
  });
}
} // namespace

// Music streams from the file as it plays, frame for frame, while update()
// keeps decoding ahead, and finishes at the end
TEST(MusicTest, StreamsTheWholeFile) {
  const std::string path = countingWav("music.wav", kRate * 2); // 2 s
  Offline offline;
  std::string error;
  const VoiceId voice = started(offline.engine.playMusic(path, {}, error), error);
  std::vector<VoiceId> finished;
  for (std::uint32_t frame = 0; frame < kRate * 2; frame += 512) {
    const std::vector<float> out = offline.render(512);
    for (std::uint32_t i = 0; i < 512 && frame + i < kRate * 2; ++i)
      ASSERT_FLOAT_EQ(out[2 * i], static_cast<float>((frame + i) % 1000) / 1000.0f)
          << "frame " << frame + i;
    const auto done = offline.engine.update();
    finished.insert(finished.end(), done.begin(), done.end());
  }
  offline.render(512);
  const auto done = offline.engine.update();
  finished.insert(finished.end(), done.begin(), done.end());
  EXPECT_EQ(finished, std::vector<VoiceId>{voice});
  EXPECT_EQ(offline.engine.stats().underruns.load(), 0u);
}

// Looping music goes back to the start without a gap
TEST(MusicTest, LoopsWithoutAGap) {
  const std::string path = countingWav("loop.wav", 3000); // 3 rounds of 0..999
  Offline offline;
  std::string error;
  ASSERT_TRUE(offline.engine.playMusic(path, {.loop = true}, error)) << error;
  for (std::uint32_t frame = 0; frame < 10000; frame += 500) {
    const std::vector<float> out = offline.render(500);
    for (std::uint32_t i = 0; i < 500; ++i)
      ASSERT_FLOAT_EQ(out[2 * i], static_cast<float>((frame + i) % 1000) / 1000.0f)
          << "frame " << frame + i;
    offline.engine.update();
  }
}

// If nothing decodes ahead (update() not called), the music runs dry: that's
// counted, and it carries on once decoding catches up
TEST(MusicTest, RunningDryIsCounted) {
  const std::string path = countingWav("dry.wav", kRate * 3);
  Offline offline;
  std::string error;
  ASSERT_TRUE(offline.engine.playMusic(path, {}, error)) << error;
  for (int i = 0; i < 60; ++i) // 0.64 s, more than the half second decoded ahead
    offline.render(512);
  EXPECT_GT(offline.engine.stats().underruns.load(), 0u);
  offline.engine.update();
  EXPECT_NE(offline.render(512)[2], 0.0f);
}

TEST(MusicTest, MissingFileIsAnError) {
  Offline offline;
  std::string error;
  EXPECT_FALSE(offline.engine.playMusic(testing::TempDir() + "missing.mp3", {}, error));
  EXPECT_NE(error.find("can't open"), std::string::npos) << error;
}

// ---------------------------------------------------------------------------
// The audio thread's rules, and speed

// Mixing allocates nothing, with sounds, music, pitch, loops, stops and
// volume changes all going on
TEST(RealTimeTest, MixingAllocatesNothing) {
  const std::string path = countingWav("busy.wav", kRate);
  Offline offline;
  offline.load("ramp", ramp(300));
  std::string error;
  ASSERT_TRUE(offline.engine.playMusic(path, {.loop = true}, error));
  std::vector<VoiceId> voices;
  voices.reserve(8);
  for (int i = 0; i < 8; ++i)
    voices.push_back(started(offline.engine.play(
        "ramp", {.pan = 0.1f * i, .pitch = 0.7f + 0.1f * i, .loop = i % 2 == 0})));
  offline.engine.stop(voices[0]);
  offline.engine.set(voices[2], {.volume = 0.3f});
  offline.engine.setMasterVolume(0.8f);
  std::vector<float> out(2 * 512);
  const std::uint32_t before = allocationsWhileMixing();
  for (int block = 0; block < 20; ++block)
    offline.output.render(out.data(), 512);
  EXPECT_EQ(allocationsWhileMixing(), before);
}

// The check above would see an allocation: one made while mixing is
// counted, and one made otherwise isn't. (Calling operator new itself: an
// optimizer may drop a `new` expression that's deleted at once.)
TEST(RealTimeTest, AllocationsWhileMixingAreCounted) {
  const std::uint32_t before = allocationsWhileMixing();
  ::operator delete(::operator new(16));
  EXPECT_EQ(allocationsWhileMixing(), before);
  {
    const MixingScope mixing;
    ::operator delete(::operator new(16));
  }
  EXPECT_EQ(allocationsWhileMixing(), before + 1);
}

// One second of audio with every voice busy (pitched, so interpolated, and
// looping) mixes in well under a second: here, under half, which leaves
// room for sanitizers and busy machines (unoptimised, it takes about 15 ms
// on an M-series Mac). The figure is printed, to compare machines.
TEST(SpeedTest, ASecondOfAFullMix) {
  Offline offline({.maxVoices = 32});
  offline.load("ramp", ramp(4800));
  for (int i = 0; i < 32; ++i)
    ASSERT_TRUE(offline.engine.play("ramp", {.pitch = 1.3f, .loop = true}));
  std::vector<float> out(2 * 512);
  const auto start = std::chrono::steady_clock::now();
  for (std::uint32_t frame = 0; frame < kRate; frame += 512)
    offline.output.render(out.data(), 512);
  const double seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("32 voices, 1 s of audio at %u Hz: mixed in %.2f ms\n", kRate, seconds * 1000);
  EXPECT_LT(seconds, 0.5);
}
