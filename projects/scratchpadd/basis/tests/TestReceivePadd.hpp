#pragma once
#include <ScratchPadd/Base.hpp>
#include <spdlog/spdlog.h>

class TestReceivePadd : public ScratchPadd::Base {
public:
  virtual void prepare() override {
    spdlog::info("Preparing: {}", name());
    setRepeatInterval(1000);
  }
  StoryPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  virtual void repeat() override { spdlog::info("Repeating: {}", name()); }

  virtual ~StoryPadd() { spdlog::info("Destroying: {}", name()); }

  virtual void starting() override {
    ScratchPadd::MessageType::Triangle triangle;
    send(ScratchPadd::Make_Msg(ScratchPadd::MessageType::Triangle()));
  }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(
        VariantHandler{[&](ScratchPadd::MessageType::Triangle &message) {
                         std::cout << name() << "Triangle: " << message << "\n";
                       },
                       [&](ScratchPadd::MessageType::Point &message) {
                         std::cout << name() << "Point: " << message << "\n";
                       },
                       [&](ScratchPadd::MessageType::Text &message) {
                         std::cout << name() << "Text: " << message << "\n";
                       }},
        messageVariant);
  }
};