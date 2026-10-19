// Tests for Cyclone's maths, integration and contact generation (the plain
// double-precision build). Written against the original engine before SAKBox
// reformatted and cleaned it up, so they also guard those changes.
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>

#include <cmath>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

void expectNear(const Vector3 &actual, const Vector3 &expected, double tolerance = kTolerance) {
  EXPECT_NEAR(actual.x, expected.x, tolerance);
  EXPECT_NEAR(actual.y, expected.y, tolerance);
  EXPECT_NEAR(actual.z, expected.z, tolerance);
}

// A quaternion rotating `angle` radians about the unit vector `axis`.
Quaternion rotation(double angle, const Vector3 &axis) {
  const double s = std::sin(angle / 2);
  return Quaternion(std::cos(angle / 2), axis.x * s, axis.y * s, axis.z * s);
}

// Where `orientation` turns `direction`, using the conversion rigid bodies use
// for their transform (body.cpp).
Vector3 rotated(const Quaternion &orientation, const Vector3 &direction);

// An awake rigid body at rest at the origin with unit mass and inertia.
RigidBody makeBody(const Vector3 &position = Vector3()) {
  RigidBody body;
  body.setMass(1.0);
  Matrix3 inertia;
  inertia.setDiagonal(1, 1, 1);
  body.setInertiaTensor(inertia);
  body.setDamping(1.0, 1.0);
  body.setPosition(position);
  body.setOrientation(1, 0, 0, 0);
  body.setVelocity(0, 0, 0);
  body.setRotation(Vector3());
  body.setAcceleration(Vector3());
  body.setCanSleep(false);
  body.setAwake();
  body.calculateDerivedData();
  return body;
}

Vector3 rotated(const Quaternion &orientation, const Vector3 &direction) {
  RigidBody body = makeBody();
  body.setOrientation(orientation);
  body.calculateDerivedData();
  return body.getDirectionInWorldSpace(direction);
}

} // namespace

// ---------------------------------------------------------------------------
// Vector3

TEST(Vector3Test, Arithmetic) {
  Vector3 a(1, 2, 3), b(4, -5, 6);
  expectNear(a + b, Vector3(5, -3, 9));
  expectNear(a - b, Vector3(-3, 7, -3));
  expectNear(a * 2.0, Vector3(2, 4, 6));
  expectNear(a.componentProduct(b), Vector3(4, -10, 18));

  Vector3 c = a;
  c.addScaledVector(b, 0.5);
  expectNear(c, Vector3(3, -0.5, 6));
}

TEST(Vector3Test, ScalarAndVectorProducts) {
  Vector3 a(1, 2, 3), b(4, -5, 6);
  EXPECT_DOUBLE_EQ(a * b, 12.0); // scalar product
  EXPECT_DOUBLE_EQ(a.scalarProduct(b), 12.0);
  expectNear(Vector3(1, 0, 0) % Vector3(0, 1, 0), Vector3(0, 0, 1)); // vector product
  expectNear(a.vectorProduct(b), Vector3(27, 6, -13));
  EXPECT_NEAR((a % b) * a, 0.0, kTolerance); // perpendicular to both
}

TEST(Vector3Test, MagnitudeAndNormalise) {
  Vector3 v(3, 4, 12);
  EXPECT_DOUBLE_EQ(v.magnitude(), 13.0);
  EXPECT_DOUBLE_EQ(v.squareMagnitude(), 169.0);
  v.normalise();
  EXPECT_NEAR(v.magnitude(), 1.0, kTolerance);
  expectNear(v, Vector3(3.0 / 13, 4.0 / 13, 12.0 / 13));

  Vector3 zero;
  zero.normalise(); // left as zero rather than dividing by zero
  expectNear(zero, Vector3());
}

// ---------------------------------------------------------------------------
// Quaternion, Matrix3, Matrix4

