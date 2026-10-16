// Cyclone's collision detection: box contacts, the quick yes/no intersection
// tests, and the bounding-volume tree that finds pairs worth checking.
// Each scene is small enough to work out the right answer by hand.
#include <gtest/gtest.h>

#include <cyclone/collide_coarse.h>
#include <cyclone/cyclone.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

void expectNear(const Vector3 &actual, const Vector3 &expected,
                double tolerance = kTolerance) {
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
  ASSERT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                               ground(), &c.data),
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
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                               ground(), &c.data),
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
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                               ground(), &c.data),
            2u);
  EXPECT_EQ(c.data.contactCount, 2u);
}

// When the contact array fills up, the contacts written so far are still
// recorded in the collision data.
TEST(BoxCollisionTest, FullContactArrayStillRecordsContacts) {
  auto body = bodyAt(Vector3(0, 0.9, 0));
  Contacts c(2);
  EXPECT_EQ(CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                               ground(), &c.data),
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
                                         boxOn(*two, Vector3(1, 1, 1)),
                                         &c.data),
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
                                         boxOn(*two, Vector3(1, 1, 1)),
                                         &c.data),
            0u);
}

// A sphere of radius 1 whose centre is 1.5 from a unit box's face: it sinks
// 0.5 into that face, touching at the face.
TEST(BoxCollisionTest, SphereAgainstBoxFace) {
  auto boxBody = bodyAt(Vector3(0, 0, 0));
  auto ball = bodyAt(Vector3(0, 1.5, 0));
  Contacts c;
  ASSERT_EQ(CollisionDetector::boxAndSphere(boxOn(*boxBody, Vector3(1, 1, 1)),
                                            sphereOn(*ball, 1), &c.data),
            1u);
  const Contact &contact = c.array[0];
  EXPECT_NEAR(contact.penetration, 0.5, kTolerance);
  EXPECT_NEAR(std::abs(contact.contactNormal.y), 1.0, kTolerance);
  expectNear(contact.contactPoint, Vector3(0, 1, 0)); // on the box's top face
}

TEST(BoxCollisionTest, SphereClearOfBoxDoesntTouch) {
  auto boxBody = bodyAt(Vector3(0, 0, 0));
  auto ball =
      bodyAt(Vector3(2.0, 2.0, 0)); // beyond the corner by more than its radius
  Contacts c;
  EXPECT_EQ(CollisionDetector::boxAndSphere(boxOn(*boxBody, Vector3(1, 1, 1)),
                                            sphereOn(*ball, 1), &c.data),
            0u);
}

// ---------------------------------------------------------------------------
// The quick yes/no tests agree with the full contact generation

TEST(IntersectionTest, QuickTestsAgreeWithContactGeneration) {
  for (double y : {0.5, 0.9, 0.99, 1.01, 2.0}) {
    auto body = bodyAt(Vector3(0, y, 0));
    Contacts c;
    const bool touching =
        CollisionDetector::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                           ground(), &c.data) > 0;
    EXPECT_EQ(IntersectionTests::boxAndHalfSpace(boxOn(*body, Vector3(1, 1, 1)),
                                                 ground()),
              touching)
        << "y " << y;
    Contacts s;
    const bool sphereTouching = CollisionDetector::sphereAndHalfSpace(
                                    sphereOn(*body, 1), ground(), &s.data) > 0;
    EXPECT_EQ(
        IntersectionTests::sphereAndHalfSpace(sphereOn(*body, 1), ground()),
        sphereTouching)
        << "y " << y;
  }
  for (double x : {1.0, 1.9, 2.1, 3.0}) {
    auto one = bodyAt(Vector3(0, 0, 0));
    auto two = bodyAt(Vector3(x, 0, 0));
    Contacts c;
    const bool touching = CollisionDetector::boxAndBox(
                              boxOn(*one, Vector3(1, 1, 1)),
                              boxOn(*two, Vector3(1, 1, 1)), &c.data) > 0;
    EXPECT_EQ(IntersectionTests::boxAndBox(boxOn(*one, Vector3(1, 1, 1)),
                                           boxOn(*two, Vector3(1, 1, 1))),
              touching)
        << "x " << x;
    EXPECT_EQ(IntersectionTests::sphereAndSphere(sphereOn(*one, 1),
                                                 sphereOn(*two, 1)),
              x < 2.0)
        << "x " << x;
  }
}

// ---------------------------------------------------------------------------
// The bounding-volume tree: finding pairs worth checking

namespace {

// Builds a tree over bodies at `positions` (each in a unit bounding sphere),
// inserted in order, and returns the pairs it reports as possibly touching,
// as sorted index pairs.
std::vector<std::pair<int, int>>
potentialPairs(const std::vector<Vector3> &positions) {
  std::vector<std::unique_ptr<RigidBody>> bodies;
  for (const Vector3 &p : positions)
    bodies.push_back(bodyAt(p));
  BVHNode<BoundingSphere> root(nullptr, BoundingSphere(positions[0], 1),
                               bodies[0].get());
  for (size_t i = 1; i < bodies.size(); ++i)
    root.insert(bodies[i].get(), BoundingSphere(positions[i], 1));

  PotentialContact contacts[32];
  const unsigned found = root.getPotentialContacts(contacts, 32);
  auto indexOf = [&](RigidBody *b) {
    return static_cast<int>(
        std::find_if(bodies.begin(), bodies.end(),
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
  EXPECT_TRUE(potentialPairs({Vector3(0, 0, 0), Vector3(10, 0, 0),
                              Vector3(0, 10, 0), Vector3(0, 0, 10)})
                  .empty());
}
