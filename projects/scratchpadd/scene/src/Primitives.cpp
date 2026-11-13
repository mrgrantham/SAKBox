#include <ScratchPadd/Scene/Primitives.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <unordered_map>

namespace ScratchPadd::Scene {

namespace {

// Two triangles' normals at least this close (cos 0.8 degrees) are in one
// plane: a quad made of them is drawn as they were
constexpr float kSamePlane = 0.9999f;

using Point = std::array<float, 3>;

Point operator-(const Point &a, const Point &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Point cross(const Point &a, const Point &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
float dot(const Point &a, const Point &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

struct Triangle {
  std::array<std::uint32_t, 3> corners;
  std::uint8_t alpha;
  Point normal; // unit, or 0 for a degenerate triangle
};

// A directed edge, from vertex `from` to `to`, as one key
std::uint64_t edge(std::uint32_t from, std::uint32_t to) {
  return (static_cast<std::uint64_t>(from) << 32) | to;
}

class Builder {
public:
  explicit Builder(const MeshData &mesh) {
    for (const Vertex &v : mesh.vertices)
      points_.push_back({static_cast<float>(v.position.x), static_cast<float>(v.position.y),
                         static_cast<float>(v.position.z)});
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
      Triangle t{{mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]}, 0, {}};
      t.alpha = polygonAlpha(mesh.vertices[t.corners[0]].colour, mesh.vertices[t.corners[1]].colour,
                             mesh.vertices[t.corners[2]].colour);
      const Point n = cross(points_[t.corners[1]] - points_[t.corners[0]],
                            points_[t.corners[2]] - points_[t.corners[0]]);
      const float length = std::sqrt(dot(n, n));
      if (length > 0)
        t.normal = {n[0] / length, n[1] / length, n[2] / length};
      const auto index = static_cast<std::uint32_t>(triangles_.size());
      for (int k = 0; k < 3; ++k)
        edges_.try_emplace(edge(t.corners[k], t.corners[(k + 1) % 3]), index);
      triangles_.push_back(t);
    }
    used_.assign(triangles_.size(), false);
    inStrip_.assign(triangles_.size(), 0);
  }

  Primitives build() {
    // Each alpha's runs, the most opaque first
    std::map<std::uint8_t, Group, std::greater<>> groups;
    for (std::uint32_t t = 0; t < triangles_.size(); ++t)
      if (!used_[t])
        joinQuad(t, groups[triangles_[t].alpha]);
    for (std::uint32_t t = 0; t < triangles_.size(); ++t)
      if (!used_[t])
        chainStrip(t, groups[triangles_[t].alpha]);

    Primitives out;
    auto add = [&](Primitive::Kind kind, std::uint8_t alpha, std::span<const std::uint32_t> run) {
      if (run.empty())
        return;
      out.indices.insert(out.indices.end(), run.begin(), run.end());
      out.runs.push_back({kind, alpha, static_cast<std::uint32_t>(run.size())});
    };
    for (const auto &[alpha, group] : groups) {
      add(Primitive::Kind::Quads, alpha, group.quads);
      for (const std::vector<std::uint32_t> &strip : group.strips)
        add(Primitive::Kind::TriangleStrip, alpha, strip);
      add(Primitive::Kind::Triangles, alpha, group.triangles);
    }
    return out;
  }

private:
  struct Group {
    std::vector<std::uint32_t> quads, triangles;
    std::vector<std::vector<std::uint32_t>> strips;
  };

  // The unused triangle on the other side of edge from -> to, if any
  std::optional<std::uint32_t> across(std::uint32_t from, std::uint32_t to,
                                      std::uint8_t alpha) const {
    const auto found = edges_.find(edge(to, from));
    if (found == edges_.end() || used_[found->second] || triangles_[found->second].alpha != alpha)
      return std::nullopt;
    return found->second;
  }

  // The corner of `t` that isn't `a` or `b`
  std::uint32_t third(std::uint32_t t, std::uint32_t a, std::uint32_t b) const {
    for (std::uint32_t corner : triangles_[t].corners)
      if (corner != a && corner != b)
        return corner;
    return triangles_[t].corners[0];
  }

  // Whether the quad a b c d, around it, is convex and faces `normal`
  bool convex(const std::array<std::uint32_t, 4> &quad, const Point &normal) const {
    for (int k = 0; k < 4; ++k) {
      const Point &p = points_[quad[k]], &q = points_[quad[(k + 1) % 4]],
                  &r = points_[quad[(k + 2) % 4]];
      if (dot(cross(q - p, r - q), normal) <= 0)
        return false;
    }
    return true;
  }

  void joinQuad(std::uint32_t t, Group &group) {
    const Triangle &triangle = triangles_[t];
    if (dot(triangle.normal, triangle.normal) == 0)
      return;
    used_[t] = true; // (so it isn't its own neighbour)
    for (int k = 0; k < 3; ++k) {
      const std::uint32_t a = triangle.corners[k], b = triangle.corners[(k + 1) % 3],
                          c = triangle.corners[(k + 2) % 3];
      const auto other = across(a, b, triangle.alpha);
      if (!other || dot(triangle.normal, triangles_[*other].normal) < kSamePlane)
        continue;
      // The other triangle is b a d: around the two, a d b c
      const std::uint32_t d = third(*other, a, b);
      const std::array<std::uint32_t, 4> quad{a, d, b, c};
      if (d == c || !convex(quad, triangle.normal))
        continue;
      used_[*other] = true;
      group.quads.insert(group.quads.end(), quad.begin(), quad.end());
      return;
    }
    used_[t] = false;
  }

  // The strip from triangle `t`, starting with its corners from `first`:
  // its indices, and in `taken` its triangles (not yet marked used)
  std::vector<std::uint32_t> stripFrom(std::uint32_t t, int first,
                                       std::vector<std::uint32_t> &taken) {
    const Triangle &start = triangles_[t];
    std::vector<std::uint32_t> strip{start.corners[first], start.corners[(first + 1) % 3],
                                     start.corners[(first + 2) % 3]};
    taken.assign(1, t);
    ++stripStamp_; // inStrip_ is this for the triangles in this strip
    inStrip_[t] = stripStamp_;
    // Triangle i of a strip is s[i] s[i+1] s[i+2], every other one reversed
    // (s[i+1] s[i] s[i+2]); the next is across its edge s[i+1] - s[i+2],
    // which runs s[i+1] -> s[i+2] in an even one and back in an odd one
    while (true) {
      const std::size_t i = strip.size() - 3;
      const std::uint32_t p = strip[i + 1], q = strip[i + 2];
      const auto next = i % 2 == 0 ? across(p, q, start.alpha) : across(q, p, start.alpha);
      if (!next || inStrip_[*next] == stripStamp_)
        break;
      strip.push_back(third(*next, p, q));
      taken.push_back(*next);
      inStrip_[*next] = stripStamp_;
    }
    return strip;
  }

  void chainStrip(std::uint32_t t, Group &group) {
    std::vector<std::uint32_t> best, bestTaken, taken;
    for (int first = 0; first < 3; ++first) {
      std::vector<std::uint32_t> strip = stripFrom(t, first, taken);
      if (strip.size() > best.size()) {
        best = std::move(strip);
        bestTaken = taken;
      }
    }
    for (std::uint32_t used : bestTaken)
      used_[used] = true;
    if (bestTaken.size() == 1)
      group.triangles.insert(group.triangles.end(), best.begin(), best.end());
    else
      group.strips.push_back(std::move(best));
  }

  std::vector<Point> points_;
  std::vector<Triangle> triangles_;
  std::unordered_map<std::uint64_t, std::uint32_t> edges_; // to the first triangle with it
  std::vector<bool> used_;
  std::vector<std::uint32_t> inStrip_; // which strip tried last took each
  std::uint32_t stripStamp_ = 0;
};

} // namespace

Primitives buildPrimitives(const MeshData &mesh) { return Builder(mesh).build(); }

std::uint8_t polygonAlpha(Colour a, Colour b, Colour c) {
  const unsigned sum = a.a + b.a + c.a; // 0 to 3 x 255
  return static_cast<std::uint8_t>((sum * 31 + 3 * 255 / 2) / (3 * 255));
}

FrameCounts costOf(std::span<const Primitive> runs) {
  FrameCounts cost;
  for (const Primitive &run : runs) {
    switch (run.kind) {
    case Primitive::Kind::Triangles:
      cost += {run.indexCount / 3, run.indexCount / 3 * 3};
      break;
    case Primitive::Kind::Quads:
      cost += {run.indexCount / 4, run.indexCount / 4 * 4};
      break;
    case Primitive::Kind::TriangleStrip:
      if (run.indexCount >= 3)
        cost += {run.indexCount - 2, run.indexCount};
      break;
    }
  }
  return cost;
}

FrameCounts costOf(const MeshData &mesh) {
  if (!mesh.primitives.empty())
    return costOf(mesh.primitives);
  return costOf(buildPrimitives(mesh).runs);
}

std::vector<std::uint32_t> triangleList(const MeshData &mesh) {
  if (mesh.primitives.empty())
    return {mesh.indices.begin(), mesh.indices.end()};
  std::vector<std::uint32_t> list;
  std::size_t at = 0;
  for (const Primitive &run : mesh.primitives) {
    const std::span<const std::uint32_t> s = mesh.indices.subspan(at, run.indexCount);
    at += run.indexCount;
    switch (run.kind) {
    case Primitive::Kind::Triangles:
      list.insert(list.end(), s.begin(), s.end());
      break;
    case Primitive::Kind::Quads:
      for (std::size_t q = 0; q + 3 < s.size(); q += 4)
        list.insert(list.end(), {s[q], s[q + 1], s[q + 2], s[q], s[q + 2], s[q + 3]});
      break;
    case Primitive::Kind::TriangleStrip:
      for (std::size_t i = 0; i + 2 < s.size(); ++i) {
        if (i % 2 == 0)
          list.insert(list.end(), {s[i], s[i + 1], s[i + 2]});
        else
          list.insert(list.end(), {s[i + 1], s[i], s[i + 2]});
      }
      break;
    }
  }
  return list;
}

bool hasTranslucentVertices(const MeshData &mesh) {
  return std::ranges::any_of(mesh.vertices, [](const Vertex &v) { return v.colour.a < 255; });
}

} // namespace ScratchPadd::Scene
