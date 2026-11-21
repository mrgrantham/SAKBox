// OverlayText.hpp - the overlay as backends draw it, in the platform's font
// (Font.hpp: Silkscreen on the DS, Orbitron elsewhere). For backends.
//
// - The DS fills its text layer from the font's pixels itself
//   (Font::pixelRectangles, in whole numbers).
// - The GPU backends (desktops, the 3DS) draw the shapes and the letters in
//   one pass of textured triangles: the letters from the font's atlas, the
//   shapes from its solid corner, each vertex's colour times the atlas's
//   alpha.
#pragma once

#include <ScratchPadd/Scene/Font.hpp>
#include <ScratchPadd/Scene/Overlay.hpp>

#include <vector>

namespace ScratchPadd::Scene {

// A corner of the overlay's triangles as the GPU backends draw them:
// points, colour and texture coordinates in the platform font's atlas (0 to
// 1, v from the top)
struct TexturedVertex {
  float x, y;
  Colour colour;
  float u, v;
};

// The overlay's shapes, then its text over them, as triangles into `out`
// (cleared first; kept by the backend, so no allocation once it's grown)
void overlayVertices(const Overlay &overlay, std::vector<TexturedVertex> &out);

// The platform font's atlas, which overlayVertices' texture coordinates
// are in, as white pixels with its alpha (for a backend whose textures are
// RGBA)
std::vector<Colour> atlasPixels(const FontAtlas &atlas);

} // namespace ScratchPadd::Scene
