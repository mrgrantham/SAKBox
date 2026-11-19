#include <ScratchPadd/Scene/OverlayText.hpp>

#include <stb/stb_easy_font.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace ScratchPadd::Scene {

namespace {
// stb_easy_font's line height, in points
constexpr float kLineHeight = 12;
} // namespace

void fontRectangles(std::string_view text, std::vector<FontRectangle> &out) {
  // stb_easy_font_print's walk through its tables, in whole numbers (its
  // own is in floats, into a vertex buffer): each character's horizontal
  // then vertical strokes, a byte each (length in the low 3 bits, a step
  // right in bit 3, the row in the top 4), then its advance
  out.clear();
  int x = 0, y = 0;
  for (const char ch : text) {
    if (ch == '\n') {
      y += static_cast<int>(kLineHeight);
      x = 0;
      continue;
    }
    if (ch < ' ' || ch > '~')
      continue;
    const auto c = static_cast<std::size_t>(ch - 32);
    const unsigned advance = stb_easy_font_charinfo[c].advance;
    const int top = (advance & 16) != 0 ? y + 1 : y;
    const auto strokes = [&](const unsigned char *segs, int count, bool vertical) {
      int at = x;
      for (int i = 0; i < count; ++i) {
        const int length = segs[i] & 7;
        at += (segs[i] >> 3) & 1;
        if (length == 0)
          continue;
        const int row = top + (segs[i] >> 4);
        out.push_back({static_cast<std::int16_t>(at), static_cast<std::int16_t>(row),
                       static_cast<std::int16_t>(vertical ? 1 : length),
                       static_cast<std::int16_t>(vertical ? length : 1)});
      }
    };
    const int hFrom = stb_easy_font_charinfo[c].h_seg, vFrom = stb_easy_font_charinfo[c].v_seg;
    strokes(&stb_easy_font_hseg[hFrom], stb_easy_font_charinfo[c + 1].h_seg - hFrom, false);
    strokes(&stb_easy_font_vseg[vFrom], stb_easy_font_charinfo[c + 1].v_seg - vFrom, true);
    x += static_cast<int>(advance & 15);
  }
}

std::vector<TextRectangle> textRectangles(const Overlay::Text &text) {
  std::vector<FontRectangle> strokes;
  fontRectangles(text.text, strokes);
  std::vector<TextRectangle> out;
  out.reserve(strokes.size());
  for (const FontRectangle &r : strokes)
    out.push_back({text.x + static_cast<float>(r.x) * text.scale,
                   text.y + static_cast<float>(r.y) * text.scale,
                   static_cast<float>(r.width) * text.scale,
                   static_cast<float>(r.height) * text.scale});
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

Overlay::Ink Overlay::inkOf(std::string_view text, float scale) {
  const std::vector<TextRectangle> rectangles =
      textRectangles({0, 0, std::string(text), {}, scale});
  if (rectangles.empty())
    return {};
  float left = rectangles[0].x, top = rectangles[0].y, right = left, bottom = top;
  for (const TextRectangle &r : rectangles) {
    left = std::min(left, r.x);
    top = std::min(top, r.y);
    right = std::max(right, r.x + r.width);
    bottom = std::max(bottom, r.y + r.height);
  }
  return {left, top, right - left, bottom - top};
}

} // namespace ScratchPadd::Scene
