#pragma once
#include <ScratchPadd/Base.hpp>
#include <optional>

class StoryPadd : public ScratchPadd::Base {
  ScratchPadd::Timer performanceTimer_;

public:
  void prepare() override {
    spdlog::info("Preparing: {}", __CLASS_NAME__);
    paddName_ = __CLASS_NAME__;
    setRepeatInterval(1000);
    performanceTimer_.setTimerName(paddName_);
    performanceTimer_.start();
  }
  StoryPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", __CLASS_NAME__);
  }

  void repeat() override {
    spdlog::info("Repeating: {}", __CLASS_NAME__);
    performanceTimer_.markInterval();
    spdlog::info("{} firing at {:.3f}s interval", paddName_,
                 performanceTimer_.getAverageIntervalInSeconds(5));
  }

  ~StoryPadd() { spdlog::info("Destroying: {}", __CLASS_NAME__); }

  void starting() override {
    ScratchPadd::MessageType::Triangle triangle;
    // send(ScratchPadd::MakeMsg(triangle));
    send(triangle);
  }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>  generateControls() override {
    return {
        {"tellastory", ScratchPadd::ControlType::Double(
                           20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aDouble",
         ScratchPadd::ControlType::Double(
             20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aString", ScratchPadd::ControlType::String(
                        "DefaultString",
                        std::make_optional(std::vector<std::string>(
                            {"DefaultString", "AnotherString", "YetAnother"})))},
        {"aBool",
         ScratchPadd::ControlType::Boolean(
             true)},
        {"anInt",
         ScratchPadd::ControlType::Integer(
             5, std::make_optional(std::make_pair(2, 9)))}};
  }

  void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Triangle &message) {
                     std::cout << paddName_ << "Triangle: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Point &message) {
                     std::cout << paddName_ << "Point: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Text &message) {
                     std::cout << paddName_ << "Text: " << message << "\n";
                   },
                   [&](auto &message) { std::cout << paddName_ << " auto\n"; }},
               messageVariant);
  }
};