TEST(QuaternionTest, MultiplyAndNormalise) {
  Quaternion q = rotation(0.7, Vector3(0, 1, 0));
  Quaternion identity(1, 0, 0, 0);
  Quaternion product = identity;
  product *= q;
  EXPECT_NEAR(product.r, q.r, kTolerance);
  EXPECT_NEAR(product.j, q.j, kTolerance);

  Quaternion unnormalised(2, 0, 0, 2);
  unnormalised.normalise();
  EXPECT_NEAR(unnormalised.r, std::sqrt(0.5), kTolerance);
  EXPECT_NEAR(unnormalised.k, std::sqrt(0.5), kTolerance);

  Quaternion zero(0, 0, 0, 0);
  zero.normalise(); // becomes the no-rotation quaternion
  EXPECT_DOUBLE_EQ(zero.r, 1.0);
}

TEST(QuaternionTest, RotatesVectors) {
  const Quaternion quarterTurn = rotation(M_PI / 2, Vector3(0, 0, 1));
  expectNear(rotated(quarterTurn, Vector3(1, 0, 0)), Vector3(0, 1, 0));
  expectNear(rotated(quarterTurn, Vector3(0, 1, 0)), Vector3(-1, 0, 0));
}

// The matrix setters rotate the same way as rigid bodies (they used to build
// the transpose, rotating the opposite way).
TEST(QuaternionTest, MatrixSettersMatchBodyTransform) {
  const Quaternion q = rotation(0.8, Vector3(0.48, 0.6, 0.64));
  RigidBody body = makeBody(Vector3(1, 2, 3));
  body.setOrientation(q);
  body.calculateDerivedData();
  const Vector3 v(0.3, -1.2, 2.0);

  Matrix3 m3;
  m3.setOrientation(q);
  expectNear(m3.transform(v), body.getDirectionInWorldSpace(v));

  Matrix4 m4;
  m4.setOrientationAndPos(q, Vector3(1, 2, 3));
  expectNear(m4.transform(v), body.getPointInWorldSpace(v));

  Matrix3 quarterTurn;
  quarterTurn.setOrientation(rotation(M_PI / 2, Vector3(0, 0, 1)));
  expectNear(quarterTurn.transform(Vector3(1, 0, 0)), Vector3(0, 1, 0));
}

TEST(QuaternionTest, AddScaledVectorIntegratesAngularVelocity) {
  // Spin at 1 rad/s about z for pi/2 seconds in small steps
  Quaternion q(1, 0, 0, 0);
  const int steps = 10000;
  for (int i = 0; i < steps; ++i) {
    q.addScaledVector(Vector3(0, 0, 1), (M_PI / 2) / steps);
    q.normalise();
  }
  expectNear(rotated(q, Vector3(1, 0, 0)), Vector3(0, 1, 0), 1e-3);
}

TEST(Matrix3Test, InverseAndTranspose) {
  Matrix3 m(2, 1, 0, 0, 3, 1, 1, 0, 4);
  const Vector3 v(1, -2, 0.5);
  expectNear(m.inverse().transform(m.transform(v)), v);
  expectNear(m.transform(m.inverse().transform(v)), v);
  expectNear(m.transpose().transform(v), m.transformTranspose(v));
  expectNear((m * m.inverse()).transform(v), v);
}

TEST(Matrix4Test, TransformRoundTrips) {
  Matrix4 m;
  m.setOrientationAndPos(rotation(0.9, Vector3(0, 0.6, 0.8)), Vector3(5, -1, 2));
  const Vector3 point(1, 2, 3), direction(0, 0, 1);
  expectNear(m.transformInverse(m.transform(point)), point);
  expectNear(m.transformInverseDirection(m.transformDirection(direction)), direction);
  expectNear(m.inverse().transform(m.transform(point)), point);
  expectNear(m.getAxisVector(3), Vector3(5, -1, 2)); // translation column
  // Directions ignore the translation
  EXPECT_NEAR(m.transformDirection(direction).magnitude(), 1.0, kTolerance);
}

TEST(Matrix4Test, ProductComposesTransforms) {
  Matrix4 a, b;
  a.setOrientationAndPos(rotation(0.4, Vector3(1, 0, 0)), Vector3(1, 0, 0));
  b.setOrientationAndPos(rotation(-1.1, Vector3(0, 0, 1)), Vector3(0, 2, -3));
  const Vector3 v(0.5, 0.25, -2);
  expectNear((a * b).transform(v), a.transform(b.transform(v)));
}

