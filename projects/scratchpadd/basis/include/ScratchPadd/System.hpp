#pragma once

#include <ScratchPadd/Messages/Message.hpp>
namespace ScratchPadd {
class Base;
class System {
public:
  System() {}
  // Virtual, so a system deleted through this interface is destroyed in full
  virtual ~System() = default;
  virtual void instantiate() = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void send(Base *sender, Message message) = 0;
  virtual void sendIncludeSender(Message message) = 0;
  virtual bool isRunning() = 0;
};
} // namespace ScratchPadd