#pragma once
#include <ScratchPadd/Base.hpp>
#include <spdlog/spdlog.h>

class TestSendPadd : public ScratchPadd::Base {
  public:
  virtual void prepare() override {
    spdlog::info("Preparing: {}",__CLASS_NAME__);
    paddName_ = __CLASS_NAME__;
    setRepeatInterval(1000);
  }
  TestSendPadd(ScratchPadd::System *system)  : Base(system){
    spdlog::info("Constructing: {}", __CLASS_NAME__);
  }

  virtual void repeat() override {
    spdlog::info("Repeating: {}",__CLASS_NAME__);
  }

  virtual ~TestSendPadd() {
    spdlog::info("Destroying: {}", __CLASS_NAME__ );
  }

  virtual void starting() override {
    ScratchPadd::MessageType::Triangle triangle;
    send(ScratchPadd::Make_Msg(ScratchPadd::MessageType::Triangle()));
  }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
        [&](ScratchPadd::MessageType::TestResponse& message)       { std::cout << paddName_ << "TestResponse: " << message <<"\n"; },
        [&](auto& message)   { std::cout << paddName_ << "Unhandled message\n"; }
    }, messageVariant);
  }
};