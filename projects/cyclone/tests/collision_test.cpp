// Cyclone's collision detection: box contacts, the quick yes/no intersection
// tests, and the bounding-volume tree that finds pairs worth checking.
// Each scene is small enough to work out the right answer by hand.
#include <gtest/gtest.h>

#include <cyclone/collide_coarse.h>
#include <cyclone/cyclone.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <utility>
#include <vector>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

void expectNear(const Vector3 &actual, const Vector3 &expected, double tolerance = kTolerance) {
  EXPECT_NEAR(actual.x, expected.x, tolerance);
  EXPECT_NEAR(actual.y, expected.y, tolerance);
  EXPECT_NEAR(actual.z, expected.z, tolerance);
}

std::unique_ptr<RigidBody> bodyAt(const Vector3 &position) {
  auto body = std::make_unique<RigidBody>();
  body->setMass(1.0);
  Matrix3 inertia;
  inertia.setDiagonal(1, 1, 1);
  body->setInertiaTensor(inertia);
  body->setDamping(1.0, 1.0);
  body->setPosition(position);
  body->setOrientation(1, 0, 0, 0);
  body->setVelocity(0, 0, 0);
  body->setRotation(Vector3());
  body->setAcceleration(Vector3());
  body->setAwake();
  body->calculateDerivedData();
  return body;
}

CollisionBox boxOn(RigidBody &body, const Vector3 &halfSize) {
  CollisionBox box;
  box.body = &body;
  box.halfSize = halfSize;
  box.calculateInternals();
  return box;
}

CollisionSphere sphereOn(RigidBody &body, double radius) {
  CollisionSphere sphere;
  sphere.body = &body;
  sphere.radius = radius;
  sphere.calculateInternals();
  return sphere;
}

CollisionPlane ground() {
  CollisionPlane plane;
  plane.direction = Vector3(0, 1, 0);
  plane.offset = 0;
  return plane;
}

struct Contacts {
  Contact array[16];
  CollisionData data;
  explicit Contacts(unsigned room = 16) {
    data.contactArray = array;
    data.friction = 0.5;
    data.restitution = 0.3;
    data.tolerance = 0.1;
    data.reset(room);
  }
};

} // namespace

// ---------------------------------------------------------------------------
// Box against the ground

// A unit box (half-size 1) sunk 0.1 into the ground touches with its four
// bottom corners, each 0.1 deep, pushed straight up.
TEST(BoxCollisionTest, BoxRestingOnGroundTouchesAtFourCorners) {
  auto body = bodyAt(Vector3(0, 0.9, 0));
  Contacts c;
  ASSERT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground(), &c.data),
            4u);
  EXPECT_EQ(c.data.contactCount, 4u);
  for (unsigned i = 0; i < 4; ++i) {
    const Contact &contact = c.array[i];
    expectNear(contact.contactNormal, Vector3(0, 1, 0));
    EXPECT_NEAR(contact.penetration, 0.1, kTolerance);
    EXPECT_NEAR(std::abs(contact.contactPoint.x), 1.0, kTolerance);
    EXPECT_NEAR(std::abs(contact.contactPoint.z), 1.0, kTolerance);
    // Halfway between the sunken corner (y = -0.1) and the ground (y = 0)
    EXPECT_NEAR(contact.contactPoint.y, -0.05, kTolerance);
    EXPECT_EQ(contact.body[0], body.get());
    EXPECT_EQ(contact.body[1], nullptr);
  }
}

TEST(BoxCollisionTest, BoxAboveGroundDoesntTouch) {
  auto body = bodyAt(Vector3(0, 1.5, 0));
  Contacts c;
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground(), &c.data),
            0u);
  EXPECT_EQ(c.data.contactCount, 0u);
}

// A box balanced on one edge (rotated 45 degrees about z) touches along
// that edge: two corners.
TEST(BoxCollisionTest, BoxOnItsEdgeTouchesAtTwoCorners) {
  auto body = bodyAt(Vector3(0, std::sqrt(2.0) - 0.05, 0));
  const double s = std::sin(M_PI / 8), cos = std::cos(M_PI / 8);
  body->setOrientation(cos, 0, 0, s); // 45 degrees about z
  body->calculateDerivedData();
  Contacts c;
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground(), &c.data),
            2u);
  EXPECT_EQ(c.data.contactCount, 2u);
}

