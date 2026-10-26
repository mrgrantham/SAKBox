// AudioPadd in a running system, played through an OfflineOutput that the
// test renders from its own thread (the audio thread here), so every answer
// and every sample can be checked.
#include <AudioPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include "MidiTestData.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace ScratchPadd::Audio;
using Access = ScratchPadd::PaddTestAccess;

namespace {

// Asks AudioPadd for things and keeps its answers
template <int N> class ListenerPadd : public ScratchPadd::Base {
public:
  ListenerPadd(ScratchPadd::System *system) : Base(system) {
    receive<SoundLoaded>([this](const SoundLoaded &loaded) { keep(loaded_, loaded); });
    receive<Playing>([this](const Playing &playing) { keep(playing_, playing); });
    receive<VoiceFinished>([this](const VoiceFinished &done) { keep(finished_, done); });
    receive<SoundFontLoaded>([this](const SoundFontLoaded &loaded) { keep(fonts_, loaded); });
  }
  std::string name() override { return "Listener" + std::to_string(N); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Sends `contents` to AudioPadd, as this padd
  template <typename T> void ask(T contents) {
    ASSERT_TRUE(Access::sendTo(*this, Access::addressOf<AudioPadd>(*this), std::move(contents)));
  }

  std::vector<SoundLoaded> loaded() { return copy(loaded_); }
  std::vector<Playing> playing() { return copy(playing_); }
  std::vector<VoiceFinished> finished() { return copy(finished_); }
  std::vector<SoundFontLoaded> fonts() { return copy(fonts_); }

private:
  template <typename T> void keep(std::vector<T> &list, const T &item) {
    std::lock_guard<std::mutex> lk(mutex_);
    list.push_back(item);
  }
  template <typename T> std::vector<T> copy(const std::vector<T> &list) {
    std::lock_guard<std::mutex> lk(mutex_);
    return list;
  }
  std::mutex mutex_;
  std::vector<SoundLoaded> loaded_;
  std::vector<Playing> playing_;
  std::vector<VoiceFinished> finished_;
  std::vector<SoundFontLoaded> fonts_;
};
using Listener = ListenerPadd<0>;

// Plays sounds but doesn't listen for any answer
class QuietPadd : public ScratchPadd::Base {
public:
  QuietPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Quiet"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};

template <typename Condition> bool eventually(Condition condition) {
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline)
      return false;
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

// A system with AudioPadd playing into an OfflineOutput the test renders
template <typename... Padds> class AudioSystem {
public:
  ScratchPadd::SystemImplementation<AudioPadd, Padds...> system;
  OfflineOutput *output = nullptr;
  std::thread mainThread;

  AudioSystem() {
    AudioPadd::outputFactory() = [this] {
      auto offline = std::make_unique<OfflineOutput>();
      output = offline.get();
      return offline;
    };
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~AudioSystem() {
    system.stop();
    mainThread.join();
    AudioPadd::outputFactory() = [] { return std::make_unique<DeviceOutput>(); };
  }

  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
  bool settles() {
    return eventually([&] {
      return (padd<AudioPadd>().pendingWorkCount() + ... + padd<Padds>().pendingWorkCount()) == 0;
    });
  }
  // The next `frames` frames, interleaved stereo
  std::vector<float> render(std::uint32_t frames) {
    std::vector<float> out(2 * frames);
    output->render(out.data(), frames);
    return out;
  }
};

LoadSoundSamples constantSamples(const std::string &name, std::uint32_t frames, float value) {
  return {.name = name,
          .samples = std::make_shared<const std::vector<float>>(frames, value),
          .rate = 48000,
          .channels = 1};
}

// Writes `bytes` to a file in the test's temporary folder; returns its path
std::string writeFile(const std::string &name, const std::vector<std::uint8_t> &bytes) {
  const std::string path = testing::TempDir() + name;
  std::ofstream(path, std::ios::binary)
      .write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  return path;
}

// The loudest sample of the next `frames` frames
float loudest(const std::vector<float> &stereo) {
  float peak = 0;
  for (float sample : stereo)
    peak = std::max(peak, std::abs(sample));
  return peak;
}

} // namespace

// Loading and playing are answered, and the padd that played a sound hears
// when it ends
TEST(AudioPaddTest, AnswersAndReportsTheEnd) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  listener.ask(constantSamples("beep", 100, 0.5f));
  listener.ask(PlaySound{.name = "beep"});
  ASSERT_TRUE(running.settles());
  ASSERT_EQ(listener.loaded().size(), 1u);
  EXPECT_TRUE(listener.loaded()[0].ok) << listener.loaded()[0].error;
  ASSERT_EQ(listener.playing().size(), 1u);
  const Playing playing = listener.playing()[0];
  EXPECT_TRUE(playing.ok) << playing.error;
  EXPECT_EQ(playing.name, "beep");

  const std::vector<float> out = running.render(128);
  EXPECT_FLOAT_EQ(out[0], 0.5f); // mono, played on both sides
  EXPECT_FLOAT_EQ(out[1], 0.5f);
  EXPECT_EQ(out[2 * 100], 0.0f);
  ASSERT_TRUE(eventually([&] { return listener.finished().size() == 1; }));
  EXPECT_EQ(listener.finished()[0].voice, playing.voice);
  EXPECT_EQ(listener.finished()[0].name, "beep");
}

// What can't be done is answered with why
TEST(AudioPaddTest, FailuresAreAnswered) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  listener.ask(LoadSound{.name = "gone", .path = testing::TempDir() + "missing.wav"});
  listener.ask(LoadSoundSamples{.name = "empty"});
  listener.ask(PlaySound{.name = "gone"});
  listener.ask(PlayMusic{.path = testing::TempDir() + "missing.mp3"});
  ASSERT_TRUE(running.settles());
  const auto loaded = listener.loaded();
  ASSERT_EQ(loaded.size(), 2u);
  EXPECT_FALSE(loaded[0].ok);
  EXPECT_NE(loaded[0].error.find("can't open"), std::string::npos) << loaded[0].error;
  EXPECT_FALSE(loaded[1].ok);
  EXPECT_EQ(loaded[1].error, "no samples");
  const auto playing = listener.playing();
  ASSERT_EQ(playing.size(), 2u);
  EXPECT_FALSE(playing[0].ok);
  EXPECT_EQ(playing[0].error, "no sound named gone");
  EXPECT_EQ(playing[0].voice, 0u);
  EXPECT_FALSE(playing[1].ok);
  EXPECT_NE(playing[1].error.find("can't open"), std::string::npos) << playing[1].error;
}

// A padd that doesn't listen for answers can still play sounds
TEST(AudioPaddTest, PlayingWithoutListening) {
  AudioSystem<Listener, QuietPadd> running;
  running.padd<Listener>().ask(constantSamples("beep", 100, 0.5f));
  ASSERT_TRUE(running.settles());
  auto &quiet = running.padd<QuietPadd>();
  EXPECT_TRUE(
      Access::sendTo(quiet, Access::addressOf<AudioPadd>(quiet), PlaySound{.name = "beep"}));
  ASSERT_TRUE(running.settles());
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.5f);
}

