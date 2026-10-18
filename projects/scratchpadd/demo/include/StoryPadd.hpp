#pragma once
#include <DemoMessages.hpp>
#include <ScratchPadd/Base.hpp>
#include <optional>

using namespace std::chrono_literals;
class StoryPadd : public ScratchPadd::Base {
  ScratchPadd::Timer performanceTimer_;

public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override {
    spdlog::info("Preparing: {}", name());
    performanceTimer_.setTimerName(name());
    performanceTimer_.start();
  }
  StoryPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << name() << " Text: " << text << "\n";
    });
  }

  void repeat() override {
    spdlog::info("Repeating: {}", name());
    performanceTimer_.markInterval();
    spdlog::info("{} firing at {:.3f}s interval", name(),
                 performanceTimer_.getAverageIntervalInSeconds(5));
  }

  ~StoryPadd() override { spdlog::info("Destroying: {}", name()); }

  // Called when a control is edited in DisplayPadd's control panel.
  void controlChanged(const std::string &controlName) override {
    std::visit(
        [&](const auto &control) {
          spdlog::info("{}: control {} is now {}", name(), controlName, control.value);
        },
        generateControlsSnapshot().controlMap.at(controlName));
  }

  void starting() override {
    // A message type defined by the demo, not the library (DemoMessages.hpp)
    send(Demo::Triangle{.position = {.x = 1, .y = 2}});
    send(ScratchPadd::MessageType::Text{"Once upon a time..."});
  }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return 1000ms;
  }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {
        {"tellastory",
         ScratchPadd::ControlType::Double(
             20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aDouble", ScratchPadd::ControlType::Double(
                        20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aString",
         ScratchPadd::ControlType::String(
             "DefaultString",
             std::make_optional(std::vector<std::string>(
                 {"DefaultString", "AnotherString", "YetAnother"})))},
        {"aBool", ScratchPadd::ControlType::Boolean(true)},
        {"anInt", ScratchPadd::ControlType::Integer(
                      5, std::make_optional(std::make_pair(2, 9)))}};
  }
};