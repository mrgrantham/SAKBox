// The cook's budgets and simplification, on meshes made here (the whole
// pipeline, from Blender, is checked by the example model's build).
#include "tools/assets/cook/Cook.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace Cook;

namespace {

// A grid of `n` x `n` squares, two triangles each, raised by `height(x, z)`
template <typename Height> Mesh grid(int n, Height height) {
  Mesh mesh;
  for (int z = 0; z <= n; ++z) {
    for (int x = 0; x <= n; ++x) {
      const float fx = static_cast<float>(x) / n, fz = static_cast<float>(z) / n;
      mesh.positions.insert(mesh.positions.end(), {fx, height(fx, fz), fz});
      mesh.normals.insert(mesh.normals.end(), {0, 1, 0});
      mesh.colours.insert(mesh.colours.end(), {1, 1, 1, 1});
    }
  }
  const auto at = [n](int x, int z) { return static_cast<std::uint32_t>(z * (n + 1) + x); };
  for (int z = 0; z < n; ++z) {
    for (int x = 0; x < n; ++x) {
      mesh.indices.insert(mesh.indices.end(), {at(x, z), at(x, z + 1), at(x + 1, z)});
      mesh.indices.insert(mesh.indices.end(), {at(x + 1, z), at(x, z + 1), at(x + 1, z + 1)});
    }
  }
  return mesh;
}

// For the tests of budgets and simplifying: their flat grids kept as they
// are (the cleanup would merge them to two triangles first)
constexpr bool kNoCleanup = false;

Mesh flat(int n) {
  return grid(n, [](float, float) { return 0.0f; });
}

// Bumpy everywhere: no two neighbouring triangles in one plane
Mesh bumpy(int n) {
  return grid(n, [](float x, float z) { return 0.3f * std::sin(40 * x) * std::cos(37 * z); });
}

// A unit cube flat-shaded: each face its own four vertices, wound
// counter-clockwise seen from outside
Mesh cube() {
  Mesh mesh;
  const float faces[6][4][3] = {
      {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}, {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
      {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
      {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}, {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}};
  for (const auto &face : faces) {
    const auto first = static_cast<std::uint32_t>(mesh.vertexCount());
    for (const auto &corner : face) {
      mesh.positions.insert(mesh.positions.end(), corner, corner + 3);
      mesh.normals.insert(mesh.normals.end(), {0, 0, 0});
      mesh.colours.insert(mesh.colours.end(), {1, 1, 1, 1});
    }
    mesh.indices.insert(mesh.indices.end(),
                        {first, first + 1, first + 2, first, first + 2, first + 3});
  }
  return mesh;
}

} // namespace

// Separate triangles cost a polygon and three vertices each; a budget of
// 0 is no limit.
TEST(CookTest, CostsAPolygonAndThreeVerticesATriangle) {
  const Budget cost = costOf(flat(10), Drawing::triangles); // 200 triangles
  EXPECT_EQ(cost.polygons, 200u);
  EXPECT_EQ(cost.vertices, 600u);
  EXPECT_TRUE(fits(cost, {}));
  EXPECT_TRUE(fits(cost, {.polygons = 200, .vertices = 600}));
  EXPECT_FALSE(fits(cost, {.polygons = 199}));
  EXPECT_FALSE(fits(cost, {.vertices = 599}));
}

// Within budget, a model is left as it is, whatever the mode.
TEST(CookTest, AModelThatFitsIsLeftAsItIs) {
  const Mesh mesh = bumpy(10);
  for (Mode mode : {Mode::fail, Mode::simplify, Mode::simplifyAggressive}) {
    const Cooked cooked = cook(mesh, {.polygons = 200}, mode, Drawing::triangles, kNoCleanup);
    EXPECT_TRUE(cooked.fitsBudget);
    EXPECT_EQ(cooked.mesh.indices, mesh.indices);
    EXPECT_EQ(cooked.steps, std::vector<std::string>{"as it is"});
  }
}

// FAIL changes nothing over budget, and says it doesn't fit.
TEST(CookTest, FailLeavesItOverBudget) {
  const Cooked cooked = cook(flat(10), {.polygons = 50}, Mode::fail, Drawing::triangles, kNoCleanup);
  EXPECT_FALSE(cooked.fitsBudget);
  EXPECT_EQ(cooked.mesh.triangleCount(), 200u);
}

// SIMPLIFY brings a flat grid within budget without changing its shape, and
// keeps only the vertices it uses.
TEST(CookTest, SimplifyFitsWhatItCanWithoutChangingItsLook) {
  const Cooked cooked = cook(flat(20), {.polygons = 100}, Mode::simplify, Drawing::triangles, kNoCleanup);
  ASSERT_TRUE(cooked.fitsBudget);
  EXPECT_LE(cooked.mesh.triangleCount(), 100u);
  EXPECT_EQ(cooked.steps, std::vector<std::string>{"simplify"});
  EXPECT_LT(cooked.error, 1e-3f); // flat: nothing moved
  for (std::uint32_t index : cooked.mesh.indices)
    EXPECT_LT(index, cooked.mesh.vertexCount());
  for (std::size_t i = 1; i < cooked.mesh.positions.size(); i += 3)
    EXPECT_EQ(cooked.mesh.positions[i], 0.0f); // still flat
}

// A bumpy surface can't come down to a handful of triangles within
// SIMPLIFY's error cap, so it fails; SIMPLIFY_AGGRESSIVE goes on through its
// rougher steps until it fits.
TEST(CookTest, AggressiveGoesFurtherThanSimplify) {
  const Mesh mesh = bumpy(30); // 1800 triangles
  const Budget budget{.polygons = 20};
  const Cooked gentle = cook(mesh, budget, Mode::simplify, Drawing::triangles);
  EXPECT_FALSE(gentle.fitsBudget);
  EXPECT_GT(gentle.mesh.triangleCount(), 20u);
  EXPECT_LE(gentle.error, kSimplifyError);

  const Cooked aggressive = cook(mesh, budget, Mode::simplifyAggressive, Drawing::triangles);
  ASSERT_TRUE(aggressive.fitsBudget);
  EXPECT_LE(aggressive.mesh.triangleCount(), 20u);
  EXPECT_GT(aggressive.mesh.triangleCount(), 0u);
  EXPECT_GE(aggressive.steps.size(), 2u);
  EXPECT_EQ(aggressive.steps.front(), "simplify");
}

// A flat-shaded mesh (each face its own vertices and normal, so every edge
// a seam) is simplified welded and comes back flat-shaded: SIMPLIFY reaches
// the budget, and each triangle has its own vertices with its face's
// normal.
TEST(CookTest, FlatShadedMeshesSimplifyAndStayFlat) {
  const Mesh smooth = flat(20);
  Mesh faceted;
  for (std::uint32_t index : smooth.indices) {
    faceted.positions.insert(faceted.positions.end(), &smooth.positions[index * 3],
                             &smooth.positions[index * 3 + 3]);
    faceted.normals.insert(faceted.normals.end(), {0, 1, 0});
    faceted.colours.insert(faceted.colours.end(), {1, 1, 1, 1});
    faceted.indices.push_back(static_cast<std::uint32_t>(faceted.indices.size()));
  }
  const Cooked cooked = cook(faceted, {.polygons = 100}, Mode::simplify, Drawing::triangles, kNoCleanup);
  ASSERT_TRUE(cooked.fitsBudget);
  EXPECT_LE(cooked.mesh.triangleCount(), 100u);
  EXPECT_EQ(cooked.mesh.vertexCount(), cooked.mesh.indices.size()); // a vertex a corner
  for (std::size_t v = 0; v < cooked.mesh.vertexCount(); ++v)
    EXPECT_NEAR(std::abs(cooked.mesh.normals[v * 3 + 1]), 1.0f, 1e-5f) << v;
}

// On the DS, a model is counted as quads and strips: a flat grid's
// squares are quads, half the polygons, so 200 triangles fit 100.
// Simplifying aims as high as that allows: a bumpy grid in strips keeps
// about as many triangles as the budget has polygons, not fewer.
TEST(CookTest, TheDsCountsQuadsAndStrips) {
  const Budget flatCost = costOf(flat(10), Drawing::quadsAndStrips);
  EXPECT_EQ(flatCost.polygons, 100u);
  EXPECT_EQ(flatCost.vertices, 400u);
  EXPECT_TRUE(cook(flat(10), {.polygons = 100}, Mode::fail, Drawing::quadsAndStrips, kNoCleanup).fitsBudget);

  const Budget budget{.polygons = 400, .vertices = 1200};
  const Cooked cooked = cook(bumpy(30), budget, Mode::simplifyAggressive, Drawing::quadsAndStrips);
  ASSERT_TRUE(cooked.fitsBudget);
  EXPECT_TRUE(fits(costOf(cooked.mesh, Drawing::quadsAndStrips), budget));
  EXPECT_GT(cooked.mesh.triangleCount(), 300u);
}

// A closed mesh (its edges matched by where they are, so a flat-shaded
// cube is closed) is culled; an open one, a sheet, is drawn double-sided.
// Its open edges: a grid's rim, a cube missing a face's four.
TEST(CookTest, ClosedMeshesHaveNoOpenEdges) {
  const Sides cubeSides = sidesOf(cube());
  EXPECT_TRUE(cubeSides.closed);
  EXPECT_EQ(cubeSides.openEdges, 0u);

  const Sides sheet = sidesOf(flat(4));
  EXPECT_FALSE(sheet.closed);
  EXPECT_EQ(sheet.openEdges, 16u);

  Mesh opened = cube();
  opened.indices.resize(opened.indices.size() - 6);
  const Sides box = sidesOf(opened);
  EXPECT_FALSE(box.closed);
  EXPECT_EQ(box.openEdges, 4u);

  EXPECT_FALSE(sidesOf(Mesh{}).closed);
}

TEST(CookTest, ModesByName) {
  EXPECT_EQ(modeFromName("FAIL"), Mode::fail);
  EXPECT_EQ(modeFromName("SIMPLIFY"), Mode::simplify);
  EXPECT_EQ(modeFromName("SIMPLIFY_AGGRESSIVE"), Mode::simplifyAggressive);
  EXPECT_FALSE(modeFromName("simplify"));
  EXPECT_STREQ(nameOf(Mode::simplifyAggressive), "SIMPLIFY_AGGRESSIVE");
}

namespace {

// A unit cube flat-shaded, each face cut into `n` x `n` squares, every
// triangle its own three vertices with its face's normal (as Blender
// exports a subdivided box with flat shading)
Mesh subdividedCube(int n) {
  Mesh mesh;
  // Each face: a corner and two edges along it, wound counter-clockwise
  // seen from outside, and its normal
  const float faces[6][4][3] = {
      {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}},  {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, 0, -1}},
      {{1, 0, 1}, {0, 0, -1}, {0, 1, 0}, {1, 0, 0}}, {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 0}},
      {{0, 1, 1}, {1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, -1, 0}}};
  for (const auto &face : faces) {
    const auto at = [&](float u, float v) {
      return std::array<float, 3>{face[0][0] + u * face[1][0] + v * face[2][0],
                                  face[0][1] + u * face[1][1] + v * face[2][1],
                                  face[0][2] + u * face[1][2] + v * face[2][2]};
    };
    const auto corner = [&](const std::array<float, 3> &p) {
      mesh.positions.insert(mesh.positions.end(), p.begin(), p.end());
      mesh.normals.insert(mesh.normals.end(), face[3], face[3] + 3);
      mesh.colours.insert(mesh.colours.end(), {1, 1, 1, 1});
      mesh.indices.push_back(static_cast<std::uint32_t>(mesh.indices.size()));
    };
    for (int i = 0; i < n; ++i)
      for (int j = 0; j < n; ++j) {
        const float u0 = float(i) / n, u1 = float(i + 1) / n, v0 = float(j) / n,
                    v1 = float(j + 1) / n;
        corner(at(u0, v0)), corner(at(u1, v0)), corner(at(u1, v1));
        corner(at(u0, v0)), corner(at(u1, v1)), corner(at(u0, v1));
      }
  }
  return mesh;
}

} // namespace