// The volume controls scale what's played, and mute silences it
TEST(AudioPaddTest, VolumeControls) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  const auto change = [&](const std::string &name, ScratchPadd::ControlTypeVariant value) {
    listener.ask(ScratchPadd::MessageType::ControlChange{.controlName = name,
                                                         .controlTypeValue = std::move(value)});
    ASSERT_TRUE(running.settles());
    running.render(1024); // let the volume ramp there
  };
  listener.ask(constantSamples("hum", 48000, 1.0f));
  listener.ask(PlaySound{.name = "hum", .settings = {.loop = true}});
  ASSERT_TRUE(running.settles());
  EXPECT_FLOAT_EQ(running.render(16)[0], 1.0f);
  change("master", ScratchPadd::ControlType::Double(0.5));
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.5f);
  change("sounds", ScratchPadd::ControlType::Double(0.5));
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.25f);
  change("music", ScratchPadd::ControlType::Double(0.0)); // not this group
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.25f);
  change("mute", ScratchPadd::ControlType::Boolean(true));
  EXPECT_EQ(running.render(16)[0], 0.0f);
  change("mute", ScratchPadd::ControlType::Boolean(false));
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.25f);
}

// Stopping: one voice, all music, or everything; each still reported
TEST(AudioPaddTest, Stopping) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  listener.ask(constantSamples("hum", 4800, 0.25f));
  listener.ask(PlaySound{.name = "hum", .settings = {.loop = true}});
  listener.ask(PlaySound{.name = "hum", .settings = {.loop = true}});
  ASSERT_TRUE(running.settles());
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.5f);
  listener.ask(StopVoice{.voice = listener.playing()[0].voice});
  ASSERT_TRUE(running.settles());
  running.render(2048);
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.25f);
  ASSERT_TRUE(eventually([&] { return listener.finished().size() == 1; }));
  EXPECT_EQ(listener.finished()[0].voice, listener.playing()[0].voice);
  listener.ask(StopMusic{}); // there's no music: the sound plays on
  ASSERT_TRUE(running.settles());
  running.render(2048);
  EXPECT_FLOAT_EQ(running.render(16)[0], 0.25f);
  listener.ask(StopAll{});
  ASSERT_TRUE(running.settles());
  running.render(2048);
  EXPECT_EQ(running.render(16)[0], 0.0f);
  ASSERT_TRUE(eventually([&] { return listener.finished().size() == 2; }));
}

