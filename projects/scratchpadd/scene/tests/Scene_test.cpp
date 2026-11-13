// The scene layer's shared parts, without a backend: transforms, meshes
// built in code, and the overlay. Built twice: with Scalar as float, and as
// the DS has it (Q16.16).
#include <ScratchPadd/Scene/Culling.hpp>
#include <ScratchPadd/Scene/Primitives.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <vector>

using namespace ScratchPadd::Scene;

namespace {
// Float's rounding, or Q16.16's steps (2^-16) over a few operations (the
// fixed-point build, as on the DS)
constexpr double kTolerance = std::is_floating_point_v<Scalar> ? 1e-5 : 1e-3;
void expectNear(const Vector3 &actual, const Vector3 &expected) {
  EXPECT_NEAR(actual.x, expected.x, kTolerance);
  EXPECT_NEAR(actual.y, expected.y, kTolerance);
  EXPECT_NEAR(actual.z, expected.z, kTolerance);
}
constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2;
} // namespace

// A quarter turn about +z takes +x to +y (right-handed); translations,
// scalings and compositions apply in the order written.
TEST(TransformTest, TurnsMovesAndScales) {
  expectNear(Transform::rotation({0, 0, 1}, kQuarterTurn).apply({1, 0, 0}), {0, 1, 0});
  expectNear(Transform::rotation({0, 1, 0}, kQuarterTurn).apply({0, 0, 1}), {1, 0, 0});
  expectNear(Transform::translation({1, 2, 3}).apply({1, 1, 1}), {2, 3, 4});
  expectNear(Transform::scaling({2, 3, 4}).apply({1, 1, 1}), {2, 3, 4});

  // Scale, then turn, then move: the composition does the right-hand one first
  const Transform t = Transform::translation({10, 0, 0}) *
                      Transform::rotation({0, 0, 1}, kQuarterTurn) * Transform::scaling({2, 2, 2});
  expectNear(t.apply({1, 0, 0}), {10, 2, 0});
  expectNear(t.applyToDirection({1, 0, 0}), {0, 2, 0});
  expectNear(t.position(), {10, 0, 0});
  expectNear(Transform::identity().apply({4, 5, 6}), {4, 5, 6});
}

TEST(TransformTest, FromAxesPutsEachAxisWhereSaid) {
  const Transform t = Transform::fromAxes({0, 1, 0}, {-1, 0, 0}, {0, 0, 1}, {5, 0, 0});
  expectNear(t.apply({1, 0, 0}), {5, 1, 0});
  expectNear(t.apply({0, 1, 0}), {4, 0, 0});
}

// Lengths and directions of long vectors: in Q16.16, 256 squared would be
// past its range, so they're taken from the vector scaled down first
TEST(VectorTest, LongVectorsKeepTheirLengthAndDirection) {
  EXPECT_NEAR(Vector3(0, 256, 0).length(), 256, kTolerance);
  expectNear(Vector3(0, 256, 0).normalised(), {0, 1, 0});
  expectNear(Vector3(300, 0, -400).normalised(), {0.6f, 0, -0.8f});
  EXPECT_NEAR(Vector3(300, 0, -400).length(), 500, kTolerance);
  expectNear(Vector3{}.normalised(), {});
  // A 16 m quad's normal (the DS ragdoll's ground)
  MeshBuilder builder;
  builder.quad({-8, 0, 8}, {8, 0, 8}, {8, 0, -8}, {-8, 0, -8});
  expectNear(builder.data().vertices[0].normal, {0, 1, 0});
}

