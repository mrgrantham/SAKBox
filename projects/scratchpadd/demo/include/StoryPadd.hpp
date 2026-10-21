#pragma once
#include <DemoMessages.hpp>
#include <ScratchPadd/Base.hpp>
#include <optional>

using namespace std::chrono_literals;
class StoryPadd : public ScratchPadd::Base {
  ScratchPadd::Timer performanceTimer_;
  int32_t chapter_ = 0;

public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override {
    spdlog::info("Preparing: {}", paddName());
    performanceTimer_.setTimerName(paddName());
    performanceTimer_.start();
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