// Four padds asking at once from their own threads, while the audio thread
// mixes: every ask is answered once, and every voice that played is
// reported finished once. (Run under ThreadSanitizer too.)
TEST(AudioPaddTest, ManyPaddsAtOnce) {
  AudioSystem<ListenerPadd<1>, ListenerPadd<2>, ListenerPadd<3>, ListenerPadd<4>> running;
  running.padd<ListenerPadd<1>>().ask(constantSamples("tick", 64, 0.1f));
  ASSERT_TRUE(running.settles());
  constexpr int kEach = 100;
  std::atomic<bool> mixing{true};
  std::thread audioThread([&] {
    std::vector<float> out(2 * 256);
    while (mixing) {
      running.output->render(out.data(), 256);
      std::this_thread::sleep_for(100us);
    }
  });
  const auto askMany = [&](auto &padd) {
    Access::push(padd, [&padd] {
      for (int i = 0; i < kEach; ++i)
        padd.ask(PlaySound{.name = "tick"});
    });
  };
  askMany(running.padd<ListenerPadd<1>>());
  askMany(running.padd<ListenerPadd<2>>());
  askMany(running.padd<ListenerPadd<3>>());
  askMany(running.padd<ListenerPadd<4>>());
  // (Which asks get a voice depends on timing: one padd may find them all
  // in use. Some play, overall.)
  std::size_t playedOverall = 0;
  const auto check = [&](auto &padd) {
    ASSERT_TRUE(eventually([&] { return padd.playing().size() == kEach; }));
    std::size_t played = 0;
    for (const Playing &playing : padd.playing())
      played += playing.ok;
    playedOverall += played;
    ASSERT_TRUE(eventually([&] { return padd.finished().size() == played; }));
  };
  check(running.padd<ListenerPadd<1>>());
  check(running.padd<ListenerPadd<2>>());
  check(running.padd<ListenerPadd<3>>());
  check(running.padd<ListenerPadd<4>>());
  EXPECT_GT(playedOverall, 0u);
  mixing = false;
  audioThread.join();
}

// A MIDI file plays (answered, then reported finished); events play now
TEST(AudioPaddTest, PlaysMidi) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  // A note for the first quarter second (240 ticks)
  const std::string path =
      writeFile("tune.mid", MidiTestData::midiFile({{0, 0x90, 69, 127}, {240, 0x80, 69, 0}}));
  listener.ask(PlayMidi{.path = path});
  ASSERT_TRUE(running.settles());
  ASSERT_EQ(listener.playing().size(), 1u);
  EXPECT_TRUE(listener.playing()[0].ok) << listener.playing()[0].error;
  EXPECT_GT(loudest(running.render(4800)), 0.01f);
  running.render(48000);
  ASSERT_TRUE(eventually([&] { return listener.finished().size() == 1; }));
  EXPECT_EQ(listener.finished()[0].name, path);

  running.render(48000); // the note dies away
  listener.ask(PlayMidiEvent{.event = MidiEvent::noteOn(0, 60, 127)});
  ASSERT_TRUE(running.settles());
  EXPECT_GT(loudest(running.render(4800)), 0.01f);
}

// StopMusic stops a MIDI file too
TEST(AudioPaddTest, StopMusicStopsMidi) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  const std::string path =
      writeFile("long.mid", MidiTestData::midiFile({{0, 0x90, 60, 100}, {9600, 0x80, 60, 0}}));
  listener.ask(PlayMidi{.path = path});
  ASSERT_TRUE(running.settles());
  running.render(4800);
  listener.ask(StopMusic{});
  ASSERT_TRUE(running.settles());
  running.render(1024);
  ASSERT_TRUE(eventually([&] { return listener.finished().size() == 1; }));
}

// Loading a SoundFont is answered; one that can't be read says why
TEST(AudioPaddTest, LoadsSoundFonts) {
  AudioSystem<Listener> running;
  auto &listener = running.padd<Listener>();
  listener.ask(LoadSoundFont{.path = testing::TempDir() + "missing.sf2"});
  const std::string path = writeFile("sine.sf2", MidiTestData::soundFont());
  listener.ask(LoadSoundFont{.path = path});
  listener.ask(PlayMidi{.path = testing::TempDir() + "missing.mid"});
  ASSERT_TRUE(running.settles());
  const auto fonts = listener.fonts();
  ASSERT_EQ(fonts.size(), 2u);
  EXPECT_FALSE(fonts[0].ok);
  EXPECT_NE(fonts[0].error.find("can't read"), std::string::npos) << fonts[0].error;
  EXPECT_TRUE(fonts[1].ok) << fonts[1].error;
  EXPECT_EQ(fonts[1].path, path);
  ASSERT_EQ(listener.playing().size(), 1u);
  EXPECT_FALSE(listener.playing()[0].ok);
  listener.ask(PlayMidiEvent{.event = MidiEvent::noteOn(0, 69, 127)});
  listener.ask(UseBuiltInInstrument{});
  ASSERT_TRUE(running.settles());
  running.render(256);
}
