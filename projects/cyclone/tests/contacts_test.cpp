// Cyclone's contact handling: the rigid-body resolver (bounce and push
// apart), joints, and the particle links and world.
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/plinks.h>

#include <algorithm>
#include <memory>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

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
  body->setCanSleep(false);
  body->calculateDerivedData();
  return body;
}

// A ball of radius 1 at height `y` moving with `velocity`, its contact with
// the ground (if any) resolved once.
struct BallOnGround {
  std::unique_ptr<RigidBody> body;
  CollisionSphere sphere;
  Contact contacts[4];
  CollisionData data;
  unsigned found = 0;

  BallOnGround(double y, const Vector3 &velocity, double restitution)
      : body(bodyAt(Vector3(0, y, 0))) {
    body->setVelocity(velocity);
    sphere.body = body.get();
    sphere.radius = 1;
    sphere.calculateInternals();
    data.contactArray = contacts;
    data.friction = 0;
    data.restitution = restitution;
    data.tolerance = 0.1;
    data.reset(4);
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    found = CollisionDetector::sphereAndHalfSpace(sphere, ground, &data);
    ContactResolver resolver(10);
    resolver.resolveContacts(contacts, found, 1.0 / 60);
  }
};

std::unique_ptr<Particle> particleAt(const Vector3 &position) {
  auto particle = std::make_unique<Particle>();
  particle->setMass(1);
  particle->setDamping(1);
  particle->setPosition(position);
  particle->setVelocity(0, 0, 0);
  particle->setAcceleration(Vector3());
  particle->clearAccumulator();
  return particle;
}

double distance(const Particle &a, const Particle &b) {
  return (a.getPosition() - b.getPosition()).magnitude();
}

} // namespace

// ---------------------------------------------------------------------------
// Rigid-body contact resolution

// Falling at 4 m/s onto the ground with restitution 0.5: it leaves at 2 m/s.
TEST(ContactResolverTest, BounceKeepsRestitutionTimesSpeed) {
  BallOnGround ball(0.99, Vector3(0, -4, 0), 0.5);
  ASSERT_EQ(ball.found, 1u);
  EXPECT_NEAR(ball.body->getVelocity().y, 2.0, 1e-6);
}

TEST(ContactResolverTest, ZeroRestitutionStops) {
  BallOnGround ball(0.99, Vector3(0, -4, 0), 0);
  ASSERT_EQ(ball.found, 1u);
  EXPECT_NEAR(ball.body->getVelocity().y, 0.0, 1e-6);
}

// Sunk 0.3 into the ground, it's moved back out to rest on the surface.
TEST(ContactResolverTest, OverlapIsPushedApart) {
  BallOnGround ball(0.7, Vector3(), 0);
  ASSERT_EQ(ball.found, 1u);
  EXPECT_NEAR(ball.body->getPosition().y, 1.0, 1e-6);
}

// Two equal balls meeting head on at 1 m/s each with restitution 1 swap
// velocities, and momentum is conserved.
TEST(ContactResolverTest, HeadOnElasticCollisionSwapsVelocities) {
  auto one = bodyAt(Vector3(0, 0, 0)), two = bodyAt(Vector3(1.9, 0, 0));
  one->setVelocity(1, 0, 0);
  two->setVelocity(-1, 0, 0);
  CollisionSphere a, b;
  a.body = one.get();
  b.body = two.get();
  a.radius = b.radius = 1;
  a.calculateInternals();
  b.calculateInternals();
  Contact contacts[2];
  CollisionData data;
  data.contactArray = contacts;
  data.friction = 0;
  data.restitution = 1;
  data.reset(2);
  ASSERT_EQ(CollisionDetector::sphereAndSphere(a, b, &data), 1u);
  ContactResolver(10).resolveContacts(contacts, 1, 1.0 / 60);
  EXPECT_NEAR(one->getVelocity().x, -1, 1e-6);
  EXPECT_NEAR(two->getVelocity().x, 1, 1e-6);
  EXPECT_NEAR(one->getPosition().x + 2.0, two->getPosition().x,
              1e-6); // just touching
}

// ---------------------------------------------------------------------------
// Joints

