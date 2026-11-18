// OverlayText.hpp - the overlay's text on every platform: stb_easy_font's
// letters, built from small rectangles, so a backend draws them as it draws
// the overlay's shapes (desktops, the 3DS), or fills them into a bitmap (the
// DS's text layer; round 96: one font on every console), with no font
// texture. For backends.
#pragma once

#include <ScratchPadd/Scene/Overlay.hpp>

#include <vector>

namespace ScratchPadd::Scene {

// One of a text's rectangles, in points, placed and scaled
struct TextRectangle {
  float x, y, width, height;
};
// `text`'s letters as rectangles ('\n' starting a new line)
std::vector<TextRectangle> textRectangles(const Overlay::Text &text);

// Adds `text`'s triangles to `triangles`
void appendTextTriangles(const Overlay::Text &text, std::vector<Overlay::Vertex> &triangles);

} // namespace ScratchPadd::Scene
