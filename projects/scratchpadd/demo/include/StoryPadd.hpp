#pragma once
#include <DemoMessages.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/Base.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

using namespace std::chrono_literals;
class StoryPadd : public ScratchPadd::Base {
  ScratchPadd::Timer performanceTimer_;
  int32_t chapter_ = 0;
  ScratchPadd::PaddAddress audio_; // AudioPadd, if the system has one

  // A soft bell, made in code: two sine partials dying away over 0.8 s
  static ScratchPadd::Audio::LoadSoundSamples chime() {
    constexpr std::uint32_t kRate = 48000;
    constexpr float kTwoPi = 6.2831853f;
    auto samples = std::make_shared<std::vector<float>>(kRate * 8 / 10);
    for (std::size_t i = 0; i < samples->size(); ++i) {
      const float t = static_cast<float>(i) / kRate;
      const float attack = std::min(1.0f, t * 200.0f); // 5 ms, so it doesn't click
      (*samples)[i] =
          attack * std::exp(-5.0f * t) *
          (0.6f * std::sin(kTwoPi * 880.0f * t) + 0.25f * std::sin(kTwoPi * 2.76f * 880.0f * t));
    }
    return {.name = "chime", .samples = std::move(samples), .rate = kRate, .channels = 1};
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
    // Each chapter rings the chime, a step further up a pentatonic scale
    // (pitch is playback speed: 2^(semitones/12))
    if (audio_) {
      constexpr int kScale[] = {0, 2, 4, 7, 9};
      const int step = kScale[chapter_ % 5] + 12 * ((chapter_ / 5) % 2);
      sendTo(audio_, ScratchPadd::Audio::PlaySound{
                         .name = "chime",
                         .settings = {.volume = 0.3f,
                                      .pan = chapter_ % 2 == 0 ? -0.5f : 0.5f,
                                      .pitch = std::exp2(static_cast<float>(step) / 12.0f)}});
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