// When the contact array fills up, the contacts written so far are still
// recorded in the collision data.
TEST(BoxCollisionTest, FullContactArrayStillRecordsContacts) {
  auto body = bodyAt(Vector3(0, 0.9, 0));
  Contacts c(2);
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground(), &c.data),
            2u);
  EXPECT_EQ(c.data.contactCount, 2u);
  EXPECT_FALSE(c.data.hasMoreContacts());
}

// ---------------------------------------------------------------------------
// Box against box and sphere

// Two unit boxes overlapping by 0.5 along x: one contact, 0.5 deep, along x.
TEST(BoxCollisionTest, OverlappingBoxesMakeOneContact) {
  auto one = bodyAt(Vector3(0, 0, 0));
  auto two = bodyAt(Vector3(1.5, 0.2, 0.1));
  Contacts c;
  ASSERT_EQ(CollisionDetector::boxAndBox(boxOn(*one, Vector3(1, 1, 1)),
                                         boxOn(*two, Vector3(1, 1, 1)), &c.data),
            1u);
  const Contact &contact = c.array[0];
  EXPECT_NEAR(contact.penetration, 0.5, kTolerance);
  EXPECT_NEAR(std::abs(contact.contactNormal.x), 1.0, kTolerance);
  // Like sphereAndSphere, the normal points from the second body to the first
  EXPECT_LT(contact.contactNormal.x, 0.0);
}

TEST(BoxCollisionTest, SeparateBoxesDontTouch) {
  auto one = bodyAt(Vector3(0, 0, 0));
  auto two = bodyAt(Vector3(2.5, 0, 0));
  Contacts c;
  EXPECT_EQ(CollisionDetector::boxAndBox(boxOn(*one, Vector3(1, 1, 1)),
                                         boxOn(*two, Vector3(1, 1, 1)), &c.data),
            0u);
}

// ---------------------------------------------------------------------------
// The box contact cache: contacts between two boxes kept from frame to frame

namespace {

// A 2 m cube resting on another, sunk 0.01 m into it, and the cache between
// them. tiltTowards() rocks the top cube 1 degree so one bottom corner dips
// lowest, as it does when a resting box rocks; frame() collides them.
struct CubeOnCube {
  std::unique_ptr<RigidBody> bottomBody = bodyAt(Vector3(0, 0, 0));
  std::unique_ptr<RigidBody> topBody = bodyAt(Vector3(0, 1.99, 0));
  CollisionBox bottom = boxOn(*bottomBody, Vector3(1, 1, 1));
  CollisionBox top = boxOn(*topBody, Vector3(1, 1, 1));
  BoxContactCache cache;
  Contacts c;

  void place(const Vector3 &position, const Quaternion &orientation = Quaternion()) {
    topBody->setPosition(position);
    topBody->setOrientation(orientation);
    topBody->calculateDerivedData();
    top.calculateInternals();
  }

  // Tips the top cube so its bottom corner at (x, -1, z) (x, z = +-1) dips
  void tiltTowards(double x, double z) {
    const double half = 0.5 * R_PI / 180, s = std::sin(half) / std::sqrt(2.0);
    place(Vector3(0, 1.99, 0), Quaternion(std::cos(half), z * s, 0, -x * s));
  }

  unsigned frame() {
    c.data.reset(16);
    return cache.boxAndBox(bottom, top, &c.data);
  }
};

} // namespace

// Resting level, the plain detector finds one contact, at one corner. The
// cache finds the whole face at once (SAKBox; it used to gather a corner
// each frame as the top cube rocked): all four corners on the first frame,
// each as deep as the cubes overlap.
TEST(BoxContactCacheTest, HoldsARestingBoxAtFourCornersAtOnce) {
  CubeOnCube cubes;
  {
    Contacts c;
    EXPECT_EQ(CollisionDetector::boxAndBox(cubes.bottom, cubes.top, &c.data), 1u);
  }
  ASSERT_EQ(cubes.frame(), 4u);
  std::set<std::pair<int, int>> corners;
  for (unsigned i = 0; i < 4; ++i) {
    const Contact &contact = cubes.c.array[i];
    EXPECT_NEAR(contact.penetration, 0.01, 1e-9);
    EXPECT_NEAR(std::abs(contact.contactNormal.y), 1, 1e-9);
    EXPECT_NEAR(contact.contactPoint.y, 0.995, 0.005 + 1e-9); // either cube's corner
    corners.insert({contact.contactPoint.x > 0, contact.contactPoint.z > 0});
  }
  EXPECT_EQ(corners.size(), 4u);
}

