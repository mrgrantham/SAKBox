// DisplayList.hpp - meshes as the DS's 3D hardware draws them: display
// lists of packed geometry commands (GBATEK, "DS 3D Geometry Commands"),
// sent to the geometry engine by glCallList. Plain C++ without libnds, so
// it's tested on desktops too.
//
// A mesh is prepared once when loaded (prepare: its quads, strips and
// triangles, their corners in the DS's own numbers), then packed for each
// way it's drawn (pack: lit or unlit, the material's colour and the ambient
// light, as the DS can't multiply those in while drawing).
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>

#include <cstdint>
#include <vector>

namespace ScratchPadd::Scene::DisplayList {

// The geometry commands used (GBATEK's numbers)
enum Command : std::uint8_t {
  kColour = 0x20,           // a vertex colour, RGB15
  kNormal = 0x21,           // a normal (1.9 x 3), lit with the polygon's lights
  kVertex16 = 0x23,         // a vertex, x y z in 4.12 (two words)
  kDiffuseAmbient = 0x30,   // the material's diffuse and ambient colours
  kSpecularEmission = 0x31, // its specular and emission colours
  kBegin = 0x40,            // starts polygons: 0 separate triangles, 1 quads,
                            // 2 a triangle strip
};

// The DS's 15-bit colour (5 bits a channel)
constexpr std::uint16_t rgb15(Colour c) {
  return static_cast<std::uint16_t>((c.r >> 3) | ((c.g >> 3) << 5) | ((c.b >> 3) << 10));
}
// `a` times `b`, a channel at a time (255 is 1)
constexpr Colour multiply(Colour a, Colour b) {
  return {static_cast<std::uint8_t>(a.r * b.r / 255), static_cast<std::uint8_t>(a.g * b.g / 255),
          static_cast<std::uint8_t>(a.b * b.b / 255), static_cast<std::uint8_t>(a.a * b.a / 255)};
}

// A Scalar as a whole number of 2^-`fractionBits` (rounded): the DS's 4.12
// vertices and 20.12 matrices are 12, its 1.9 normals 9
std::int32_t toFixed(Scalar value, int fractionBits);

// A unit vector as the DS's packed normal (or light direction): x, y and z
// 10 bits each, 1.9 (just under 1 at most)
std::uint32_t packNormal(const Vector3 &unit);

// A mesh in the DS's numbers: its runs (the mesh's own, or
// buildPrimitives'), each its corners in order
struct Source {
  struct Corner {
    std::int16_t x, y, z; // 4.12, after dividing by 2^scaleShift
    std::uint32_t normal; // packNormal's
    Colour colour;
  };
  struct Run {
    Primitive::Kind kind;
    std::uint8_t alpha;         // its polygons' (0-31)
    std::uint32_t first, count; // its corners
  };
  std::vector<Corner> corners;
  std::vector<Run> runs;
  // The DS's vertices only reach +-8 (4.12), so a bigger mesh is stored
  // 2^scaleShift times smaller, and drawn scaled back up
  int scaleShift = 0;
};
Source prepare(const MeshData &mesh);

// How a mesh is drawn, as far as its display list goes
struct Variant {
  bool lit = true;
  Colour colour;  // the material's: it multiplies the vertex colours
  Colour ambient; // the environment's (lit only)
  bool operator==(const Variant &) const = default;
};

// A display list for glCallList (its first word is how many follow), and
// the alpha (0-31) of the polygons in it: the DS sets a polygon's alpha
// before its list is called, so a list holds one alpha's runs
struct Packed {
  std::uint8_t alpha;
  std::vector<std::uint32_t> words;
};

// The mesh's display lists, one for each alpha its runs have, the most
// opaque first. Each run starts its kind of polygon. Unlit, each corner's
// colour times the material's. Lit (as desktops light: colour x (ambient +
// the lights)), each colour change sets the material's diffuse colour to it
// and its emission to it times the ambient light, and each normal is lit by
// the polygon's lights. A colour or normal is only sent when it changes.
std::vector<Packed> pack(const Source &source, const Variant &variant);

} // namespace ScratchPadd::Scene::DisplayList