// A joint allowed 0.5 m of slack: bodies 2 m apart get a contact pulling them
// together (1.5 m too far); 0.3 m apart is within the slack.
TEST(JointTest, ContactOnlyWhenStretchedBeyondItsSlack) {
  auto one = bodyAt(Vector3(0, 0, 0)), two = bodyAt(Vector3(2, 0, 0));
  Joint joint;
  joint.set(one.get(), Vector3(), two.get(), Vector3(), 0.5);
  Contact contact;
  ASSERT_EQ(joint.addContact(&contact, 1), 1u);
  EXPECT_NEAR(contact.penetration, 1.5, kTolerance);
  EXPECT_NEAR(contact.contactNormal.x, 1.0, kTolerance);

  // Resolving it brings them back within the slack
  contact.setBodyData(one.get(), two.get(), 0, 0);
  ContactResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
  EXPECT_NEAR((two->getPosition() - one->getPosition()).magnitude(), 0.5, 1e-6);

  two->setPosition(0.3, 0, 0);
  two->calculateDerivedData();
  EXPECT_EQ(joint.addContact(&contact, 1), 0u);
}

// ---------------------------------------------------------------------------
// Particle links

// A 2 m cable: slack at 1 m, taut and pulled back to 2 m at 3 m.
TEST(ParticleLinkTest, CableOnlyPullsWhenTaut) {
  auto a = particleAt(Vector3(0, 0, 0)), b = particleAt(Vector3(1, 0, 0));
  ParticleCable cable;
  cable.particle[0] = a.get();
  cable.particle[1] = b.get();
  cable.maxLength = 2;
  cable.restitution = 0;
  ParticleContact contact;
  EXPECT_EQ(cable.addContact(&contact, 1), 0u);

  b->setPosition(3, 0, 0);
  ASSERT_EQ(cable.addContact(&contact, 1), 1u);
  EXPECT_NEAR(contact.penetration, 1.0, kTolerance);
  ParticleContactResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
  EXPECT_NEAR(distance(*a, *b), 2.0, 1e-6);
}

// A 2 m rod holds its length whether stretched or squashed.
TEST(ParticleLinkTest, RodHoldsItsLengthBothWays) {
  for (double start : {3.0, 1.0}) {
    SCOPED_TRACE(testing::Message() << "starting " << start << " m apart");
    auto a = particleAt(Vector3(0, 0, 0)), b = particleAt(Vector3(start, 0, 0));
    ParticleRod rod;
    rod.particle[0] = a.get();
    rod.particle[1] = b.get();
    rod.length = 2;
    ParticleContact contact;
    ASSERT_EQ(rod.addContact(&contact, 1), 1u);
    EXPECT_NEAR(contact.penetration, 1.0, kTolerance);
    ParticleContactResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
    EXPECT_NEAR(distance(*a, *b), 2.0, 1e-6);
  }
  auto a = particleAt(Vector3(0, 0, 0)), b = particleAt(Vector3(2, 0, 0));
  ParticleRod rod;
  rod.particle[0] = a.get();
  rod.particle[1] = b.get();
  rod.length = 2;
  ParticleContact contact;
  EXPECT_EQ(rod.addContact(&contact, 1), 0u);
}

// ---------------------------------------------------------------------------
// The particle world

// A particle dropped from 5 m under gravity lands on the ground and stays
// there, at any frame rate.
TEST(ParticleWorldTest, DroppedParticleLandsAndRests) {
  for (double dt : {1.0 / 120, 1.0 / 60, 0.05}) {
    SCOPED_TRACE(testing::Message() << "dt " << dt);
    ParticleWorld world(10);
    auto particle = particleAt(Vector3(0, 5, 0));
    particle->setAcceleration(Vector3::GRAVITY);
    particle->setDamping(0.99);
    world.getParticles().push_back(particle.get());
    GroundContacts ground;
    ground.init(&world.getParticles());
    world.getContactGenerators().push_back(&ground);

    double lowest = 5, highestLastSecond = -5, lowestLastSecond = 5;
    const int steps = static_cast<int>(10 / dt);
    for (int i = 0; i < steps; ++i) {
      world.startFrame();
      world.runPhysics(dt);
      const double y = particle->getPosition().y;
      lowest = std::min(lowest, y);
      if (i >= steps - static_cast<int>(1 / dt)) {
        highestLastSecond = std::max(highestLastSecond, y);
        lowestLastSecond = std::min(lowestLastSecond, y);
      }
    }
    // Resting: within a frame's worth of gravity (g dt^2) of the ground for
    // the whole last second. (The ground contact only fires below y = 0, so
    // the velocity alternates between 0 and g dt from frame to frame.)
    EXPECT_LT(highestLastSecond, 0.01);
    EXPECT_GT(lowestLastSecond, -9.81 * dt * dt - 1e-9);
    EXPECT_GT(lowest, -0.6); // never sinks far in, even at 20 fps
  }
}
