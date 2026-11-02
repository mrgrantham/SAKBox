// The scene layer's shared parts, without a backend: transforms, meshes
// built in code, and the overlay. Built twice: with Scalar as float, and as
// the DS has it (Q16.16).
#include <ScratchPadd/Scene/Scene.hpp>

#include <gtest/gtest.h>

#include <numbers>
#include <type_traits>

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