// The cleanup merges what's flat and the same colour, and nothing else: a
// flat grid loses nine in ten of its triangles and a flat-shaded subdivided
// cube more than eight in ten (not all they could: meshoptimizer's own
// rules, and the error cap its passes share, stop it short of the cube's
// 12), the cube still closed; a bumpy grid keeps every triangle; two
// colours keep their edge between them.
TEST(CookTest, CleanupMergesOnlyWhatsFlat) {
  EXPECT_LE(cleanedUp(flat(10)).triangleCount(), 200u / 10);
  EXPECT_EQ(cleanedUp(bumpy(10)).triangleCount(), bumpy(10).triangleCount());

  const Mesh cube = subdividedCube(8);
  ASSERT_EQ(cube.triangleCount(), 6u * 8 * 8 * 2);
  const Mesh cleaned = cleanedUp(cube);
  EXPECT_LE(cleaned.triangleCount(), cube.triangleCount() * 15 / 100);
  EXPECT_TRUE(sidesOf(cleaned).closed); // no holes

  // Half the grid red: the line between the colours stays
  Mesh twoColours = flat(10);
  for (std::size_t v = 0; v < twoColours.vertexCount(); ++v)
    if (twoColours.positions[v * 3] > 0.5f)
      twoColours.colours[v * 4 + 1] = 0;
  const Mesh kept = cleanedUp(twoColours);
  EXPECT_GT(kept.triangleCount(), cleanedUp(flat(10)).triangleCount());
  EXPECT_LT(kept.triangleCount(), twoColours.triangleCount());
  // Its colours are only the two
  for (std::size_t v = 0; v < kept.vertexCount(); ++v)
    EXPECT_TRUE(kept.colours[v * 4 + 1] == 0 || kept.colours[v * 4 + 1] == 1);
}

