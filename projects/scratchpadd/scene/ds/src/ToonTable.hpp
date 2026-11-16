// ToonTable.hpp - the DS's toon table for toon shading (Material::Shading::
// Toon), as its hardware uses it (GBATEK, "DS 3D Texture Blending").
//
// A toon polygon is lit as usual, and the red part of its lit colour (0-31)
// picks one of the table's 32 colours. The table is one for the whole frame
// (so the DS uses Environment::toonBands for every toon mesh, not a
// material's own), and how it's used is too (Environment::dsToonTable):
//
//   Shades      the pixel is the table's colour. The mesh is lit as white,
//               so the red part is how much light it gets, and the table
//               turns that into a few greys: every toon mesh drawn in them,
//               its own colours gone.
//   Highlights  the pixel is the lit colour plus the table's. The mesh keeps
//               its colours, with a band of extra light added where its
//               lit red part is high: so the bands follow its red, and a
//               mesh with little red gets few of them.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <array>
#include <cmath>
#include <cstdint>

namespace ScratchPadd::Scene::DisplayList {

// Shades: the darkest band's grey (0-31), so unlit sides aren't black
inline constexpr int kDarkestShade = 8;
// Highlights: what the brightest band adds (0-31)
inline constexpr int kBrightestHighlight = 12;

// The table's 32 colours as the DS's 15-bit ones
inline std::array<std::uint16_t, 32> toonTable(DsToonTable kind, int bands) {
  std::array<std::uint16_t, 32> table{};
  for (int red = 0; red < 32; ++red) {
    const float step = toonStep(static_cast<float>(red) / 31.0f, bands);
    const int grey = static_cast<int>(kind == DsToonTable::Shades
                                          ? kDarkestShade + std::lround(step * (31 - kDarkestShade))
                                          : std::lround(step * kBrightestHighlight));
    table[static_cast<std::size_t>(red)] =
        static_cast<std::uint16_t>(grey | (grey << 5) | (grey << 10));
  }
  return table;
}

} // namespace ScratchPadd::Scene::DisplayList
