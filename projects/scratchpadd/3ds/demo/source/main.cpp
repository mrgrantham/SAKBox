// ScratchPadd on a Nintendo 3DS: three padds, two of them on threads of
// their own, talking only by messages, as on a desktop.
//
//   ScreenPadd     (main thread) the frame loop: reads the buttons and sends
//                  them as messages; shows what the other padds send it on
//                  the top screen (status lines, then a log)
//   SoundBoardPadd (own thread)  gives AudioPadd sounds made in code, plays
//                  them when buttons are pressed, and a tune on a timer;
//                  logs AudioPadd's answers (a chime plays once all are
//                  loaded, so the log shows sound works without a button)
//   AudioPadd      (own thread)  plays them through the 3DS's DSP
//
// A, B and X play a chime, a knock and a blast; Y starts and stops the
// tune; START quits.
#include <3ds.h>

#include <AudioPadd.hpp>
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace {

// A button pressed this frame
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
std::string threadNumber() {
  return std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000);
}

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system) : Base(system) {
    receive<StatusLine>([this](const StatusLine &line) { status_[line.row] = line.text; });
    receive<LogLine>([this](const LogLine &line) {
      log_.push_back(line.text);
      while (log_.size() > kLogLines) {
        log_.pop_front();
      }
    });
  }
  std::string name() override { return "ScreenPadd"; }
  // The frame loop and the HOME Menu's checks, on the main thread as in devkitPro's examples
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    consoleInit(GFX_TOP, &top_);
    status_[0] = "ScreenPadd: main thread " + threadNumber();
    push([this] { frame(); });
  }

private:
  static constexpr std::size_t kLogLines = 18;

  // One frame, then the next (gspWaitForVBlank paces them at 60 a second)
  void frame() {
    if (!aptMainLoop()) {
      system_->stop();
      return;
    }
    hidScanInput();
    const u32 down = hidKeysDown();
    if (down & KEY_START) {
      system_->stop();
      return;
    }
    static const std::pair<u32, const char *> kButtons[] = {
        {KEY_A, "A"}, {KEY_B, "B"}, {KEY_X, "X"}, {KEY_Y, "Y"}};
    for (const auto &[key, button] : kButtons) {
      if (down & key) {
        send(ButtonPressed{button});
      }
    }
    draw();
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    push([this] { frame(); });
  }

  // The title, the status lines, the buttons, then the log (rows are 1-based)
  void draw() {
    std::printf("\x1b[1;1HScratchPadd on a 3DS: padds talking by messages\x1b[K");
    int row = 3;
    for (const auto &[index, text] : status_) {
      std::printf("\x1b[%d;1H%.49s\x1b[K", row++, text.c_str());
    }
    std::printf("\x1b[%d;1HA chime  B knock  X blast  Y tune  START quit\x1b[K", ++row);
    row += 2;
    for (const std::string &line : log_) {
      std::printf("\x1b[%d;1H%.49s\x1b[K", row++, line.c_str());
    }
  }

  PrintConsole top_;
  std::map<int, std::string> status_;
  std::deque<std::string> log_;
};

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

  // Gives AudioPadd the sounds, made in code
  void prepare() override {
    audio_ = addressOf("AudioPadd");
    using namespace ScratchPadd::Audio;
    constexpr std::uint32_t kRate = 32728; // the DSP's own rate, so nothing is resampled
    const auto load = [&](const char *name, std::vector<float> samples) {
      sendTo(audio_, LoadSoundSamples{
                         .name = name,
                         .samples = std::make_shared<const std::vector<float>>(std::move(samples)),
                         .rate = kRate,
                         .channels = 1});
    };
    load("chime", MadeSounds::chime(kRate));
    load("knock", MadeSounds::knock(kRate));
    load("blast", MadeSounds::blast(kRate));
  }

  void starting() override {
    send(StatusLine{1, "SoundBoardPadd: own thread " + threadNumber()});
    // The tune's instrument: a vibraphone (General MIDI's 12th) on channel 1
    sendTo(audio_,
           ScratchPadd::Audio::PlayMidiEvent{ScratchPadd::Audio::MidiEvent::programChange(0, 11)});
  }

  void repeat() override {
    if (!tunePlaying_) {
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
      if (!tunePlaying_ && lastKey_) {
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
  std::optional<int> lastKey_;
};

} // namespace

int main() {
  gfxInitDefault();
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  {
    ScratchPadd::SystemImplementation<ScreenPadd, SoundBoardPadd, AudioPadd> system;
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START, or the HOME Menu closing it)
  }
  gfxExit();
  return 0;
}
