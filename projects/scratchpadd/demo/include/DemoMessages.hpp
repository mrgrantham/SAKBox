#pragma once
// Messages defined by the demo app rather than the library: any copyable
// struct can be sent between padds (see Base::on and Base::send).

#include <array>
#include <iostream>

namespace Demo {

struct Point {
  double x{0};
  double y{0};
  friend std::ostream &operator<<(std::ostream &output, const Point &p) {
    output << "{ X:" << p.x << " Y: " << p.y << " } ";
    return output;
  }
};

struct Triangle {
  Point position;
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

} // namespace Demo
