#pragma once

#include <ScratchPadd/Helper.hpp>
#include <ScratchPadd/Messages/Controls.hpp>
#include <ScratchPadd/Messages/Geometry.hpp>
#include <array>
#include <functional>
#include <iostream>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// This is the namespace for all data structures passed between padds
#define MessageTypes                                                           \
  ScratchPadd::MessageType::Point, ScratchPadd::MessageType::Triangle,         \
      ScratchPadd::MessageType::Text,                                          \
      ScratchPadd::MessageType::ControlSnapshot,                               \
      ScratchPadd::MessageType::ControlChange,                                 \
      ScratchPadd::MessageType::ControlRequest

namespace ScratchPadd {
namespace MessageType {

struct Text {
  std::string contents;
  friend std::ostream &operator<<(std::ostream &output, const Text &T) {
    output << " { text context:" << T.contents << " }";
    return output;
  }
};

} // namespace MessageType

using MessageVariant = std::variant<MessageTypes>;
using Message = std::shared_ptr<MessageVariant>;

template <class MessageType> static Message MakeMsg(MessageType &&messageType) {
  return std::make_shared<MessageVariant>(std::move(messageType));
}
} // namespace ScratchPadd