// OutlineHull.hpp - the vertices for a mesh's outline (Material::outline,
// on the backends that draw it as an inverted hull: desktops and the 3DS).
//
// The hull is the mesh drawn again, pushed out along its normals, its front
// faces culled, so only a rim shows around the mesh. A flat-shaded mesh has
// a vertex for every face at a corner, each with its face's normal, so
// pushed out along those the faces would come apart at the edges (a box's
// sides would leave gaps at its corners). So the hull's normals are shared:
// every vertex at the same place gets the average of their normals, and the
// hull stays closed.
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>

#include <span>
#include <vector>

namespace ScratchPadd::Scene {

// `vertices` with each normal the (normalised) average of the normals of
// every vertex at exactly the same position; the same order, so the mesh's
// indices still apply
std::vector<Vertex> outlineHullVertices(std::span<const Vertex> vertices);

} // namespace ScratchPadd::Scene
