#pragma once
#include <ScratchPadd/Base.hpp>

class AudioPadd : public ScratchPadd::Base {
public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override { spdlog::info("Preparing: {}", name()); }

  AudioPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }
  virtual ~AudioPadd() { spdlog::info("Destroying: {}", name()); }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }

  void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Triangle &message) {
                     std::cout << name() << " Triangle: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Point &message) {
                     std::cout << name() << " Point: " << message << "\n";
                   },
                   [&](ScratchPadd::MessageType::Text &message) {
                     std::cout << name() << " Text: " << message << "\n";
                   },
                   [&](auto &message) { std::cout << name() << " auto\n"; }},
               messageVariant);
  }
};