#pragma once
#include <ScratchPadd/Base.hpp>

class AudioPadd : public ScratchPadd::Base {
  public:
  virtual void prepare() override {
    spdlog::info("Preparing: {}",__CLASS_NAME__);
    paddName_ = __CLASS_NAME__;
  }

  AudioPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}",__CLASS_NAME__);
  }
  virtual ~AudioPadd() {
    spdlog::info("Destroying: {}", __CLASS_NAME__);
  }

  void initializeControls() override {
  }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
        [&](ScratchPadd::MessageType::Triangle& message)       { std::cout << paddName_ << " Triangle: " << message <<"\n"; },
        [&](ScratchPadd::MessageType::Point& message)   { std::cout << paddName_ << " Point: " << message << "\n"; },
        [&](ScratchPadd::MessageType::Text& message)       { std::cout << paddName_ << " Text: " << message << "\n"; },
        [&](auto& message)       { std::cout << paddName_ << " auto\n"; }
    }, messageVariant);
  }

};