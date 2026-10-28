// MeshData.hpp - a mesh as a renderer loads it, and a builder for meshes
// made in code. (Meshes from Blender will come cooked by the build; see
// DESIGN.md.)
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace ScratchPadd::Scene {

struct Vertex {
  Vector3 position, normal;
  Colour colour;
};

// Indexed triangles: three indices into `vertices` each. A view: whoever
// made it keeps the data (a MeshBuilder, or the build's cooked arrays).
struct MeshData {
  std::span<const Vertex> vertices;
  std::span<const std::uint32_t> indices;
};

// Builds a mesh in code from flat-shaded triangles and quads: each face
// gets its own vertices, with the face's normal, so edges stay sharp.
class MeshBuilder {
public:
  // A triangle, counter-clockwise seen from the front
  MeshBuilder &triangle(const Vector3 &a, const Vector3 &b, const Vector3 &c, Colour colour = {});
  // A quad, a b c d counter-clockwise seen from the front (as two triangles)
  MeshBuilder &quad(const Vector3 &a, const Vector3 &b, const Vector3 &c, const Vector3 &d,
                    Colour colour = {});
  // A box centred on `centre`, `halfSize` from it along each axis
  MeshBuilder &box(const Vector3 &centre, const Vector3 &halfSize, Colour colour = {});

  MeshData data() const { return {vertices_, indices_}; }
  std::size_t triangleCount() const { return indices_.size() / 3; }

private:
  std::vector<Vertex> vertices_;
  std::vector<std::uint32_t> indices_;
};

} // namespace ScratchPadd::Scene