// cook() cleans up first, for every mode and platform, and says so; with
// cleanup off a model keeps every triangle (heavy on purpose)
TEST(CookTest, CookCleansUpUnlessTurnedOff) {
  const Cooked cleaned = cook(subdividedCube(8), {}, Mode::fail, Drawing::triangles);
  EXPECT_LE(cleaned.mesh.triangleCount(), 768u * 15 / 100);
  EXPECT_EQ(cleaned.steps, (std::vector<std::string>{"clean up", "as it is"}));

  const Cooked heavy = cook(subdividedCube(8), {}, Mode::fail, Drawing::triangles, false);
  EXPECT_EQ(heavy.mesh.triangleCount(), 768u);
  EXPECT_EQ(heavy.steps, (std::vector<std::string>{"as it is"}));

  // Nothing flat: no cleanup step
  EXPECT_EQ(cook(bumpy(10), {}, Mode::fail, Drawing::triangles).steps,
            (std::vector<std::string>{"as it is"}));
  // Over budget: simplified from the cleaned-up mesh, the cleanup first
  const Cooked simplified =
      cook(subdividedCube(8), {.polygons = 10}, Mode::simplifyAggressive, Drawing::triangles);
  ASSERT_GE(simplified.steps.size(), 2u);
  EXPECT_EQ(simplified.steps[0], "clean up");
}
