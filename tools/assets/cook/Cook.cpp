#include "Cook.hpp"

#include <meshoptimizer.h>

#include <algorithm>
#include <cmath>

namespace Cook {

Budget costOf(const Mesh &mesh) {
  const auto triangles = static_cast<std::uint32_t>(mesh.triangleCount());
  return {.polygons = triangles, .vertices = 3 * triangles};
}

bool fits(const Budget &cost, const Budget &budget) {
  return (budget.polygons == 0 || cost.polygons <= budget.polygons) &&
         (budget.vertices == 0 || cost.vertices <= budget.vertices);
}

std::optional<Mode> modeFromName(const std::string &name) {
  if (name == "FAIL")
    return Mode::fail;
  if (name == "SIMPLIFY")
    return Mode::simplify;
  if (name == "SIMPLIFY_AGGRESSIVE")
    return Mode::simplifyAggressive;
  return std::nullopt;
}

const char *nameOf(Mode mode) {
  switch (mode) {
  case Mode::fail:
    return "FAIL";
  case Mode::simplify:
    return "SIMPLIFY";
  case Mode::simplifyAggressive:
    return "SIMPLIFY_AGGRESSIVE";
  }
  return "?";
}

namespace {

// The most triangles `budget` allows
std::size_t triangleBudget(const Budget &budget) {
  std::size_t most = SIZE_MAX;
  if (budget.polygons)
    most = budget.polygons;
  if (budget.vertices)
    most = std::min<std::size_t>(most, budget.vertices / 3);
  return most;
}

// `mesh` with only the vertices `indices` uses, in the order first used
// (so they're fetched in order when drawn)
Mesh compact(const Mesh &mesh, std::vector<std::uint32_t> indices) {
  std::vector<std::uint32_t> remap(mesh.vertexCount());
  const std::size_t kept = meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(),
                                                            indices.size(), mesh.vertexCount());
  Mesh out;
  out.positions.resize(kept * 3);
  out.normals.resize(kept * 3);
  out.colours.resize(kept * 4);
  for (std::size_t v = 0; v < mesh.vertexCount(); ++v) {
    if (remap[v] == ~0u)
      continue;
    std::copy_n(&mesh.positions[v * 3], 3, &out.positions[remap[v] * 3]);
    std::copy_n(&mesh.normals[v * 3], 3, &out.normals[remap[v] * 3]);
    std::copy_n(&mesh.colours[v * 4], 4, &out.colours[remap[v] * 4]);
  }
  meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
  out.indices = std::move(indices);
  return out;
}

// One simplify step: meshoptimizer's attribute-aware simplifier (normals and
// colours weighed in), or its sloppy one, aiming at `targetTriangles`
struct Step {
  const char *name;
  bool sloppy;
  float maxError;
  unsigned options;
};

std::vector<std::uint32_t> simplify(const Mesh &mesh, const Step &step, std::size_t targetTriangles,
                                    float &error) {
  std::vector<std::uint32_t> indices(mesh.indices.size());
  const std::size_t target = targetTriangles * 3;
  std::size_t count;
  if (step.sloppy) {
    count = meshopt_simplifySloppy(indices.data(), mesh.indices.data(), mesh.indices.size(),
                                   mesh.positions.data(), mesh.vertexCount(), sizeof(float) * 3,
                                   nullptr, target, step.maxError, &error);
  } else {
    // Each vertex's normal and colour, side by side, as the simplifier wants
    // its attributes; normals weigh more than colours (shading shows more)
    std::vector<float> attributes(mesh.vertexCount() * 7);
    for (std::size_t v = 0; v < mesh.vertexCount(); ++v) {
      std::copy_n(&mesh.normals[v * 3], 3, &attributes[v * 7]);
      std::copy_n(&mesh.colours[v * 4], 4, &attributes[v * 7 + 3]);
    }
    const float weights[7] = {0.5f, 0.5f, 0.5f, 0.25f, 0.25f, 0.25f, 0.25f};
    count = meshopt_simplifyWithAttributes(
        indices.data(), mesh.indices.data(), mesh.indices.size(), mesh.positions.data(),
        mesh.vertexCount(), sizeof(float) * 3, attributes.data(), sizeof(float) * 7, weights, 7,
        nullptr, target, step.maxError, step.options, &error);
  }
  indices.resize(count);
  return indices;
}

// Whether every triangle's corners all have its face's normal: a
// flat-shaded mesh, where every edge is a seam (each face has its own
// vertices), which the simplifier can't collapse without crossing one
bool flatShaded(const Mesh &mesh) {
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const float *a = &mesh.positions[mesh.indices[t] * 3],
                *b = &mesh.positions[mesh.indices[t + 1] * 3],
                *c = &mesh.positions[mesh.indices[t + 2] * 3];
    const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    float face[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2],
                     u[0] * w[1] - u[1] * w[0]};
    const float length = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);
    if (length == 0)
      continue;
    for (float &f : face)
      f /= length;
    for (int k = 0; k < 3; ++k) {
      const float *n = &mesh.normals[mesh.indices[t + k] * 3];
      if (n[0] * face[0] + n[1] * face[1] + n[2] * face[2] < 0.999f)
        return false;
    }
  }
  return !mesh.indices.empty();
}

