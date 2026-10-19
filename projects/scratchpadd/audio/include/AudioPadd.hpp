#pragma once
#include <ScratchPadd/Base.hpp>

class AudioPadd : public ScratchPadd::Base {
public:
  std::string name() override { return __CLASS_NAME__; }
  void prepare() override { spdlog::info("Preparing: {}", paddName()); }

  AudioPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << paddName() << " Text: " << text << "\n";
    });
  }
  ~AudioPadd() override { spdlog::info("Destroying: {}", paddName()); }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }
};