// Rocking, it's still held at all four corners of the face, whichever
// dips. (Tilted, each cube's corners are a little apart across the face, so
// there can be a contact for each.)
TEST(BoxContactCacheTest, HoldsARockingBoxAtFourCorners) {
  CubeOnCube cubes;
  for (auto [x, z] :
       {std::pair{1.0, 1.0}, std::pair{-1.0, 1.0}, std::pair{-1.0, -1.0}, std::pair{1.0, -1.0}}) {
    SCOPED_TRACE(testing::Message() << "dipping corner " << x << ", " << z);
    cubes.tiltTowards(x, z);
    const unsigned found = cubes.frame();
    ASSERT_GE(found, 4u);
    std::set<std::pair<int, int>> corners;
    for (unsigned i = 0; i < found; ++i)
      corners.insert({cubes.c.array[i].contactPoint.x > 0, cubes.c.array[i].contactPoint.z > 0});
    EXPECT_EQ(corners.size(), 4u);
  }
}

// Lifted, but within the tolerance (0.1 m), the kept contacts stay; more
// than the tolerance clear they're dropped, and found again when the cube
// comes back.
TEST(BoxContactCacheTest, DropsContactsOnceTheBoxesMoveApart) {
  CubeOnCube cubes;
  cubes.frame();
  cubes.place(Vector3(0, 1.99 + 0.05, 0)); // within the tolerance: kept
  EXPECT_EQ(cubes.frame(), 4u);
  cubes.place(Vector3(0, 2.2, 0));
  EXPECT_EQ(cubes.frame(), 0u);
  cubes.place(Vector3(0, 1.99 + 0.05, 0)); // apart, and nothing kept
  EXPECT_EQ(cubes.frame(), 0u);
  cubes.place(Vector3(0, 1.99, 0));
  EXPECT_EQ(cubes.frame(), 4u);
}

// Slid half off the bottom cube, the top cube is held where the faces
// still overlap: no contact is beyond the bottom cube's edge, and the top
// cube's middle (x = 1.5) is no longer over the contacts, so it tips off.
TEST(BoxContactCacheTest, DropsCornersThatSlideOffTheFace) {
  CubeOnCube cubes;
  cubes.frame();
  cubes.place(Vector3(1.5, 1.99, 0));
  const unsigned found = cubes.frame();
  EXPECT_EQ(found, 4u);
  for (unsigned i = 0; i < found; ++i) {
    EXPECT_LE(cubes.c.array[i].contactPoint.x, 1.0 + 1e-9);
    EXPECT_GE(cubes.c.array[i].contactPoint.x, 0.5 - 1e-9);
  }
}

// Forgetting a box (or clearing the cache) drops what was kept for it: the
// cubes, lifted within the tolerance, then have no contact.
TEST(BoxContactCacheTest, ForgetAndClear) {
  CubeOnCube cubes;
  cubes.frame();
  cubes.place(Vector3(0, 1.99 + 0.05, 0));
  EXPECT_EQ(cubes.frame(), 4u);
  cubes.cache.forget(cubes.top);
  EXPECT_EQ(cubes.frame(), 0u);
  cubes.place(Vector3(0, 1.99, 0));
  cubes.frame();
  cubes.place(Vector3(0, 1.99 + 0.05, 0));
  cubes.cache.clear();
  EXPECT_EQ(cubes.frame(), 0u);
}

// The whole area where two faces touch, on the first frame, for a box set
// on another turned, and hanging off it (Catto, GDC 2007; Gregorius,
// GDC 2015): the corners of that area (up to eight), between the faces,
// with the top cube's middle over them (so it's held still).
TEST(BoxContactCacheTest, FindsTheAreaWhereTheFacesTouch) {
  struct Case {
    const char *name;
    Vector3 position;
    double turn; // about y, radians
  };
  for (const Case &test : {Case{"turned 45 degrees", Vector3(0, 1.99, 0), R_PI / 4},
                           Case{"turned 10 degrees", Vector3(0, 1.99, 0), R_PI / 18},
                           Case{"hanging 0.6 off", Vector3(0.6, 1.99, 0.3), 0},
                           Case{"smaller box", Vector3(0.2, 1.49, -0.1), 0.3}}) {
    SCOPED_TRACE(test.name);
    CubeOnCube cubes;
    if (test.position.y < 1.9) {
      cubes.top.halfSize = Vector3(0.5, 0.5, 0.5);
    }
    cubes.place(test.position, Quaternion(std::cos(test.turn / 2), 0, std::sin(test.turn / 2), 0));
    const unsigned found = cubes.frame();
    ASSERT_GE(found, 4u);
    double minX = 1e9, maxX = -1e9, minZ = 1e9, maxZ = -1e9;
    for (unsigned i = 0; i < found; ++i) {
      const Contact &contact = cubes.c.array[i];
      EXPECT_NEAR(std::abs(contact.contactNormal.y), 1, 1e-6);
      EXPECT_NEAR(contact.penetration, 0.01, 1e-6);
      EXPECT_LE(std::abs(contact.contactPoint.x), 1 + 1e-6); // on the bottom cube's face
      EXPECT_LE(std::abs(contact.contactPoint.z), 1 + 1e-6);
      minX = std::min<double>(minX, contact.contactPoint.x);
      maxX = std::max<double>(maxX, contact.contactPoint.x);
      minZ = std::min<double>(minZ, contact.contactPoint.z);
      maxZ = std::max<double>(maxZ, contact.contactPoint.z);
    }
    EXPECT_LT(minX, test.position.x);
    EXPECT_GT(maxX, test.position.x);
    EXPECT_LT(minZ, test.position.z);
    EXPECT_GT(maxZ, test.position.z);
  }
}

