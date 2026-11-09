// Primitives.hpp - a mesh's triangles as the DS draws them most cheaply:
// pairs joined into quads, the rest chained into strips (MeshData's
// Primitive runs). The build's cook runs it on each model's DS version, to
// count what it costs and to cook the runs in; the DS's backend runs it on
// a mesh loaded without them (one made in code). Desktops draw triangles,
// so they turn the runs back into a list (triangleList).
//
// What the DS counts (GBATEK, "DS 3D Polygon Definitions by Vertices"): a
// polygon each triangle or quad, and in its vertex RAM, 3 vertices a
// triangle, 4 a quad, and a strip's vertices once each (2 more than its
// triangles). Polygons are usually what a model runs out of first: a quad
// is one where two triangles were two.
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ScratchPadd::Scene {

// A mesh's indices and the runs they make (for MeshData)
struct Primitives {
  std::vector<std::uint32_t> indices;
  std::vector<Primitive> runs;
};

// `mesh`'s triangles as quads and strips:
// - Two triangles sharing an edge (the same two vertices: the same
//   position, normal and colour) become a quad when they're in one plane,
//   the quad is convex, and both have the same alpha. Each triangle is
//   joined with its first such neighbour, in the mesh's order.
// - The others are chained into strips, each from the first triangle left,
//   along whichever of its edges makes the longest, through triangles with
//   its alpha. A strip of one is left a separate triangle.
// The runs go by alpha, the most opaque first; within one, the quads, then
// the strips, then the separate triangles. The same mesh always gives the
// same runs.
Primitives buildPrimitives(const MeshData &mesh);

// A triangle's alpha (0-31) as the DS draws it, one for the whole polygon:
// the average of its corners'
std::uint8_t polygonAlpha(Colour a, Colour b, Colour c);

// What `runs` cost the DS: see the top of this file
FrameCounts costOf(std::span<const Primitive> runs);
// What `mesh` costs the DS: its runs', or if it has none, buildPrimitives'
FrameCounts costOf(const MeshData &mesh);

// `mesh`'s indices as separate triangles, three each, as desktops draw
// them: its runs undone (strips' every other triangle turned back), or its
// indices as they are if it has none
std::vector<std::uint32_t> triangleList(const MeshData &mesh);

// Whether any of `mesh`'s vertices is see-through (alpha below 255): such
// a mesh is drawn translucent, after the solid ones, whatever its material
bool hasTranslucentVertices(const MeshData &mesh);

} // namespace ScratchPadd::Scene
