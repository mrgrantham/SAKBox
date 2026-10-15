#pragma once
#include <ScratchPadd/Base.hpp>

class AudioPadd : public ScratchPadd::Base {
public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override { spdlog::info("Preparing: {}", name()); }

  AudioPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
    on<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << name() << " Text: " << text << "\n";
    });
  }
  virtual ~AudioPadd() { spdlog::info("Destroying: {}", name()); }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }
};