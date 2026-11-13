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

// Whether a mesh is closed: every edge shared by exactly two triangles,
// edges matched by where their ends are (so a flat-shaded mesh, each
// triangle with its own vertices, is closed if its surface is). A closed
// mesh never shows its triangles' backs from outside, so they're culled;
// an open one (a plane, a pane, a fin of one sheet) is drawn double-sided.
struct Sides {
  bool closed = false;
  std::size_t openEdges = 0; // edges on only one triangle, or on more than two
};
Sides sidesOf(const Mesh &mesh);

// What a platform can draw, and what a model may use of it
struct Budget {
  std::uint32_t polygons = 0; // 0: no limit
  std::uint32_t vertices = 0; // vertices sent to draw it (0: no limit)
};

// How a platform draws a model, which is what its budget counts
enum class Drawing {
  Triangles,      // separate triangles: a polygon and 3 vertices each
  QuadsAndStrips, // as the DS: Scene::buildPrimitives' quads, strips and
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
  Fail,              // stop the build
  Simplify,          // simplify, keeping its look: seams, edges, normals
                     // and colours in place, the error capped
  SimplifyAggressive // as simplify, then whatever fits: crossing seams,
                     // then merging nearby vertices regardless of shape
};
std::optional<Mode> modeFromName(const std::string &name); // "FAIL", ...
const char *nameOf(Mode mode);

// The most a simplify step may move the surface, as a share of the model's
// size, in Mode::simplify
inline constexpr float kSimplifyError = 0.05f;
// The cleanup's error cap: a hundredth of a percent of the model's size, so
// it merges only what's flat (or in a straight line) and the same colour,
// and the model looks as it did
inline constexpr float kCleanupError = 0.0001f;

struct Cooked {
  Mesh mesh;
  // The steps taken, in order: "clean up" (if it took any triangles away),
  // then "as it is", or "simplify", "simplify (no error cap)", "simplify
  // (across seams)", "simplify (sloppy)"
  std::vector<std::string> steps;
  float error = 0; // the last step's, as a share of the model's size
  bool fitsBudget = true;
};

// `mesh` fitted to `budget`, drawn as `drawing`, as `mode` allows: as it is
// if it fits; if not, simplified step by step until it does (or no step's
// left), each step keeping as many triangles as fit. fitsBudget says
// whether it ended within the budget.
// `mesh` for a platform: first cleaned up (unless `cleanup` is false: a
// model kept heavy on purpose, to test a device), triangles on flat
// surfaces merged where nothing would look different; then, if it's over
// `budget`, what `mode` says
Cooked cook(const Mesh &mesh, const Budget &budget, Mode mode, Drawing drawing,
            bool cleanup = true);

// The lossless cleanup alone: `mesh` with the triangles merged that don't
// change how it looks (the same shape within kCleanupError, the same
// normals and colours)
Mesh cleanedUp(const Mesh &mesh);

} // namespace Cook
