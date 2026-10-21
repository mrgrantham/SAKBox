#pragma once

#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/PaddAddress.hpp>

#include <string_view>
#include <typeindex>

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
  // Queues `message` for every padd that handles its type, except `sender`
  virtual void send(Base *sender, Message message) = 0;
  // Queues `message` for every padd that handles its type, `sender` (which
  // may be null, for a message from outside any padd) included
  virtual void sendIncludeSender(Base *sender, Message message) = 0;
  // The padd of class `type`, or an empty address if there isn't one. Works
  // once instantiate() has built the padds.
  virtual PaddAddress paddOfType(std::type_index type) = 0;
  // The padd named `name` (its Base::paddName()), or an empty address
  virtual PaddAddress paddNamed(std::string_view name) = 0;
  virtual bool isRunning() = 0;
};
} // namespace ScratchPadd
