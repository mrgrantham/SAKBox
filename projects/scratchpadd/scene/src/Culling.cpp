#include <ScratchPadd/Scene/Culling.hpp>

namespace ScratchPadd::Scene {

namespace {

Scalar largest(Scalar a, Scalar b) { return a > b ? a : b; }
Scalar smallest(Scalar a, Scalar b) { return a < b ? a : b; }

} // namespace

Bounds boundsOf(const MeshData &mesh) {
  if (mesh.vertices.empty())
    return {};
  Vector3 low = mesh.vertices[0].position, high = low;
  for (const Vertex &v : mesh.vertices) {
    low = {smallest(low.x, v.position.x), smallest(low.y, v.position.y),
           smallest(low.z, v.position.z)};
    high = {largest(high.x, v.position.x), largest(high.y, v.position.y),
            largest(high.z, v.position.z)};
  }
  Bounds bounds{(low + high) * Scalar(0.5f), 0};
  for (const Vertex &v : mesh.vertices)
    bounds.radius = largest(bounds.radius, (v.position - bounds.centre).length());
  return bounds;
}

Frustum::Frustum(const Camera &camera, Scalar aspect, Scalar widen) {
  const Vector3 forward = (camera.target - camera.eye).normalised();
  const Vector3 right = forward.cross(camera.up).normalised();
  const Vector3 up = right.cross(forward);
  // Half the view's height and width, a unit ahead (tan of half the angle)
  const Scalar half = camera.fieldOfView * Scalar(0.0087266463f); // (degrees / 2, in radians)
  const Scalar tall = sine(half) / cosine(half), wide = tall * aspect * widen;
  const auto through = [&](const Vector3 &normal, const Vector3 &point) {
    const Vector3 n = normal.normalised();
    return Plane{n, n.dot(point)};
  };
  planes_ = {through(forward, camera.eye + forward * camera.nearPlane),
             through(-forward, camera.eye + forward * camera.farPlane),
             // (each side's normal is square to the edge of the view there)
             through(forward * wide + right, camera.eye),
             through(forward * wide - right, camera.eye), through(forward * tall - up, camera.eye),
             through(forward * tall + up, camera.eye)};
}

bool Frustum::outside(const Bounds &bounds, const Transform &where) const {
  const Vector3 centre = where.apply(bounds.centre);
  // The transform's largest scale: its longest column
  Scalar scale = 0;
  for (int column = 0; column < 3; ++column)
    scale = largest(scale,
                    Vector3{where.m[0][column], where.m[1][column], where.m[2][column]}.length());
  const Scalar radius = bounds.radius * scale;
  for (const Plane &plane : planes_)
    if (plane.normal.dot(centre) - plane.offset < -radius)
      return true;
  return false;
}

} // namespace ScratchPadd::Scene
