#include <ScratchPadd/Scene/OutlineHull.hpp>

#include <algorithm>
#include <cstdint>
#include <numeric>

namespace ScratchPadd::Scene {

std::vector<Vertex> outlineHullVertices(std::span<const Vertex> vertices) {
  std::vector<Vertex> hull(vertices.begin(), vertices.end());
  // The vertices in order of position, so those at the same place are together
  std::vector<std::uint32_t> order(vertices.size());
  std::iota(order.begin(), order.end(), 0u);
  const auto before = [&](std::uint32_t a, std::uint32_t b) {
    const Vector3 &p = vertices[a].position, &q = vertices[b].position;
    if (p.x != q.x)
      return p.x < q.x;
    if (p.y != q.y)
      return p.y < q.y;
    return p.z < q.z;
  };
  std::sort(order.begin(), order.end(), before);
  // Each run of vertices at one place: their normals' sum, normalised, for all
  for (std::size_t start = 0; start < order.size();) {
    std::size_t end = start + 1;
    while (end < order.size() && vertices[order[end]].position == vertices[order[start]].position)
      ++end;
    Vector3 sum;
    for (std::size_t i = start; i < end; ++i)
      sum = sum + vertices[order[i]].normal;
    const Vector3 shared = sum.normalised();
    for (std::size_t i = start; i < end; ++i)
      hull[order[i]].normal = shared;
    start = end;
  }
  return hull;
}

} // namespace ScratchPadd::Scene
