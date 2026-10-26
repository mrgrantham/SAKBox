#pragma once

// What the ScratchPadd demos for the Nintendo 3DS and DS share: the messages
// their padds send, and SoundBoardPadd. Each demo adds its own ScreenPadd
// (the frame loop, buttons and text, which differ between the two), and
// AudioPadd (AudioPadd.hpp picks the platform's).

#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace HandheldDemo {

// A button pressed this frame ("A", "B", "X" or "Y")
struct ButtonPressed {
  std::string button;
};

// A status line on the top screen (row 0 is the first under the title), or
// a line for the log under them
struct StatusLine {
  int row = 0;
  std::string text;
};
struct LogLine {
  std::string text;
};

// A short number for the thread this runs on, to show which padd is where
inline std::string threadNumber() {
  return std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
}

// On the DS, the sounds and the tune are files in the app's file system
// (NitroFS, packed into the .nds when it's built: see BUILD's demo_files);
// elsewhere they're compiled in
#if defined(__NDS__)
inline constexpr bool kSoundsFromFiles = true;
#else
inline constexpr bool kSoundsFromFiles = false;
#endif

// Gives AudioPadd a chime, a knock and a blast made in code, plays them for
// A, B and X, and a tune for Y; logs AudioPadd's answers, and plays the
// chime once all three have loaded, so the log shows sound works without a
// button.
// - On the DS: WAV files (nitro:/sounds/chime.wav, 16-bit; knock.wav, 8-bit;
//   blast.wav, IMA-ADPCM), and the tune is a MIDI file played by AudioPadd
//   (nitro:/music/tune.mid, looping).
// - Elsewhere: compiled in, rendered when the app is built (MadeSoundsPcm16),
//   and the tune is a note every 250 ms from repeat().
class SoundBoardPadd : public ScratchPadd::Base {
public:
  SoundBoardPadd(ScratchPadd::System *system) : Base(system) {
    receive<ButtonPressed>([this](const ButtonPressed &pressed) { press(pressed.button); });
    using namespace ScratchPadd::Audio;
    receive<SoundLoaded>([this](const SoundLoaded &loaded) {
      send(LogLine{loaded.name + (loaded.ok ? " loaded" : " not loaded: " + loaded.error)});
      if (loaded.ok && ++loaded_ == kSounds) {
        sendTo(audio_, PlaySound{.name = "chime"});
      }
    });
    receive<Playing>([this](const Playing &playing) {
      send(LogLine{"AudioPadd: " + playing.name +
                   (playing.ok ? " playing" : " not playing: " + playing.error)});
    });
  }
  std::string name() override { return "SoundBoardPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  // The tune's beat
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::milliseconds(250);
  }

  // Gives AudioPadd the sounds
  void prepare() override {
    audio_ = addressOf("AudioPadd");
    using namespace ScratchPadd::Audio;
    if constexpr (kSoundsFromFiles) {
      for (const char *name : {"chime", "knock", "blast"}) {
        sendTo(audio_,
               LoadSound{.name = name, .path = std::string("nitro:/sounds/") + name + ".wav"});
      }
      return;
    }
    const auto load = [&](const char *name, std::vector<std::int16_t> samples) {
      sendTo(audio_, LoadSoundPcm16{.name = name,
                                    .samples = std::make_shared<const std::vector<std::int16_t>>(
                                        std::move(samples)),
                                    .rate = MadeSoundsPcm16::kRate,
                                    .channels = 1});
    };
    load("chime", MadeSoundsPcm16::chime());
    load("knock", MadeSoundsPcm16::knock());
    load("blast", MadeSoundsPcm16::blast());
  }

  void starting() override {
    send(StatusLine{1, "SoundBoardPadd: own " + threadNumber()});
    // The tune's instrument: a vibraphone (General MIDI's 12th) on channel 1
    sendTo(audio_,
           ScratchPadd::Audio::PlayMidiEvent{ScratchPadd::Audio::MidiEvent::programChange(0, 11)});
  }

  void repeat() override {
    if (++ticks_ % 4 == 0) { // every second: shows timers work
      send(StatusLine{4, "Timer: " + std::to_string(ticks_ / 4) + " s"});
    }
    if (!tunePlaying_ || kSoundsFromFiles) { // (the MIDI file plays itself)
      return;
    }
    using ScratchPadd::Audio::MidiEvent;
    using ScratchPadd::Audio::PlayMidiEvent;
    constexpr int kTune[] = {72, 76, 79, 76, 74, 77, 81, 77};
    const int key = kTune[beat_++ % std::size(kTune)];
    if (lastKey_) {
      sendTo(audio_, PlayMidiEvent{MidiEvent::noteOff(0, *lastKey_)});
    }
    sendTo(audio_, PlayMidiEvent{MidiEvent::noteOn(0, key, 90)});
    lastKey_ = key;
    send(StatusLine{3, "Tune: beat " + std::to_string(beat_)});
  }

private:
  void press(const std::string &button) {
    static const std::map<std::string, const char *> kSounds = {
        {"A", "chime"}, {"B", "knock"}, {"X", "blast"}};
    if (auto sound = kSounds.find(button); sound != kSounds.end()) {
      sendTo(audio_, ScratchPadd::Audio::PlaySound{.name = sound->second});
      send(LogLine{button + ": " + sound->second + " (thread " + threadNumber() + ")"});
      ++played_;
    } else if (button == "Y") {
      tunePlaying_ = !tunePlaying_;
      if constexpr (kSoundsFromFiles) {
        if (tunePlaying_) {
          sendTo(audio_, ScratchPadd::Audio::PlayMidi{.path = "nitro:/music/tune.mid",
                                                      .settings = {.loop = true}});
        } else {
          sendTo(audio_, ScratchPadd::Audio::StopMusic{});
        }
      } else if (!tunePlaying_ && lastKey_) {
        sendTo(audio_, ScratchPadd::Audio::PlayMidiEvent{
                           ScratchPadd::Audio::MidiEvent::noteOff(0, *lastKey_)});
        lastKey_.reset();
      }
      send(LogLine{std::string("Y: tune ") + (tunePlaying_ ? "on" : "off")});
    }
    send(StatusLine{2, "Sounds played: " + std::to_string(played_)});
  }

  static constexpr int kSounds = 3;
  ScratchPadd::PaddAddress audio_;
  int loaded_ = 0;
  bool tunePlaying_ = false;
  int beat_ = 0;
  int played_ = 0;
  int ticks_ = 0; // repeat()s so far
  std::optional<int> lastKey_;
};

} // namespace HandheldDemo
