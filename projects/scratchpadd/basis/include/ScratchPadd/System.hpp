#pragma once

#if defined(SCRATCHPADD_TEST)
#pragma warning "Using ScratchPadd Test Messages"
#include <ScratchPadd/Tests/Message.hpp>
#else
#include <ScratchPadd/Messages/Message.hpp>
#endif
namespace ScratchPadd {
class Base;
class System {
public:
  System() {}
  virtual void instantiate() = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void send(Base *sender, Message message) = 0;
  virtual void sendIncludeSender(Message message) = 0;
  virtual bool isRunning() = 0;
};
} // namespace ScratchPadd