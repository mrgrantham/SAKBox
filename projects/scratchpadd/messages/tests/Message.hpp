#pragma once

#include <array>
#include <iostream>

// This is the namespace for all data structures passed between padds
#define MessageTypes                                                           \
  ScratchPadd::MessageType::Point, ScratchPadd::MessageType::Triangle,         \
      ScratchPadd::MessageType::Text

namespace ScratchPadd {
namespace MessageType {

namespace Transform {
struct Translate {};
struct Scale {};
struct Rotate {};
} // namespace Transform

struct Color {};
struct Point {
  double x{0};
  double y{0};
  friend std::ostream &operator<<(std::ostream &output, const Point &p) {
    output << "{ X:" << p.x << " Y: " << p.y << " } ";
    return output;
  }
};

struct Position : Point {};

struct Triangle {
  Position position;
  std::array<Point, 3> vertexes;
  friend std::ostream &operator<<(std::ostream &output, const Triangle &T) {
    output << " { position:" << T.position << ", vertexes: {\n";
    for (const Point &vertex : T.vertexes) {
      output << " " << vertex << " ,\n";
    }
    output << " }";
    return output;
  }
};

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

template <class MessageType>
static Message Make_Msg(MessageType &&MessageType) {
  return std::make_shared<MessageVariant>(MessageType);
}
} // namespace ScratchPadd