// ---------------------------------------------------------------------------
// Integration

// Cyclone's integrator updates velocity and then position (semi-implicit
// Euler), so after n steps of dt under constant acceleration a:
//   v = v0 + n a dt,   x = x0 + n dt v0 + a dt^2 n (n + 1) / 2
TEST(RigidBodyTest, FreeFlightMatchesIntegratorClosedForm) {
  RigidBody body = makeBody();
  body.setVelocity(3, 0, 0);
  body.setAcceleration(Vector3(0, -10, 0));
  const int n = 100;
  const double dt = 0.01;
  for (int i = 0; i < n; ++i) {
    body.integrate(dt);
  }
  expectNear(body.getVelocity(), Vector3(3, -10 * n * dt, 0));
  expectNear(body.getPosition(), Vector3(3 * n * dt, -10 * dt * dt * n * (n + 1) / 2, 0));
}

TEST(RigidBodyTest, ForcesAccelerateByInverseMass) {
  RigidBody body = makeBody();
  body.setMass(4.0);
  body.addForce(Vector3(8, 0, 0)); // a = F / m = 2
  body.integrate(0.5);
  expectNear(body.getVelocity(), Vector3(1, 0, 0));
}

// Damping scales velocity by damping^dt each step: 0.5 per second.
TEST(RigidBodyTest, LinearDamping) {
  RigidBody body = makeBody();
  body.setDamping(0.5, 1.0);
  body.setVelocity(1, 0, 0);
  for (int i = 0; i < 10; ++i) {
    body.integrate(0.1);
  }
  EXPECT_NEAR(body.getVelocity().x, 0.5, kTolerance);
}

TEST(RigidBodyTest, RotationTurnsTheBody) {
  RigidBody body = makeBody();
  body.setRotation(Vector3(0, 0, 1)); // 1 rad/s about z
  const int steps = 10000;
  for (int i = 0; i < steps; ++i) {
    body.integrate((M_PI / 2) / steps);
  }
  expectNear(body.getDirectionInWorldSpace(Vector3(1, 0, 0)), Vector3(0, 1, 0), 1e-3);
}

TEST(RigidBodyTest, LocalAndWorldSpaceRoundTrip) {
  RigidBody body = makeBody(Vector3(2, 3, 4));
  body.setOrientation(rotation(1.2, Vector3(0, 1, 0)));
  body.calculateDerivedData();
  const Vector3 point(1, -1, 0.5);
  expectNear(body.getPointInLocalSpace(body.getPointInWorldSpace(point)), point);
  expectNear(body.getPointInWorldSpace(Vector3()), Vector3(2, 3, 4));
}

// Particles move with the old velocity and then update it (explicit Euler):
//   x = x0 + n dt v0 + a dt^2 n (n - 1) / 2
TEST(ParticleTest, IntegrationMatchesClosedForm) {
  Particle p;
  p.setMass(1.0);
  p.setDamping(1.0);
  p.setPosition(0, 0, 0);
  p.setVelocity(1, 0, 0);
  p.setAcceleration(0, -10, 0);
  const int n = 10;
  const double dt = 0.1;
  for (int i = 0; i < n; ++i) {
    p.integrate(dt);
  }
  expectNear(p.getVelocity(), Vector3(1, -10 * n * dt, 0));
  expectNear(p.getPosition(), Vector3(n * dt, -10 * dt * dt * n * (n - 1) / 2, 0));
}

// ---------------------------------------------------------------------------
// Contact generation

namespace {

struct ContactFixture {
  Contact contacts[4];
  CollisionData data;
  ContactFixture() {
    data.contactArray = contacts;
    data.friction = 0.5;
    data.restitution = 0.3;
    data.tolerance = 0.1;
    data.reset(4);
  }
};

CollisionSphere sphereOn(RigidBody &body, double radius) {
  CollisionSphere sphere;
  sphere.body = &body;
  sphere.radius = radius;
  sphere.calculateInternals();
  return sphere;
}

} // namespace

