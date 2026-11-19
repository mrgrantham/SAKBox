// OverlayText.hpp - the overlay's text on every platform: stb_easy_font's
// letters, built from small rectangles, so a backend draws them as it draws
// the overlay's shapes (desktops, the 3DS), or fills them into a bitmap (the
// DS's text layer; round 96: one font on every console), with no font
// texture. For backends.
#pragma once

#include <ScratchPadd/Scene/Overlay.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace ScratchPadd::Scene {

// One of a text's rectangles, in points, placed and scaled
struct TextRectangle {
  float x, y, width, height;
};
// `text`'s letters as rectangles ('\n' starting a new line)
std::vector<TextRectangle> textRectangles(const Overlay::Text &text);

// A letter's stroke in the font's own units, before it's scaled and placed:
// whole numbers, the text starting at (0, 0). For a backend without floating
// point hardware (the DS: round 105, its text took up to 40 ms a frame in
// soft floats), which places them in whole numbers itself
struct FontRectangle {
  std::int16_t x, y, width, height;
};
// `text`'s strokes into `out` (cleared first; kept by the caller, so no
// allocation once it's grown). Characters the font hasn't (outside ' ' to
// '~') are left out, taking no room
void fontRectangles(std::string_view text, std::vector<FontRectangle> &out);

// Adds `text`'s triangles to `triangles`
void appendTextTriangles(const Overlay::Text &text, std::vector<Overlay::Vertex> &triangles);

} // namespace ScratchPadd::Scene
