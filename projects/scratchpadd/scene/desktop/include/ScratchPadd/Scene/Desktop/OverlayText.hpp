// OverlayText.hpp - the overlay's text on desktops: stb_easy_font's
// letters, built from small rectangles, so a backend draws them as it draws
// the overlay's shapes, with no font texture. For backends.
#pragma once

#include <ScratchPadd/Scene/Overlay.hpp>

#include <vector>

namespace ScratchPadd::Scene {

// Adds `text`'s triangles to `triangles`
void appendTextTriangles(const Overlay::Text &text, std::vector<Overlay::Vertex> &triangles);

} // namespace ScratchPadd::Scene
