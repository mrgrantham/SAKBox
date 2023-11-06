#pragma once

namespace ScratchPadd::MessageType {

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

} // namespace ScratchPadd::MessageType