// The overlay's text on desktops: stb_easy_font's letters, built from small
// rectangles, so it needs no font texture
#include "Backend.hpp"

#include <stb/stb_easy_font.h>

#include <string>

namespace ScratchPadd::Scene {

namespace {
// One of stb_easy_font's vertices: x, y, z and a colour, 16 bytes
struct EasyFontVertex {
  float x, y, z;
  unsigned char colour[4];
};
// stb_easy_font's line height, in points
constexpr float kLineHeight = 12;
} // namespace

void appendTextTriangles(const Overlay::Text &text, std::vector<Overlay::Vertex> &triangles) {
  // stb_easy_font wants a writable, terminated string, and room for about
  // 270 bytes of vertices a character
  std::string copy(text.text);
  std::vector<EasyFontVertex> quads(copy.size() * 270 / sizeof(EasyFontVertex) + 4);
  unsigned char rgba[4] = {text.colour.r, text.colour.g, text.colour.b, text.colour.a};
  const int count = stb_easy_font_print(0, 0, copy.data(), rgba, quads.data(),
                                        static_cast<int>(quads.size() * sizeof(EasyFontVertex)));
  for (int q = 0; q < count; ++q) {
    const EasyFontVertex *corner = &quads[static_cast<size_t>(q) * 4];
    auto at = [&](int i) {
      return Overlay::Vertex{text.x + corner[i].x * text.scale, text.y + corner[i].y * text.scale,
                             text.colour};
    };
    triangles.insert(triangles.end(), {at(0), at(1), at(2), at(0), at(2), at(3)});
  }
}

float Overlay::textWidth(std::string_view text, float scale) {
  std::string copy(text);
  return static_cast<float>(stb_easy_font_width(copy.data())) * scale;
}

float Overlay::lineHeight(float scale) { return kLineHeight * scale; }

} // namespace ScratchPadd::Scene
