// TextureTiles.hpp - where an image's pixels go in a 3DS texture: the
// order the GPU reads them in. Kept apart from the renderer (no citro3d),
// so the order is tested on desktops.
//
// The GPU stores a texture in 8 x 8 tiles, each tile's pixels in Morton
// order, and counts its rows from the bottom: its texture coordinate t is
// 0 at the last row in memory and 1 at the first. Scene's images and
// texture coordinates run from the top (v 0 is an image's first row), so
// the image's rows go in last row first: then t is v, and every textured
// mesh (and the overlay's font) is the right way up with no change to its
// texture coordinates.
#pragma once

#include <cstddef>

namespace ScratchPadd::Scene {

// Where pixel (x, y) of a width x height image (y from its top) goes in
// the GPU's order, in pixels
constexpr std::size_t tiledIndex(int x, int y, int width, int height) {
  const int row = height - 1 - y; // (the GPU's rows count from the bottom)
  const int tile = (row / 8) * (width / 8) + x / 8;
  const int tx = x % 8, ty = row % 8;
  const int inTile = (tx & 1) | ((ty & 1) << 1) | ((tx & 2) << 1) | ((ty & 2) << 2) |
                     ((tx & 4) << 2) | ((ty & 4) << 3);
  return static_cast<std::size_t>(tile) * 64 + static_cast<std::size_t>(inTile);
}

} // namespace ScratchPadd::Scene