// Two cubes meeting edge to edge (one turned 45 degrees about z, the other
// about x, so their edges cross): the cache reports the plain detector's
// edge contact, keeps it, and drops it once they part.
TEST(BoxContactCacheTest, KeepsEdgeContactsToo) {
  const double half = R_PI / 8, root2 = std::sqrt(2.0);
  auto lowerBody = bodyAt(Vector3(0, 0, 0));
  lowerBody->setOrientation(Quaternion(std::cos(half), 0, 0, std::sin(half)));
  lowerBody->calculateDerivedData();
  auto upperBody = bodyAt(Vector3(0, 2 * root2 - 0.05, 0));
  upperBody->setOrientation(Quaternion(std::cos(half), std::sin(half), 0, 0));
  upperBody->calculateDerivedData();
  CollisionBox lower = boxOn(*lowerBody, Vector3(1, 1, 1));
  CollisionBox upper = boxOn(*upperBody, Vector3(1, 1, 1));

  Contacts plain;
  BoxContactFeature touching;
  ASSERT_EQ(CollisionDetector::boxAndBox(lower, upper, &plain.data, &touching), 1u);
  EXPECT_EQ(touching.kind, BoxContactFeature::EdgeOnEdge);
  EXPECT_NEAR(plain.array[0].penetration, 0.05, 1e-9);

  BoxContactCache cache;
  for (int frame = 0; frame < 2; ++frame) {
    Contacts c;
    ASSERT_EQ(cache.boxAndBox(lower, upper, &c.data), 1u);
    EXPECT_NEAR(c.array[0].penetration, 0.05, 1e-9);
    expectNear(c.array[0].contactPoint, plain.array[0].contactPoint);
  }

  upperBody->setPosition(0, 2 * root2 + 0.2, 0);
  upperBody->calculateDerivedData();
  upper.calculateInternals();
  Contacts apart;
  EXPECT_EQ(cache.boxAndBox(lower, upper, &apart.data), 0u);
}

// A sphere of radius 1 whose centre is 1.5 from a unit box's face: it sinks
// 0.5 into that face, touching at the face.
TEST(BoxCollisionTest, SphereAgainstBoxFace) {
  auto boxBody = bodyAt(Vector3(0, 0, 0));
  auto ball = bodyAt(Vector3(0, 1.5, 0));
  Contacts c;
  ASSERT_EQ(CollisionDetector::boxAndSphere(boxOn(*boxBody, Vector3(1, 1, 1)), sphereOn(*ball, 1),
                                            &c.data),
            1u);
  const Contact &contact = c.array[0];
  EXPECT_NEAR(contact.penetration, 0.5, kTolerance);
  EXPECT_NEAR(std::abs(contact.contactNormal.y), 1.0, kTolerance);
  expectNear(contact.contactPoint, Vector3(0, 1, 0)); // on the box's top face
}

TEST(BoxCollisionTest, SphereClearOfBoxDoesntTouch) {
  auto boxBody = bodyAt(Vector3(0, 0, 0));
  auto ball = bodyAt(Vector3(2.0, 2.0, 0)); // beyond the corner by more than its radius
  Contacts c;
  EXPECT_EQ(CollisionDetector::boxAndSphere(boxOn(*boxBody, Vector3(1, 1, 1)), sphereOn(*ball, 1),
                                            &c.data),
            0u);
}

// ---------------------------------------------------------------------------
// The quick yes/no tests agree with the full contact generation

