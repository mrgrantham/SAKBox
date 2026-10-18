// GRayTraceTriMesh::orientTriangles: making a mesh's triangles face the same
// way as their neighbors (revived from code dead upstream since 2011).
#include <gtest/gtest.h>

#include <WClasses/G3D.h>
#include <WClasses/GRayTrace.h>

#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

using namespace GClasses;

namespace {

using Triangle = std::vector<size_t>;

std::vector<Triangle> triangles(GRayTraceTriMesh &mesh) {
  std::vector<Triangle> out;
  for (size_t i = 0; i < mesh.triangleCount(); i++) {
    size_t a, b, c;
    mesh.triangle(i, &a, &b, &c);
    out.push_back({a, b, c});
  }
  return out;
}

// Consistent orientation: no side is walked in the same direction by two
// triangles.
bool consistentlyOriented(GRayTraceTriMesh &mesh) {
  std::map<std::pair<size_t, size_t>, int> directedSides;
  for (const Triangle &t : triangles(mesh))
    for (size_t j = 0; j < 3; j++)
      if (++directedSides[{t[j], t[(j + 1) % 3]}] > 1)
        return false;
  return true;
}

// Whether triangle i's normal (right-hand rule) points away from `inside`.
bool facesAwayFrom(GRayTraceTriMesh &mesh, size_t i, const double inside[3]) {
  const G3DReal *p0 = mesh.vertex(i, 0)->m_vals,
                *p1 = mesh.vertex(i, 1)->m_vals,
                *p2 = mesh.vertex(i, 2)->m_vals;
  const double e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
  const double e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
  const double normal[3] = {e1[1] * e2[2] - e1[2] * e2[1],
                            e1[2] * e2[0] - e1[0] * e2[2],
                            e1[0] * e2[1] - e1[1] * e2[0]};
  const double out[3] = {p0[0] - inside[0], p0[1] - inside[1],
                         p0[2] - inside[2]};
  return normal[0] * out[0] + normal[1] * out[1] + normal[2] * out[2] > 0;
}

// A unit cube from (x, 0, 0): two triangles per face, all facing outward
// except the local triangle numbers in `reversed`.
void addCube(GRayTraceTriMesh &mesh, double x,
             const std::set<size_t> &reversed = {}) {
  const size_t base = mesh.pointCount();
  for (int i = 0; i < 8; i++) // point i's coordinates are its bits
    mesh.addPoint(G3DVector(x + (i & 1), (i >> 1) & 1, (i >> 2) & 1));
  // Each face's corners, counter-clockwise seen from outside
  const size_t faces[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4},
                              {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
  size_t local = 0;
  auto add = [&](size_t a, size_t b, size_t c) {
    if (reversed.count(local++))
      mesh.addTriangle(base + a, base + c, base + b);
    else
      mesh.addTriangle(base + a, base + b, base + c);
  };
  for (const auto &f : faces) {
    add(f[0], f[1], f[2]);
    add(f[0], f[2], f[3]);
  }
}

} // namespace

// The cube helper itself is right: consistent, and every face outward.
TEST(MeshOrientationTest, TestCubeFacesOutward) {
  GRayTraceTriMesh mesh(nullptr);
  addCube(mesh, 0);
  ASSERT_EQ(mesh.triangleCount(), 12u);
  EXPECT_TRUE(consistentlyOriented(mesh));
  const double centre[3] = {0.5, 0.5, 0.5};
  for (size_t i = 0; i < 12; i++)
    EXPECT_TRUE(facesAwayFrom(mesh, i, centre)) << "triangle " << i;
}

TEST(MeshOrientationTest, AlreadyConsistentMeshIsUntouched) {
  GRayTraceTriMesh mesh(nullptr);
  addCube(mesh, 0);
  const std::vector<Triangle> before = triangles(mesh);
  size_t flipped = 99;
  EXPECT_TRUE(mesh.orientTriangles(&flipped));
  EXPECT_EQ(flipped, 0u);
  EXPECT_EQ(triangles(mesh), before);
}

// Three reversed triangles are flipped back. The first triangle keeps its
// orientation, so the whole cube faces outward again.
TEST(MeshOrientationTest, FlippedTrianglesAreTurnedBack) {
  GRayTraceTriMesh mesh(nullptr);
  addCube(mesh, 0, {3, 4, 9});
  ASSERT_FALSE(consistentlyOriented(mesh));
  size_t flipped = 0;
  EXPECT_TRUE(mesh.orientTriangles(&flipped));
  EXPECT_EQ(flipped, 3u);
  EXPECT_TRUE(consistentlyOriented(mesh));
  const double centre[3] = {0.5, 0.5, 0.5};
  for (size_t i = 0; i < 12; i++)
    EXPECT_TRUE(facesAwayFrom(mesh, i, centre)) << "triangle " << i;
}

// Separate pieces are oriented independently, each following its own first
// triangle: a cube that faces inward throughout stays that way.
TEST(MeshOrientationTest, EachPieceFollowsItsFirstTriangle) {
  GRayTraceTriMesh mesh(nullptr);
  addCube(mesh, 0, {5});
  std::set<size_t> all;
  for (size_t i = 0; i < 12; i++)
    all.insert(i);
  addCube(mesh, 3, all); // all inward
  size_t flipped = 0;
  EXPECT_TRUE(mesh.orientTriangles(&flipped));
  EXPECT_EQ(flipped, 1u); // just the first cube's odd one out
  EXPECT_TRUE(consistentlyOriented(mesh));
  const double centre1[3] = {0.5, 0.5, 0.5}, centre2[3] = {3.5, 0.5, 0.5};
  for (size_t i = 0; i < 12; i++) {
    EXPECT_TRUE(facesAwayFrom(mesh, i, centre1)) << "triangle " << i;
    EXPECT_FALSE(facesAwayFrom(mesh, 12 + i, centre2)) << "triangle " << 12 + i;
  }
}

// A Moebius strip has only one side, so no orientation is consistent.
TEST(MeshOrientationTest, MoebiusStripIsReported) {
  GRayTraceTriMesh mesh(nullptr);
  const size_t n = 12;
  for (size_t i = 0; i < n; i++) {
    const double angle = 2 * M_PI * i / n, twist = M_PI * i / n;
    for (double w : {-0.3, 0.3}) {
      const double r = 1 + w * std::cos(twist);
      mesh.addPoint(G3DVector(r * std::cos(angle), r * std::sin(angle),
                              w * std::sin(twist)));
    }
  }
  for (size_t i = 0; i < n; i++) {
    size_t a = 2 * i, b = 2 * i + 1, c = 2 * ((i + 1) % n),
           d = 2 * ((i + 1) % n) + 1;
    if (i == n - 1)
      std::swap(c, d); // the half twist joins the ends the other way round
    mesh.addTriangle(a, b, d);
    mesh.addTriangle(a, d, c);
  }
  EXPECT_FALSE(mesh.orientTriangles());
}

// Waffles' own cylinder generator, with and without end caps.
TEST(MeshOrientationTest, GeneratedCylinderIsConsistent) {
  for (bool caps : {false, true}) {
    SCOPED_TRACE(testing::Message() << "end caps " << caps);
    G3DVector bottom(0, 0, 0), top(0, 0, 2);
    GRayTracePhysicalMaterial material; // makeCylinder needs one
    std::unique_ptr<GRayTraceTriMesh> mesh(GRayTraceTriMesh::makeCylinder(
        &material, &bottom, &top, 1.0, 12, caps));
    EXPECT_TRUE(consistentlyOriented(*mesh));
    // A cylinder is convex, so every triangle should face away from its
    // middle
    const double middle[3] = {0, 0, 1};
    for (size_t i = 0; i < mesh->triangleCount(); i++)
      EXPECT_TRUE(facesAwayFrom(*mesh, i, middle)) << "triangle " << i;
    size_t flipped = 99;
    EXPECT_TRUE(mesh->orientTriangles(&flipped));
    EXPECT_EQ(flipped, 0u);
  }
}
