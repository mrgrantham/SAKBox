#pragma once

// Messages passed between padds.
//
// Any copyable struct can be a message: padds register a handler for it with
// Base::receive<T>() and send it with Base::send(). The library itself only defines
// the generic Text message below and the control messages in Controls.hpp;
// applications define their own types without touching ScratchPadd.

#include <ScratchPadd/Messages/Controls.hpp>
#include <ScratchPadd/TypeName.hpp>
#include <cstdlib>
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
  // The contents' type name, for logging. Worked out once per type.
  virtual const std::string &typeName() const = 0;
  // The contents' size and whether they're trivially copyable, for the
  // message statistics (which types would suit a stream)
  virtual std::size_t size() const = 0;
  virtual bool triviallyCopyable() const = 0;
};

// A message's contents made by `make` (a function returning them), where
// the message keeps them: never copied onto the sender's stack. For big
// contents on a thread with a small stack (a DS's: a game state's copies
// there overflowed it). C++17 builds a returned value in place.
template <typename Make> struct MadeBy {
  Make make;
};

// A message holding a T.
template <typename T> class MessageOf : public MessageBase {
public:
  explicit MessageOf(T contents) : value(std::move(contents)) {}
  template <typename Make> explicit MessageOf(MadeBy<Make> by) : value(by.make()) {}
  std::type_index type() const override { return typeid(T); }
  // (Base::send logs this for every message, so it's demangled once, not
  // per call; the arguments of a debug log are built even when debug
  // logging is off.)
  const std::string &typeName() const override {
    static const std::string kName = ScratchPadd::typeName<T>();
    return kName;
  }
  std::size_t size() const override { return sizeof(T); }
  bool triviallyCopyable() const override { return std::is_trivially_copyable_v<T>; }

  const T value;
};

// One immutable message, shared by every receiver.
using Message = std::shared_ptr<const MessageBase>;

// Wraps `contents` in a Message.
template <typename T> Message makeMessage(T contents) {
  return std::make_shared<const MessageOf<std::decay_t<T>>>(std::move(contents));
}

// A message whose T is made by `make` in the message itself (MadeBy)
template <typename T, typename Make> Message makeMessageBy(Make make) {
  return std::make_shared<const MessageOf<T>>(MadeBy<Make>{std::move(make)});
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
