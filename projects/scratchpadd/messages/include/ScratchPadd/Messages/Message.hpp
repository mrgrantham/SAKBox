#pragma once

// Messages passed between padds.
//
// Any copyable struct can be a message: padds register a handler for it with
// Base::on<T>() and send it with Base::send(). The library itself only defines
// the generic Text message below and the control messages in Controls.hpp;
// applications define their own types without touching ScratchPadd.

#include <ScratchPadd/Messages/Controls.hpp>
#include <cstdlib>
#include <cxxabi.h>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

namespace ScratchPadd {

// The type-erased base of every message. A message is created once by
// makeMessage() and then shared, read-only, by every padd that receives it.
class MessageBase {
public:
  virtual ~MessageBase() = default;
  // The type of the contents, used to find a receiver's handler.
  virtual std::type_index type() const = 0;
  // The contents' type name, for logging.
  virtual std::string typeName() const = 0;
};

// A message holding a T.
template <typename T> class MessageOf : public MessageBase {
public:
  explicit MessageOf(T contents) : value(std::move(contents)) {}
  std::type_index type() const override { return typeid(T); }
  std::string typeName() const override {
    int status = 0;
    char *demangled = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
    std::string name = status == 0 ? demangled : typeid(T).name();
    std::free(demangled);
    return name;
  }

  const T value;
};

// One immutable message, shared by every receiver.
using Message = std::shared_ptr<const MessageBase>;

// Wraps `contents` in a Message.
template <typename T> Message makeMessage(T contents) {
  return std::make_shared<const MessageOf<std::decay_t<T>>>(std::move(contents));
}

// The contents of `message` if it holds a T, or nullptr.
template <typename T> const T *messageAs(const Message &message) {
  if (!message || message->type() != std::type_index(typeid(T))) {
    return nullptr;
  }
  return &static_cast<const MessageOf<T> &>(*message).value;
}

namespace MessageType {

// A line of text: the library's one general-purpose message.
struct Text {
  std::string contents;
  friend std::ostream &operator<<(std::ostream &output, const Text &T) {
    output << " { text context:" << T.contents << " }";
    return output;
  }
};

} // namespace MessageType
} // namespace ScratchPadd
