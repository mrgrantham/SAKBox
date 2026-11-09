// Cook.hpp - what the cook tool does to a model, apart from reading and
// writing files: check it against a platform's budget, and simplify it to
// fit when the rule says so (see README.md).
#pragma once

#include <ScratchPadd/Scene/Primitives.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Cook {

// A model as the cook works on it: indexed triangles, each vertex a
// position, a normal and a colour (red, green, blue, alpha, 0-1)
struct Mesh {
  std::vector<float> positions; // 3 a vertex
  std::vector<float> normals;   // 3 a vertex
  std::vector<float> colours;   // 4 a vertex
  std::vector<std::uint32_t> indices;

  std::size_t vertexCount() const { return positions.size() / 3; }
  std::size_t triangleCount() const { return indices.size() / 3; }
};

// What a platform can draw, and what a model may use of it
struct Budget {
  std::uint32_t polygons = 0; // 0: no limit
  std::uint32_t vertices = 0; // vertices sent to draw it (0: no limit)
};

// How a platform draws a model, which is what its budget counts
enum class Drawing {
  triangles,      // separate triangles: a polygon and 3 vertices each
  quadsAndStrips, // as the DS: Scene::buildPrimitives' quads, strips and
                  // triangles (a polygon each; 4 vertices a quad, a strip's
                  // once each), which its backend draws as cooked
};

// What it costs to draw, as the platform counts it
Budget costOf(const Mesh &mesh, Drawing drawing);
bool fits(const Budget &cost, const Budget &budget);

// The mesh as the scene layer has it (colours as bytes, as the cooked C++
// gives them), and as the DS draws it
std::vector<ScratchPadd::Scene::Vertex> sceneVertices(const Mesh &mesh);
ScratchPadd::Scene::Primitives primitivesOf(const Mesh &mesh);

// What to do with a model over its budget
enum class Mode {
  fail,              // stop the build
  simplify,          // simplify, keeping its look: seams, edges, normals
                     // and colours in place, the error capped
  simplifyAggressive // as simplify, then whatever fits: crossing seams,
                     // then merging nearby vertices regardless of shape
};
std::optional<Mode> modeFromName(const std::string &name); // "FAIL", ...
const char *nameOf(Mode mode);

// The most a simplify step may move the surface, as a share of the model's
// size, in Mode::simplify
inline constexpr float kSimplifyError = 0.05f;

struct Cooked {
  Mesh mesh;
  // The steps taken, in order: "as it is", "simplify", "simplify (no error
  // cap)", "simplify (across seams)", "simplify (sloppy)"
  std::vector<std::string> steps;
  float error = 0; // the last step's, as a share of the model's size
  bool fitsBudget = true;
};

// `mesh` fitted to `budget`, drawn as `drawing`, as `mode` allows: as it is
// if it fits; if not, simplified step by step until it does (or no step's
// left), each step keeping as many triangles as fit. fitsBudget says
// whether it ended within the budget.
Cooked cook(const Mesh &mesh, const Budget &budget, Mode mode, Drawing drawing);

} // namespace Cook