// Faces get their own vertices with the face's normal, facing out, and
// each triangle winds counter-clockwise seen from its front.
TEST(MeshBuilderTest, BoxesFaceOutward) {
  MeshBuilder builder;
  builder.box({1, 2, 3}, {0.5f, 1, 2}, Colour{10, 20, 30});
  const MeshData mesh = builder.data();
  EXPECT_EQ(builder.triangleCount(), 12u);
  EXPECT_EQ(mesh.vertices.size(), 24u);
  for (size_t i = 0; i < mesh.indices.size(); i += 3) {
    const Vertex &a = mesh.vertices[mesh.indices[i]], &b = mesh.vertices[mesh.indices[i + 1]],
                 &c = mesh.vertices[mesh.indices[i + 2]];
    const Vector3 winding = (b.position - a.position).cross(c.position - a.position);
    EXPECT_GT(winding.dot(a.normal), 0) << "triangle " << i / 3;
    // Outward: from the centre towards the face
    EXPECT_GT((a.position - Vector3{1, 2, 3}).dot(a.normal), 0) << "triangle " << i / 3;
    EXPECT_EQ(a.colour, (Colour{10, 20, 30}));
  }
}

// A mesh's bounding sphere is about the middle of its box and reaches
// its furthest vertex.
TEST(CullingTest, BoundsHoldEveryVertex) {
  MeshBuilder builder;
  builder.box({1, 2, 3}, {0.5f, 1, 2}, Colour{});
  const MeshData mesh = builder.data();
  const Bounds bounds = boundsOf(mesh);
  expectNear(bounds.centre, {1, 2, 3});
  Scalar furthest = 0;
  for (const Vertex &v : mesh.vertices) {
    const Scalar distance = (v.position - bounds.centre).length();
    EXPECT_LE(distance, bounds.radius + Scalar(kTolerance));
    furthest = std::max(furthest, distance);
  }
  EXPECT_NEAR(bounds.radius, furthest, kTolerance);
  EXPECT_EQ(boundsOf(MeshData{}).radius, 0);
}

// The default camera, at z = 5 looking down -z, 60 degrees tall, square:
// a unit sphere is outside only when wholly past a side, behind the eye,
// nearer than the near plane or beyond the far one; partly in is drawn.
TEST(CullingTest, FrustumKeepsWhatTheCameraSees) {
  const Camera camera;
  const Frustum frustum(camera, 1);
  const Bounds unit{{0, 0, 0}, 1};
  const auto outsideAt = [&](const Vector3 &at) {
    return frustum.outside(unit, Transform::translation(at));
  };
  EXPECT_FALSE(outsideAt({0, 0, 0}));   // in the middle
  EXPECT_FALSE(outsideAt({0, 0, -90})); // far, before the far plane
  EXPECT_TRUE(outsideAt({0, 0, -100})); // beyond it
  EXPECT_TRUE(outsideAt({0, 0, 10}));   // behind the eye
  EXPECT_TRUE(outsideAt({10, 0, 0}));   // off to the right
  EXPECT_TRUE(outsideAt({-10, 0, 0}));  // the left
  EXPECT_TRUE(outsideAt({0, 10, 0}));   // above
  EXPECT_TRUE(outsideAt({0, -10, 0}));  // below
  // At z = 0 the view's edge is 5 tan 30 = 2.89 from the middle: a unit
  // sphere at 3.5 straddles it, at 4.5 is past it
  EXPECT_FALSE(outsideAt({3.5f, 0, 0}));
  EXPECT_TRUE(outsideAt({4.5f, 0, 0}));
  // A scaled sphere is as big as its largest scale
  EXPECT_FALSE(
      frustum.outside(unit, Transform::translation({4.5f, 0, 0}) * Transform::scaling({1, 1, 3})));
  // Twice as wide (an aspect of 2), or widened, the side moves out
  EXPECT_FALSE(Frustum(camera, 2).outside(unit, Transform::translation({4.5f, 0, 0})));
  EXPECT_FALSE(Frustum(camera, 1, 2).outside(unit, Transform::translation({4.5f, 0, 0})));
  EXPECT_TRUE(Frustum(camera, 2).outside(unit, Transform::translation({0, 4.5f, 0})));
}

