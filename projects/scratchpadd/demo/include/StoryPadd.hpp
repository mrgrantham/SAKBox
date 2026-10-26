#pragma once
#include <DataDepRetriever/DataDependencies.hpp>
#include <DemoMessages.hpp>
#include <ScratchPadd/Audio/MadeSounds.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <memory>
#include <optional>
#include <vector>

using namespace std::chrono_literals;
class StoryPadd : public ScratchPadd::Base {
  ScratchPadd::Timer performanceTimer_;
  int32_t chapter_ = 0;
  ScratchPadd::PaddAddress audio_; // AudioPadd, if the system has one
  int lastKey_ = 60;               // the tune's note playing

  // A soft bell, made in code
  static ScratchPadd::Audio::LoadSoundSamples chime() {
    constexpr std::uint32_t kRate = 48000;
    return {.name = "chime",
            .samples = std::make_shared<const std::vector<float>>(
                ScratchPadd::Audio::MadeSounds::chime(kRate)),
            .rate = kRate,
            .channels = 1};
  }

public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override {
    spdlog::info("Preparing: {}", paddName());
    performanceTimer_.setTimerName(paddName());
    performanceTimer_.start();
    audio_ = addressOf("AudioPadd");
  }
  StoryPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << paddName() << " Text: " << text << "\n";
    });
  }

  void repeat() override {
    spdlog::info("Repeating: {}", paddName());
    performanceTimer_.markInterval();
    spdlog::info("{} firing at {:.3f}s interval", paddName(),
                 performanceTimer_.getAverageIntervalInSeconds(5));
    // The padd changes a control itself; the panel shows it straight away
    chapter_ = (chapter_ + 1) % 100;
    setControl("chapter", ScratchPadd::ControlType::Integer(chapter_));
    if (audio_) {
      playChapterMusic();
    }
  }

  // Each chapter plays the next note of a tune up and down a pentatonic
  // scale, on a vibraphone (MIDI), and every fifth rings the chime (a sound
  // made in code)
  void playChapterMusic() {
    using ScratchPadd::Audio::MidiEvent;
    using ScratchPadd::Audio::PlayMidiEvent;
    constexpr int kTune[] = {60, 62, 64, 67, 69, 72, 69, 67, 64, 62};
    const int key = kTune[chapter_ % 10];
    sendTo(audio_, PlayMidiEvent{.event = MidiEvent::noteOff(0, lastKey_)});
    sendTo(audio_, PlayMidiEvent{.event = MidiEvent::noteOn(0, key, 90)});
    lastKey_ = key;
    if (chapter_ % 5 == 0) {
      sendTo(audio_, ScratchPadd::Audio::PlaySound{
                         .name = "chime",
                         .settings = {.volume = 0.3f, .pan = chapter_ % 10 == 0 ? -0.5f : 0.5f}});
    }
  }

  ~StoryPadd() override { spdlog::info("Destroying: {}", paddName()); }

  // Called when a control is edited in DisplayPadd's control panel.
  // Ticking showExtra adds a control, and unticking it removes it again.
  void controlChanged(const std::string &controlName) override {
    std::visit(
        [&](const auto &control) {
          spdlog::info("{}: control {} is now {}", paddName(), controlName, control.value);
        },
        generateControlsSnapshot().controlMap.at(controlName));
    if (controlName == "showExtra") {
      if (control<ScratchPadd::ControlType::Boolean>("showExtra").value)
        addControl("extra", ScratchPadd::ControlType::Double(0.5, std::make_pair(0.0, 1.0)));
      else
        removeControl("extra");
    }
  }

  void starting() override {
    if (audio_) {
      sendTo(audio_, chime());
      // The General MIDI SoundFont Bazel downloads with the demo; the
      // built-in instrument plays until it's loaded, or if it isn't there
      if (const auto font = DataDepRetriever::GetFullDependencyPath(
              "fluidr3_mono_soundfont/file/FluidR3Mono_GM.sf3")) {
        sendTo(audio_, ScratchPadd::Audio::LoadSoundFont{.path = *font});
      }
      sendTo(audio_, ScratchPadd::Audio::PlayMidiEvent{
                         .event = ScratchPadd::Audio::MidiEvent::programChange(0, 11)});
    }
    // A message type defined by the demo, not the library (DemoMessages.hpp)
    send(Demo::Triangle{.position = {.x = 1, .y = 2}});
    send(ScratchPadd::MessageType::Text{"Once upon a time..."});
  }
  std::optional<std::chrono::duration<double>> repeatInterval() override { return 1000ms; }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {
    return {
        {"tellastory",
         ScratchPadd::ControlType::Double(20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aDouble",
         ScratchPadd::ControlType::Double(20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aString", ScratchPadd::ControlType::String(
                        "DefaultString", std::make_optional(std::vector<std::string>(
                                             {"DefaultString", "AnotherString", "YetAnother"})))},
        {"aBool", ScratchPadd::ControlType::Boolean(true)},
        {"anInt", ScratchPadd::ControlType::Integer(5, std::make_optional(std::make_pair(2, 9)))},
        // Advanced by the padd every second (see repeat())
        {"chapter", ScratchPadd::ControlType::Integer(0, std::make_pair(0, 99))},
        // Adds or removes the "extra" control (see controlChanged())
        {"showExtra", ScratchPadd::ControlType::Boolean(false)}};
  }
};