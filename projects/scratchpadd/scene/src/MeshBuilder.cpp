#include <ScratchPadd/Scene/MeshData.hpp>

namespace ScratchPadd::Scene {

MeshBuilder &MeshBuilder::triangle(const Vector3 &a, const Vector3 &b, const Vector3 &c,
                                   Colour colour) {
  const Vector3 normal = (b - a).cross(c - a).normalised();
  const auto first = static_cast<std::uint32_t>(vertices_.size());
  for (const Vector3 &corner : {a, b, c}) {
    vertices_.push_back({corner, normal, colour});
  }
  indices_.insert(indices_.end(), {first, first + 1, first + 2});
  return *this;
}

MeshBuilder &MeshBuilder::quad(const Vector3 &a, const Vector3 &b, const Vector3 &c,
                               const Vector3 &d, Colour colour) {
  const Vector3 normal = (b - a).cross(c - a).normalised();
  const auto first = static_cast<std::uint32_t>(vertices_.size());
  for (const Vector3 &corner : {a, b, c, d}) {
    vertices_.push_back({corner, normal, colour});
  }
  indices_.insert(indices_.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
  return *this;
}

MeshBuilder &MeshBuilder::box(const Vector3 &centre, const Vector3 &halfSize, Colour colour) {
  // The eight corners: bit 0 is +x, bit 1 +y, bit 2 +z
  Vector3 p[8];
  for (int i = 0; i < 8; ++i) {
    p[i] = centre + Vector3{(i & 1) ? halfSize.x : -halfSize.x, (i & 2) ? halfSize.y : -halfSize.y,
                            (i & 4) ? halfSize.z : -halfSize.z};
  }
  quad(p[4], p[5], p[7], p[6], colour); // +z
  quad(p[1], p[0], p[2], p[3], colour); // -z
  quad(p[5], p[1], p[3], p[7], colour); // +x
  quad(p[0], p[4], p[6], p[2], colour); // -x
  quad(p[6], p[7], p[3], p[2], colour); // +y
  quad(p[0], p[1], p[5], p[4], colour); // -y
  return *this;
}

} // namespace ScratchPadd::Scene
