#include <ScratchPadd/Scene/OverlayText.hpp>

#include <stb/stb_easy_font.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace ScratchPadd::Scene {

namespace {
// One of stb_easy_font's vertices: x, y, z and a colour, 16 bytes
struct EasyFontVertex {
  float x, y, z;
  unsigned char colour[4];
};
// stb_easy_font's line height, in points
constexpr float kLineHeight = 12;
// The most quads stb_easy_font makes a character (X's 11, counted from its
// tables): it stops at the end of its buffer, dropping the rest, so a short
// text of busy letters given its average (about 270 bytes, 4 quads) lost
// strokes ("Rock" needs 19 quads)
constexpr std::size_t kMostQuadsPerCharacter = 11;
} // namespace

std::vector<TextRectangle> textRectangles(const Overlay::Text &text) {
  // stb_easy_font wants a writable, terminated string, and room for every
  // quad it makes (4 vertices each: top left, top right, bottom right,
  // bottom left)
  std::string copy(text.text);
  std::vector<EasyFontVertex> quads(copy.size() * kMostQuadsPerCharacter * 4);
  const int count = stb_easy_font_print(0, 0, copy.data(), nullptr, quads.data(),
                                        static_cast<int>(quads.size() * sizeof(EasyFontVertex)));
  std::vector<TextRectangle> out;
  out.reserve(static_cast<std::size_t>(count));
  for (int q = 0; q < count; ++q) {
    const EasyFontVertex *corner = &quads[static_cast<size_t>(q) * 4];
    out.push_back({text.x + corner[0].x * text.scale, text.y + corner[0].y * text.scale,
                   (corner[2].x - corner[0].x) * text.scale,
                   (corner[2].y - corner[0].y) * text.scale});
  }
  return out;
}

void appendTextTriangles(const Overlay::Text &text, std::vector<Overlay::Vertex> &triangles) {
  for (const TextRectangle &r : textRectangles(text)) {
    const Overlay::Vertex a{r.x, r.y, text.colour}, b{r.x + r.width, r.y, text.colour},
        c{r.x + r.width, r.y + r.height, text.colour}, d{r.x, r.y + r.height, text.colour};
    triangles.insert(triangles.end(), {a, b, c, a, c, d});
  }
}

float Overlay::textWidth(std::string_view text, float scale) {
  std::string copy(text);
  return static_cast<float>(stb_easy_font_width(copy.data())) * scale;
}

float Overlay::lineHeight(float scale) { return kLineHeight * scale; }

} // namespace ScratchPadd::Scene
