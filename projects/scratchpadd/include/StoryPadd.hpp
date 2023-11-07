#pragma once
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
  }

  void repeat() override {
    spdlog::info("Repeating: {}", name());
    performanceTimer_.markInterval();
    spdlog::info("{} firing at {:.3f}s interval", name(),
                 performanceTimer_.getAverageIntervalInSeconds(5));
  }

  ~StoryPadd() { spdlog::info("Destroying: {}", name()); }

  void starting() override {
    ScratchPadd::MessageType::Triangle triangle;
    // send(ScratchPadd::MakeMsg(triangle));
    send(triangle);
  }
  std::optional<std::chrono::milliseconds> repeatInterval() override {
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

  void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Triangle &message) {
                     std::cout << name() << "Triangle: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Point &message) {
                     std::cout << name() << "Point: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Text &message) {
                     std::cout << name() << "Text: " << message << "\n";
                   },
                   [&](auto &message) { std::cout << name() << " auto\n"; }},
               messageVariant);
  }
};