// A flat-shaded mesh welded: vertices at the same place with the same
// colour made one, each with the average of its faces' normals, so the
// simplifier can collapse its edges
Mesh weld(const Mesh &mesh) {
  const meshopt_Stream streams[] = {
      {mesh.positions.data(), sizeof(float) * 3, sizeof(float) * 3},
      {mesh.colours.data(), sizeof(float) * 4, sizeof(float) * 4},
  };
  std::vector<std::uint32_t> remap(mesh.vertexCount());
  const std::size_t unique =
      meshopt_generateVertexRemapMulti(remap.data(), mesh.indices.data(), mesh.indices.size(),
                                       mesh.vertexCount(), streams, std::size(streams));
  Mesh welded;
  welded.positions.resize(unique * 3);
  welded.colours.resize(unique * 4);
  welded.normals.assign(unique * 3, 0.0f);
  for (std::size_t v = 0; v < mesh.vertexCount(); ++v) {
    std::copy_n(&mesh.positions[v * 3], 3, &welded.positions[remap[v] * 3]);
    std::copy_n(&mesh.colours[v * 4], 4, &welded.colours[remap[v] * 4]);
    for (int d = 0; d < 3; ++d)
      welded.normals[remap[v] * 3 + d] += mesh.normals[v * 3 + d];
  }
  for (std::size_t v = 0; v < unique; ++v) {
    float *n = &welded.normals[v * 3];
    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (length > 0)
      for (int d = 0; d < 3; ++d)
        n[d] /= length;
  }
  welded.indices.resize(mesh.indices.size());
  meshopt_remapIndexBuffer(welded.indices.data(), mesh.indices.data(), mesh.indices.size(),
                           remap.data());
  return welded;
}

// The mesh flat-shaded again: each triangle its own three vertices, with
// its face's normal
Mesh flatten(const Mesh &mesh) {
  Mesh flat;
  for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
    const float *a = &mesh.positions[mesh.indices[t] * 3],
                *b = &mesh.positions[mesh.indices[t + 1] * 3],
                *c = &mesh.positions[mesh.indices[t + 2] * 3];
    const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    float n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]};
    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (length > 0)
      for (float &d : n)
        d /= length;
    for (int k = 0; k < 3; ++k) {
      const std::uint32_t v = mesh.indices[t + k];
      flat.positions.insert(flat.positions.end(), &mesh.positions[v * 3],
                            &mesh.positions[v * 3 + 3]);
      flat.normals.insert(flat.normals.end(), n, n + 3);
      flat.colours.insert(flat.colours.end(), &mesh.colours[v * 4], &mesh.colours[v * 4 + 4]);
      flat.indices.push_back(static_cast<std::uint32_t>(flat.indices.size()));
    }
  }
  return flat;
}

} // namespace

Cooked cook(const Mesh &mesh, const Budget &budget, Mode mode) {
  Cooked cooked{.mesh = mesh, .steps = {"as it is"}};
  if (fits(costOf(mesh), budget) || mode == Mode::fail) {
    cooked.fitsBudget = fits(costOf(mesh), budget);
    return cooked;
  }
  // Each step from the original, rougher than the last, until one fits:
  // keeping the look (outer edges locked, the error capped), then for
  // SIMPLIFY_AGGRESSIVE without the cap, across seams, and finally sloppy
  // (merging nearby vertices, whatever the mesh's shape)
  const Step steps[] = {
      {"simplify", false, kSimplifyError, meshopt_SimplifyLockBorder},
      {"simplify (no error cap)", false, 1.0f, meshopt_SimplifyLockBorder},
      {"simplify (across seams)", false, 1.0f, meshopt_SimplifyPermissive},
      {"simplify (sloppy)", true, 1.0f, 0},
  };
  const std::size_t stepCount = mode == Mode::simplify ? 1 : std::size(steps);
  const std::size_t target = triangleBudget(budget);
  // A flat-shaded mesh is simplified welded, then flat-shaded again
  const bool flat = flatShaded(mesh);
  const Mesh source = flat ? weld(mesh) : mesh;
  cooked.steps.clear();
  for (std::size_t i = 0; i < stepCount; ++i) {
    float error = 0;
    std::vector<std::uint32_t> indices = simplify(source, steps[i], target, error);
    cooked.steps.emplace_back(steps[i].name);
    cooked.error = error;
    cooked.mesh = compact(source, std::move(indices));
    if (flat)
      cooked.mesh = flatten(cooked.mesh);
    if (fits(costOf(cooked.mesh), budget)) {
      cooked.fitsBudget = true;
      return cooked;
    }
  }
  cooked.fitsBudget = false;
  return cooked;
}

} // namespace Cook
