#include <ScratchPadd/Scene/Overlay.hpp>

#include <cmath>

namespace ScratchPadd::Scene {

void Overlay::rectangle(float x, float y, float width, float height, Colour colour) {
  const Vertex a{x, y, colour}, b{x + width, y, colour}, c{x + width, y + height, colour},
      d{x, y + height, colour};
  triangles_.insert(triangles_.end(), {a, b, c, a, c, d});
}

void Overlay::triangle(float x0, float y0, float x1, float y1, float x2, float y2, Colour colour) {
  triangles_.insert(triangles_.end(),
                    {Vertex{x0, y0, colour}, Vertex{x1, y1, colour}, Vertex{x2, y2, colour}});
}

void Overlay::hexagon(float x, float y, float radius, Colour colour, bool pointyTop) {
  // Six triangles from the middle, a corner every 60 degrees from 0 (or 30)
  constexpr float kSixth = 3.14159265f / 3;
  const float start = pointyTop ? kSixth / 2 : 0;
  for (int i = 0; i < 6; ++i) {
    const float a = start + kSixth * static_cast<float>(i);
    const float b = a + kSixth;
    triangle(x, y, x + radius * std::cos(a), y + radius * std::sin(a), x + radius * std::cos(b),
             y + radius * std::sin(b), colour);
  }
}

void Overlay::line(float x0, float y0, float x1, float y1, Colour colour, float thickness) {
  const float dx = x1 - x0, dy = y1 - y0, length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0)
    return;
  // Half the thickness to each side, across the line
  const float sideX = -dy / length * thickness / 2, sideY = dx / length * thickness / 2;
  const Vertex a{x0 + sideX, y0 + sideY, colour}, b{x1 + sideX, y1 + sideY, colour},
      c{x1 - sideX, y1 - sideY, colour}, d{x0 - sideX, y0 - sideY, colour};
  triangles_.insert(triangles_.end(), {a, b, c, a, c, d});
}

} // namespace ScratchPadd::Scene