// Rectangles are two triangles; text is kept as it was given, for the
// backend to draw in its font; clear() empties both for the next frame.
TEST(OverlayTest, CollectsShapesAndText) {
  Overlay overlay;
  overlay.rectangle(10, 20, 30, 40, Colour{1, 2, 3, 4});
  ASSERT_EQ(overlay.triangles().size(), 6u);
  EXPECT_EQ(overlay.triangles()[2].x, 40);
  EXPECT_EQ(overlay.triangles()[2].y, 60);

  overlay.text(100, 50, "Hi", Colour{255, 0, 0}, 2);
  ASSERT_EQ(overlay.texts().size(), 1u);
  EXPECT_EQ(overlay.texts()[0].text, "Hi");
  EXPECT_EQ(overlay.texts()[0].x, 100);
  EXPECT_EQ(overlay.texts()[0].scale, 2);
  EXPECT_EQ(overlay.triangles().size(), 6u); // text isn't triangles here

  overlay.clear();
  EXPECT_TRUE(overlay.triangles().empty());
  EXPECT_TRUE(overlay.texts().empty());
}

// A line is a quad `thickness` wide around it
TEST(OverlayTest, LinesAreThinQuads) {
  Overlay overlay;
  overlay.line(10, 10, 110, 10, Colour{}, 4);
  ASSERT_EQ(overlay.triangles().size(), 6u);
  float top = 1e9f, bottom = -1e9f;
  for (const Overlay::Vertex &v : overlay.triangles()) {
    top = std::min(top, v.y);
    bottom = std::max(bottom, v.y);
  }
  EXPECT_FLOAT_EQ(top, 8);
  EXPECT_FLOAT_EQ(bottom, 12);
  overlay.clear();
  overlay.line(5, 5, 5, 5, Colour{}); // no length: nothing
  EXPECT_TRUE(overlay.triangles().empty());
}

TEST(EnvironmentTest, KeepsAtMostFourLights) {
  Environment environment;
  for (int i = 0; i < 6; ++i)
    environment.addLight({});
  EXPECT_EQ(environment.lightCount, Environment::kMostLights);
}

namespace {
// A mesh's triangles, each turned to start at its lowest index (keeping
// its winding), sorted: the same for the same triangles in any order
std::vector<std::array<std::uint32_t, 3>> triangleSet(std::span<const std::uint32_t> indices) {
  std::vector<std::array<std::uint32_t, 3>> set;
  for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
    std::array<std::uint32_t, 3> t{indices[i], indices[i + 1], indices[i + 2]};
    std::ranges::rotate(t, std::ranges::min_element(t));
    set.push_back(t);
  }
  std::ranges::sort(set);
  return set;
}

// An n x n grid of squares sharing their vertices, curved as a saddle
// (height x z), so no two neighbouring triangles are in one plane: strips,
// not quads
struct Grid {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  MeshData data() const { return {vertices, indices}; }
};
Grid bumpyGrid(int n) {
  Grid grid;
  for (int z = 0; z <= n; ++z)
    for (int x = 0; x <= n; ++x)
      grid.vertices.push_back(
          {{static_cast<float>(x), 0.15f * static_cast<float>(x * z), static_cast<float>(z)},
           {0, 1, 0},
           {}});
  auto at = [n](int x, int z) { return static_cast<std::uint32_t>(z * (n + 1) + x); };
  for (int z = 0; z < n; ++z)
    for (int x = 0; x < n; ++x)
      grid.indices.insert(grid.indices.end(), {at(x, z), at(x, z + 1), at(x + 1, z), at(x + 1, z),
                                               at(x, z + 1), at(x + 1, z + 1)});
  return grid;
}
} // namespace

