#pragma once
#include <ScratchPadd/Base.hpp>
#include <spdlog/spdlog.h>

class TestSendPadd : public ScratchPadd::Base {
public:
  virtual void prepare() override {
    spdlog::info("Preparing: {}", name());
    setRepeatInterval(1000);
  }
  TestSendPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  virtual void repeat() override { spdlog::info("Repeating: {}", name()); }

  virtual ~TestSendPadd() { spdlog::info("Destroying: {}", name()); }

  virtual void starting() override {
    ScratchPadd::MessageType::Triangle triangle;
    send(ScratchPadd::Make_Msg(ScratchPadd::MessageType::Triangle()));
  }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::TestResponse &message) {
                     std::cout << name() << "TestResponse: " << message << "\n";
                   },
                   [&](auto &message) {
                     std::cout << name() << "Unhandled message\n";
                   }},
               messageVariant);
  }
};