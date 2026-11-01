// The cook's budgets and simplification, on meshes made here (the whole
// pipeline, from Blender, is checked by the example model's build).
#include "tools/assets/cook/Cook.hpp"

#include <gtest/gtest.h>

#include <cmath>

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

Mesh flat(int n) {
  return grid(n, [](float, float) { return 0.0f; });
}

// Bumpy everywhere: no two neighbouring triangles in one plane
Mesh bumpy(int n) {
  return grid(n, [](float x, float z) { return 0.3f * std::sin(40 * x) * std::cos(37 * z); });
}

} // namespace

// A triangle list costs a polygon and three vertices a triangle; a budget of
// 0 is no limit.
TEST(CookTest, CostsAPolygonAndThreeVerticesATriangle) {
  const Budget cost = costOf(flat(10)); // 200 triangles
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
    const Cooked cooked = cook(mesh, {.polygons = 200}, mode);
    EXPECT_TRUE(cooked.fitsBudget);
    EXPECT_EQ(cooked.mesh.indices, mesh.indices);
    EXPECT_EQ(cooked.steps, std::vector<std::string>{"as it is"});
  }
}

// FAIL changes nothing over budget, and says it doesn't fit.
TEST(CookTest, FailLeavesItOverBudget) {
  const Cooked cooked = cook(flat(10), {.polygons = 50}, Mode::fail);
  EXPECT_FALSE(cooked.fitsBudget);
  EXPECT_EQ(cooked.mesh.triangleCount(), 200u);
}

// SIMPLIFY brings a flat grid within budget without changing its shape, and
// keeps only the vertices it uses.
TEST(CookTest, SimplifyFitsWhatItCanWithoutChangingItsLook) {
  const Cooked cooked = cook(flat(20), {.polygons = 100}, Mode::simplify);
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
  const Cooked gentle = cook(mesh, budget, Mode::simplify);
  EXPECT_FALSE(gentle.fitsBudget);
  EXPECT_GT(gentle.mesh.triangleCount(), 20u);
  EXPECT_LE(gentle.error, kSimplifyError);

  const Cooked aggressive = cook(mesh, budget, Mode::simplifyAggressive);
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
  const Cooked cooked = cook(faceted, {.polygons = 100}, Mode::simplify);
  ASSERT_TRUE(cooked.fitsBudget);
  EXPECT_LE(cooked.mesh.triangleCount(), 100u);
  EXPECT_EQ(cooked.mesh.vertexCount(), cooked.mesh.indices.size()); // a vertex a corner
  for (std::size_t v = 0; v < cooked.mesh.vertexCount(); ++v)
    EXPECT_NEAR(std::abs(cooked.mesh.normals[v * 3 + 1]), 1.0f, 1e-5f) << v;
}

TEST(CookTest, ModesByName) {
  EXPECT_EQ(modeFromName("FAIL"), Mode::fail);
  EXPECT_EQ(modeFromName("SIMPLIFY"), Mode::simplify);
  EXPECT_EQ(modeFromName("SIMPLIFY_AGGRESSIVE"), Mode::simplifyAggressive);
  EXPECT_FALSE(modeFromName("simplify"));
  EXPECT_STREQ(nameOf(Mode::simplifyAggressive), "SIMPLIFY_AGGRESSIVE");
}