// A box's faces become quads: 6 polygons and 24 vertices where 12
// triangles were 12 and 36; desktops get the same triangles back, winding
// as they did
TEST(PrimitivesTest, FlatFacesBecomeQuads) {
  MeshBuilder builder;
  builder.box({0, 0, 0}, {1, 2, 3});
  const Primitives built = buildPrimitives(builder.data());
  ASSERT_EQ(built.runs.size(), 1u);
  EXPECT_EQ(built.runs[0], (Primitive{Primitive::Kind::quads, 31, 24}));
  EXPECT_EQ(costOf(built.runs), (FrameCounts{6, 24}));
  EXPECT_EQ(costOf(builder.data()), (FrameCounts{6, 24}));

  const MeshData cooked{builder.data().vertices, built.indices, built.runs};
  EXPECT_EQ(triangleSet(triangleList(cooked)), triangleSet(builder.data().indices));
}

// A curved surface chains into strips, each triangle once: fewer vertices
// than separate triangles, the same polygons, and the same triangles back
TEST(PrimitivesTest, CurvedSurfacesBecomeStrips) {
  const Grid grid = bumpyGrid(8); // 128 triangles
  const Primitives built = buildPrimitives(grid.data());
  const FrameCounts cost = costOf(built.runs);
  EXPECT_EQ(cost.polygons, 128u);
  EXPECT_LT(cost.vertices, 128u * 3 / 2); // long strips: near one a triangle
  EXPECT_TRUE(std::ranges::any_of(
      built.runs, [](const Primitive &r) { return r.kind == Primitive::Kind::triangleStrip; }));
  const MeshData cooked{grid.vertices, built.indices, built.runs};
  EXPECT_EQ(triangleSet(triangleList(cooked)), triangleSet(grid.indices));
  // The same mesh, the same runs (the cook's count is the DS's)
  EXPECT_EQ(buildPrimitives(grid.data()).indices, built.indices);
}

// Quads and strips keep to one alpha, so a polygon's alpha (its corners'
// average, the DS having one a polygon) is its run's; the most opaque first
TEST(PrimitivesTest, RunsKeepToOneAlpha) {
  MeshBuilder builder;
  builder.quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, Colour{255, 255, 255, 64});
  builder.quad({0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, Colour{255, 255, 255, 255});
  const Primitives built = buildPrimitives(builder.data());
  ASSERT_EQ(built.runs.size(), 2u);
  EXPECT_EQ(built.runs[0].alpha, 31);
  EXPECT_EQ(built.runs[1].alpha, 8);
  EXPECT_TRUE(hasTranslucentVertices(builder.data()));
  MeshBuilder solid;
  solid.box({}, {1, 1, 1});
  EXPECT_FALSE(hasTranslucentVertices(solid.data()));

  EXPECT_EQ(polygonAlpha({.a = 255}, {.a = 255}, {.a = 255}), 31);
  EXPECT_EQ(polygonAlpha({.a = 0}, {.a = 0}, {.a = 0}), 0);
  EXPECT_EQ(polygonAlpha({.a = 0}, {.a = 255}, {.a = 128}), 16);
}

// Two triangles in one plane that would make a concave quad (an arrowhead)
// stay triangles: the DS draws only convex ones properly
TEST(PrimitivesTest, ConcavePairsStayTriangles) {
  const Vertex vertices[] = {{{0, 0, 0}, {0, 0, 1}, {}},
                             {{2, 0, 0}, {0, 0, 1}, {}},
                             {{1, 0.5f, 0}, {0, 0, 1}, {}},
                             {{1, 2, 0}, {0, 0, 1}, {}}};
  // 0 1 2 and 2 1 3 share the edge 1-2; around them 0 1 3 2, dented at 2
  const std::uint32_t indices[] = {0, 1, 2, 2, 1, 3};
  const Primitives built = buildPrimitives({vertices, indices});
  EXPECT_TRUE(std::ranges::none_of(
      built.runs, [](const Primitive &r) { return r.kind == Primitive::Kind::quads; }));
  EXPECT_EQ(costOf(built.runs).polygons, 2u);
}