TEST(IntersectionTest, QuickTestsAgreeWithContactGeneration) {
  for (double y : {0.5, 0.9, 0.99, 1.01, 2.0}) {
    auto body = bodyAt(Vector3(0, y, 0));
    Contacts c;
    const bool touching =
        CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground(), &c.data) > 0;
    EXPECT_EQ(IntersectionTests::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)), ground()),
              touching)
        << "y " << y;
    Contacts s;
    const bool sphereTouching =
        CollisionDetector::sphereAndHalfSpace(sphereOn(*body, 1), ground(), &s.data) > 0;
    EXPECT_EQ(IntersectionTests::sphereAndHalfSpace(sphereOn(*body, 1), ground()), sphereTouching)
        << "y " << y;
  }
  for (double x : {1.0, 1.9, 2.1, 3.0}) {
    auto one = bodyAt(Vector3(0, 0, 0));
    auto two = bodyAt(Vector3(x, 0, 0));
    Contacts c;
    const bool touching = CollisionDetector::boxAndBox(boxOn(*one, Vector3(1, 1, 1)),
                                                       boxOn(*two, Vector3(1, 1, 1)), &c.data) > 0;
    EXPECT_EQ(
        IntersectionTests::boxAndBox(boxOn(*one, Vector3(1, 1, 1)), boxOn(*two, Vector3(1, 1, 1))),
        touching)
        << "x " << x;
    EXPECT_EQ(IntersectionTests::sphereAndSphere(sphereOn(*one, 1), sphereOn(*two, 1)), x < 2.0)
        << "x " << x;
  }
}

// ---------------------------------------------------------------------------
// The bounding-volume tree: finding pairs worth checking

namespace {

// Builds a tree over bodies at `positions` (each in a unit bounding sphere),
// inserted in order, and returns the pairs it reports as possibly touching,
// as sorted index pairs.
std::vector<std::pair<int, int>> potentialPairs(const std::vector<Vector3> &positions) {
  std::vector<std::unique_ptr<RigidBody>> bodies;
  bodies.reserve(positions.size());
  for (const Vector3 &p : positions)
    bodies.push_back(bodyAt(p));
  BVHNode<BoundingSphere> root(nullptr, BoundingSphere(positions[0], 1), bodies[0].get());
  for (size_t i = 1; i < bodies.size(); ++i)
    root.insert(bodies[i].get(), BoundingSphere(positions[i], 1));

  PotentialContact contacts[32];
  const unsigned found = root.getPotentialContacts(contacts, 32);
  auto indexOf = [&](RigidBody *b) {
    return static_cast<int>(std::find_if(bodies.begin(), bodies.end(),
                                         [&](const auto &owned) { return owned.get() == b; }) -
                            bodies.begin());
  };
  std::vector<std::pair<int, int>> pairs;
  for (unsigned i = 0; i < found; ++i) {
    int a = indexOf(contacts[i].body[0]), b = indexOf(contacts[i].body[1]);
    pairs.emplace_back(std::min(a, b), std::max(a, b));
  }
  std::sort(pairs.begin(), pairs.end());
  return pairs;
}

} // namespace

TEST(BoundingVolumeTest, SpheresOverlap) {
  const BoundingSphere origin(Vector3(0, 0, 0), 1), near(Vector3(1.5, 0, 0), 1),
      far(Vector3(2.5, 0, 0), 1);
  EXPECT_TRUE(origin.overlaps(&near));
  EXPECT_FALSE(origin.overlaps(&far));
}

// Two nearby bodies are reported as a candidate pair and a distant one is
// not, whatever order they were inserted in.
TEST(BoundingVolumeTest, TreeFindsNearbyPairsInAnyOrder) {
  const Vector3 nearA(0, 0, 0), nearB(1.5, 0, 0), far(50, 0, 0);
  using Pairs = std::vector<std::pair<int, int>>;
  EXPECT_EQ(potentialPairs({nearA, nearB, far}), (Pairs{{0, 1}}));
  EXPECT_EQ(potentialPairs({far, nearA, nearB}), (Pairs{{1, 2}}));
  EXPECT_EQ(potentialPairs({nearA, far, nearB}), (Pairs{{0, 2}}));
}

TEST(BoundingVolumeTest, TreeReportsNothingForSpreadOutBodies) {
  EXPECT_TRUE(
      potentialPairs({Vector3(0, 0, 0), Vector3(10, 0, 0), Vector3(0, 10, 0), Vector3(0, 0, 10)})
          .empty());
}