TEST(CollisionTest, SphereAndSphere) {
  ContactFixture fixture;
  RigidBody one = makeBody(Vector3(0, 0, 0));
  RigidBody two = makeBody(Vector3(1.5, 0, 0));
  ASSERT_EQ(CollisionDetector::sphereAndSphere(sphereOn(one, 1), sphereOn(two, 1), &fixture.data),
            1u);

  const Contact &contact = fixture.contacts[0];
  expectNear(contact.contactNormal, Vector3(-1, 0, 0)); // from two towards one
  EXPECT_NEAR(contact.penetration, 0.5, kTolerance);
  expectNear(contact.contactPoint, Vector3(0.75, 0, 0)); // midway between the centres
  EXPECT_EQ(contact.body[0], &one);
  EXPECT_EQ(contact.body[1], &two);
  EXPECT_DOUBLE_EQ(contact.friction, 0.5);
  EXPECT_EQ(fixture.data.contactCount, 1u);
}

TEST(CollisionTest, SeparatedSpheresDontTouch) {
  ContactFixture fixture;
  RigidBody one = makeBody(Vector3(0, 0, 0));
  RigidBody two = makeBody(Vector3(2.5, 0, 0));
  EXPECT_EQ(CollisionDetector::sphereAndSphere(sphereOn(one, 1), sphereOn(two, 1), &fixture.data),
            0u);
  EXPECT_EQ(fixture.data.contactCount, 0u);
}

TEST(CollisionTest, SphereAndHalfSpace) {
  ContactFixture fixture;
  RigidBody ball = makeBody(Vector3(0, 0.5, 0));
  CollisionPlane ground;
  ground.direction = Vector3(0, 1, 0);
  ground.offset = 0;
  ASSERT_EQ(CollisionDetector::sphereAndHalfSpace(sphereOn(ball, 1), ground, &fixture.data), 1u);

  const Contact &contact = fixture.contacts[0];
  expectNear(contact.contactNormal, Vector3(0, 1, 0));
  EXPECT_NEAR(contact.penetration, 0.5, kTolerance);
  expectNear(contact.contactPoint, Vector3(0, 0, 0)); // on the plane, below the centre

  RigidBody high = makeBody(Vector3(0, 3, 0));
  EXPECT_EQ(CollisionDetector::sphereAndHalfSpace(sphereOn(high, 1), ground, &fixture.data), 0u);
}

// No contacts are written once the array is full.
TEST(CollisionTest, StopsWhenContactArrayIsFull) {
  ContactFixture fixture;
  fixture.data.reset(1);
  RigidBody one = makeBody(Vector3(0, 0, 0));
  RigidBody two = makeBody(Vector3(1, 0, 0));
  EXPECT_EQ(CollisionDetector::sphereAndSphere(sphereOn(one, 1), sphereOn(two, 1), &fixture.data),
            1u);
  EXPECT_EQ(CollisionDetector::sphereAndSphere(sphereOn(one, 1), sphereOn(two, 1), &fixture.data),
            0u);
  EXPECT_FALSE(fixture.data.hasMoreContacts());
}

// Friction acts at the contact point, so it spins a sliding sphere the way
// physics says. Sphere one moves toward sphere two (along +x) while sliding
// along +y. Friction pushes one along -y at its +x side and two along +y at
// its -x side, so both turn about -z. With the old contact point (on one's
// far side) sphere one turned about +z.
TEST(CollisionTest, SphereFrictionSpinsTheRightWay) {
  ContactFixture fixture;
  fixture.data.friction = 0.9;
  fixture.data.restitution = 0.0;
  RigidBody one = makeBody(Vector3(0, 0, 0));
  RigidBody two = makeBody(Vector3(1.9, 0, 0));
  one.setVelocity(2, 5, 0);
  ASSERT_EQ(CollisionDetector::sphereAndSphere(sphereOn(one, 1), sphereOn(two, 1), &fixture.data),
            1u);

  ContactResolver resolver(10);
  resolver.resolveContacts(fixture.contacts, fixture.data.contactCount, 0.01);

  EXPECT_LT(one.getRotation().z, 0.0);
  EXPECT_LT(two.getRotation().z, 0.0);
  EXPECT_LT(one.getVelocity().y, 5.0); // friction slowed the sliding
}
