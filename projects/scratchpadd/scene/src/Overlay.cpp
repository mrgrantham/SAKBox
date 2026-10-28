#include <ScratchPadd/Scene/Overlay.hpp>

#include <stb/stb_easy_font.h>

#include <cmath>
#include <string>

namespace ScratchPadd::Scene {

namespace {
// One of stb_easy_font's vertices: x, y, z and a colour, 16 bytes
struct EasyFontVertex {
  float x, y, z;
  unsigned char colour[4];
};
} // namespace

void Overlay::text(float x, float y, std::string_view text, Colour colour, float scale) {
  // stb_easy_font wants a writable, terminated string, and room for about
  // 270 bytes of vertices a character
  std::string copy(text);
  std::vector<EasyFontVertex> quads(copy.size() * 270 / sizeof(EasyFontVertex) + 4);
  unsigned char rgba[4] = {colour.r, colour.g, colour.b, colour.a};
  const int count = stb_easy_font_print(0, 0, copy.data(), rgba, quads.data(),
                                        static_cast<int>(quads.size() * sizeof(EasyFontVertex)));
  for (int q = 0; q < count; ++q) {
    const EasyFontVertex *corner = &quads[static_cast<size_t>(q) * 4];
    auto at = [&](int i) {
      return Vertex{x + corner[i].x * scale, y + corner[i].y * scale, colour};
    };
    triangles_.insert(triangles_.end(), {at(0), at(1), at(2), at(0), at(2), at(3)});
  }
}

float Overlay::textWidth(std::string_view text, float scale) {
  std::string copy(text);
  return static_cast<float>(stb_easy_font_width(copy.data())) * scale;
}

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
