// ToonTable.hpp - the DS's toon table for toon shading (Material::Shading::
// Toon), as its hardware uses it (GBATEK, "DS 3D Texture Blending").
//
// A toon polygon is lit as usual, and the red part of its lit colour (0-31)
// picks one of the table's 32 colours. The table is one for the whole frame
// (so the DS uses Environment::toonBands for every toon mesh, not a
// material's own), and how it's used is too (Environment::dsToonTable).
// Without a texture (ours have none) the pixel is the table's colour, not
// the mesh's: in Shades as it is, in Highlights added to itself (GBATEK's
// formulas with a white texture; seen in melonDS). So toon meshes are drawn
// lit as white (the red part is then how much light they get) in the
// table's greys: a few bands, dark to light, every toon mesh alike.
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
