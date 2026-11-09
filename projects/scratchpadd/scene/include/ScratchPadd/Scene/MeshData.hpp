// MeshData.hpp - a mesh as a renderer loads it, and a builder for meshes
// made in code. (Meshes from Blender come cooked by the build: see
// tools/assets.)
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cstdint>
#include <span>
#include <vector>

// The DS's C library (newlib) has <sys/types.h> define `quad` as a macro
// (quad_t), which renames MeshBuilder::quad in any file that has seen it.
// Included here, it's renamed the same way in every file, so they agree.
#if __has_include(<sys/types.h>)
#include <sys/types.h>
#endif

namespace ScratchPadd::Scene {

struct Vertex {
  Vector3 position, normal;
  Colour colour;
};

// A run of a mesh's indices drawn as one kind of polygon, all with the
// same alpha: what the DS draws natively (see Primitives.hpp)
struct Primitive {
  enum class Kind : std::uint8_t {
    triangles,     // 3 indices a triangle
    quads,         // 4 a quad, around it (counter-clockwise from the front)
    triangleStrip, // 2 more than its triangles: the first is 0 1 2, then
                   // each new index makes one with the two before it (as
                   // OpenGL's GL_TRIANGLE_STRIP: every other one reversed)
  };
  Kind kind = Kind::triangles;
  std::uint8_t alpha = 31; // its polygons' (0-31): their corners' average
  std::uint32_t indexCount = 0;
  bool operator==(const Primitive &) const = default;
};

// A mesh: indices into `vertices`, three a triangle; or, with `primitives`,
// the runs they say, one after another (the build cooks the DS's version of
// a model that way). A view: whoever made it keeps the data (a MeshBuilder,
// or the build's cooked arrays).
struct MeshData {
  std::span<const Vertex> vertices;
  std::span<const std::uint32_t> indices;
  std::span<const Primitive> primitives;
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
