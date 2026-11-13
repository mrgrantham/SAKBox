// Culling.hpp - frustum culling, shared by every backend: a mesh's bounding
// sphere, worked out when it's loaded, and whether a draw's sphere is wholly
// outside what the camera sees, so it's skipped (FrameCounts::culled). It
// errs towards drawing: a mesh even partly in view is drawn. (Back faces
// are culled by each GPU, by the mesh's or the material's doubleSided.)
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <array>

namespace ScratchPadd::Scene {

// A sphere holding every vertex: about the middle of the mesh's box
struct Bounds {
  Vector3 centre;
  Scalar radius = 0;
};
Bounds boundsOf(const MeshData &mesh);

// What the camera sees: six planes, each facing in (near, far, left,
// right, top, bottom)
class Frustum {
public:
  // The camera's view at `aspect` (width / height), its sides moved out by
  // `widen` (1: as seen; the 3DS's stereo eyes each see a little to one
  // side, so it widens them)
  Frustum(const Camera &camera, Scalar aspect, Scalar widen = 1);

  // Whether `bounds`, placed by `where`, is wholly outside (the sphere's
  // radius times the transform's largest scale)
  bool outside(const Bounds &bounds, const Transform &where) const;

private:
  struct Plane {
    Vector3 normal; // length 1, facing in
    Scalar offset;  // normal . point on the plane
  };
  std::array<Plane, 6> planes_;
};

} // namespace ScratchPadd::Scene
