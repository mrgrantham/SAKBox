#include <ScratchPadd/Scene/Overlay.hpp>

#include <cmath>

namespace ScratchPadd::Scene {

void Overlay::rectangle(float x, float y, float width, float height, Colour colour) {
  const Vertex a{x, y, colour}, b{x + width, y, colour}, c{x + width, y + height, colour},
      d{x, y + height, colour};
  triangles_.insert(triangles_.end(), {a, b, c, a, c, d});
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
