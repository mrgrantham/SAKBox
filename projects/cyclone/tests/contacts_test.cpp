// Cyclone's contact handling: the rigid-body resolver (bounce and push
// apart), joints, the particle links, constraints and world, and the
// rigid-body world (boxes landing and stacking).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/plinks.h>
#include <cyclone/world.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

// The book's resolver, which these tests pin: one contact at a time
ContactResolver booksResolver(unsigned iterations) {
  ContactResolver resolver(iterations);
  resolver.setMethod(ContactResolver::Method::oneContactAtATime);
  return resolver;
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
    ContactResolver resolver = booksResolver(10);
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
  booksResolver(10).resolveContacts(contacts, 1, 1.0 / 60);
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
  booksResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
  EXPECT_NEAR((two->getPosition() - one->getPosition()).magnitude(), 0.5, 1e-6);

  two->setPosition(0.3, 0, 0);
  two->calculateDerivedData();
  EXPECT_EQ(joint.addContact(&contact, 1), 0u);
}

// ---------------------------------------------------------------------------
// Particle links

// The resolver updates every other contact's penetration by how far each
// resolve moved the particles. A contact resolved for its velocity alone
// (closing, but not overlapping) moves nothing, so must count as no move:
// it used to keep its last move, here from the frame before, which "used
// up" another contact's penetration so that it was never pushed out.
// (Particles tied by rods flew apart; see stress_test.)
TEST(ParticleContactTest, AVelocityOnlyResolveMovesNothing) {
  auto a = particleAt(Vector3(0, 0, 0));
  ParticleContact contacts[2];
  ParticleContactResolver resolver(10);
  // Frame one: the ground pushes the particle up by 1
  contacts[0].particle[0] = a.get();
  contacts[0].particle[1] = nullptr;
  contacts[0].contactNormal = Vector3(0, 1, 0);
  contacts[0].penetration = 1;
  contacts[0].restitution = 0;
  resolver.resolveContacts(contacts, 1, 1.0 / 60);
  EXPECT_NEAR(a->getPosition().y, 1.0, kTolerance);
  // Frame two, the same array: falling onto the ground (closing, not
  // overlapping), and sunk 0.5 into a second surface facing the same way
  a->setVelocity(0, -1, 0);
  contacts[0].penetration = 0;
  contacts[1] = contacts[0];
  contacts[1].penetration = 0.5;
  resolver.resolveContacts(contacts, 2, 1.0 / 60);
  EXPECT_NEAR(a->getPosition().y, 1.5, kTolerance); // pushed out of the second
  EXPECT_NEAR(a->getVelocity().y, 0.0, kTolerance); // and stopped by the first
}

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
// Particle constraints: a cable or rod from a particle to a fixed point

// A 2 m cable to the origin: slack at 1 m, taut and pulled back to 2 m at 3 m.
TEST(ParticleConstraintTest, CableOnlyPullsWhenTaut) {
  auto particle = particleAt(Vector3(1, 0, 0));
  ParticleCableConstraint cable;
  cable.particle = particle.get();
  cable.anchor = Vector3(0, 0, 0);
  cable.maxLength = 2;
  cable.restitution = 0;
  ParticleContact contact;
  EXPECT_EQ(cable.addContact(&contact, 1), 0u);

  particle->setPosition(3, 0, 0);
  ASSERT_EQ(cable.addContact(&contact, 1), 1u);
  EXPECT_NEAR(contact.penetration, 1.0, kTolerance);
  ParticleContactResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
  EXPECT_NEAR(particle->getPosition().magnitude(), 2.0, 1e-6);
}

// A 2 m rod to the origin holds its length whether stretched or squashed.
TEST(ParticleConstraintTest, RodHoldsItsLengthBothWays) {
  for (double start : {3.0, 1.0}) {
    SCOPED_TRACE(testing::Message() << "starting " << start << " m out");
    auto particle = particleAt(Vector3(0, start, 0));
    ParticleRodConstraint rod;
    rod.particle = particle.get();
    rod.anchor = Vector3(0, 0, 0);
    rod.length = 2;
    ParticleContact contact;
    ASSERT_EQ(rod.addContact(&contact, 1), 1u);
    EXPECT_NEAR(contact.penetration, 1.0, kTolerance);
    ParticleContactResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
    EXPECT_NEAR(particle->getPosition().magnitude(), 2.0, 1e-6);
  }
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
    // Resting still on the ground for the whole last second. (Before
    // particles updated their velocity before moving, the velocity
    // alternated between 0 and g dt from frame to frame, so the particle
    // jittered by g dt^2.)
    EXPECT_NEAR(highestLastSecond, 0, 1e-9);
    EXPECT_NEAR(lowestLastSecond, 0, 1e-9);
    EXPECT_GT(lowest, -0.6); // never sinks far in, even at 20 fps
  }
}

// Forces registered with the world are applied each step: a particle under
// a registered gravity generator falls like one with gravity built in.
TEST(ParticleWorldTest, RegisteredForcesAreApplied) {
  ParticleWorld world(10);
  auto registered = particleAt(Vector3(0, 100, 0));
  auto builtIn = particleAt(Vector3(0, 100, 0));
  builtIn->setAcceleration(Vector3::GRAVITY);
  ParticleGravity gravity(Vector3::GRAVITY);
  world.getParticles() = {registered.get(), builtIn.get()};
  world.getForceRegistry().add(registered.get(), &gravity);
  for (int i = 0; i < 60; ++i) {
    world.startFrame();
    world.runPhysics(1.0 / 60);
  }
  EXPECT_LT(registered->getPosition().y, 96.0); // fell about g/2
  EXPECT_NEAR(registered->getPosition().y, builtIn->getPosition().y, kTolerance);
}

// A pendulum: a particle on a 1 m rod from a fixed point, let go level with
// it. The rod holds its length throughout, the swing reaches the other side,
// and a full swing takes as long as a pendulum's: 7.416 sqrt(L / g) for a
// 90 degree amplitude (4 K(sin 45) sqrt(L / g)), timed between the first and
// third passes through the bottom.
TEST(ParticleWorldTest, PendulumKeepsItsLengthAndPeriod) {
  ParticleWorld world(10);
  auto bob = particleAt(Vector3(1, 0, 0));
  bob->setAcceleration(Vector3::GRAVITY);
  ParticleRodConstraint rod;
  rod.particle = bob.get();
  rod.anchor = Vector3(0, 0, 0);
  rod.length = 1;
  world.getParticles().push_back(bob.get());
  world.getContactGenerators().push_back(&rod);

  const double dt = 1.0 / 600;
  double longest = 1, shortest = 1, farthestLeft = 1, previousX = 1;
  std::vector<double> bottoms;
  for (int i = 1; i <= 3 * 600 && bottoms.size() < 3; ++i) {
    world.startFrame();
    world.runPhysics(dt);
    const double length = bob->getPosition().magnitude();
    longest = std::max(longest, length);
    shortest = std::min(shortest, length);
    const double x = bob->getPosition().x;
    farthestLeft = std::min(farthestLeft, x);
    if ((previousX > 0) != (x > 0))
      bottoms.push_back(i * dt);
    previousX = x;
  }
  EXPECT_NEAR(longest, 1.0, 1e-6);
  EXPECT_NEAR(shortest, 1.0, 1e-6);
  EXPECT_LT(farthestLeft, -0.95);
  ASSERT_EQ(bottoms.size(), 3u);
  const double period = 7.4163 * std::sqrt(1 / 9.81);
  EXPECT_NEAR(bottoms[2] - bottoms[0], period, 0.01 * period);
}

// ---------------------------------------------------------------------------
// The rigid-body world

namespace {

// Unit cubes (mass 1) at the given heights, falling under a gravity
// generator registered with the world, onto the ground (y = 0) and each
// other: the world's box shapes and plane
struct BoxWorld {
  World world{World::Settings{.maxContacts = 256, .friction = 0.9, .restitution = 0.1}};
  Gravity gravity{Vector3::GRAVITY};
  std::vector<std::unique_ptr<RigidBody>> bodies;
  std::deque<CollisionBox> boxes; // (a deque, so adding one moves none)

  explicit BoxWorld(const std::vector<Vector3> &positions,
                    ContactResolver::Method method = ContactResolver::Method::oneContactAtATime) {
    world.getResolver().setMethod(method);
    for (const Vector3 &position : positions) {
      bodies.push_back(bodyAt(position));
      RigidBody &body = *bodies.back();
      Matrix3 inertia;
      inertia.setBlockInertiaTensor(Vector3(0.5, 0.5, 0.5), 1);
      body.setInertiaTensor(inertia);
      body.setDamping(0.95, 0.8);
      body.calculateDerivedData();
      world.getBodies().push_back(&body);
      world.getForceRegistry().add(&body, &gravity);
      CollisionBox &box = boxes.emplace_back();
      box.body = &body;
      box.halfSize = Vector3(0.5, 0.5, 0.5);
      world.addShape(&box);
    }
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    world.addPlane(ground);
  }

  // Runs the world for `seconds`, returning the most any box tipped
  double run(double seconds);
};

// How far a body's up axis has tipped from vertical, in degrees
double tiltDegrees(const RigidBody &body) {
  const Vector3 up = body.getTransform().transformDirection(Vector3(0, 1, 0));
  return std::acos(std::clamp<double>(up.y, -1, 1)) * 180 / R_PI;
}

double BoxWorld::run(double seconds) {
  double mostTilted = 0;
  for (int i = 0; i < static_cast<int>(seconds * 60); ++i) {
    world.startFrame();
    world.runPhysics(1.0 / 60);
    for (const auto &body : bodies)
      mostTilted = std::max(mostTilted, tiltDegrees(*body));
  }
  return mostTilted;
}

} // namespace

// A box dropped from 3 m lands flat on the ground and comes to rest.
TEST(WorldTest, DroppedBoxComesToRest) {
  BoxWorld scene({Vector3(0, 3, 0)});
  EXPECT_LT(scene.run(5), 1.0);
  const RigidBody &box = *scene.bodies[0];
  EXPECT_NEAR(box.getPosition().y, 0.5, 0.02);
  EXPECT_NEAR(box.getVelocity().magnitude(), 0, 0.05);
}

// Stacks of two and three boxes stay stacked for 30 s: none drifts more
// than a few centimetres sideways, sinks into the one below or tips. (With
// only the deepest contact between each pair of boxes, a box resting on
// another was held up at one corner: three boxes rocked from the start and
// the top one fell off after 3.5 s. Taller stacks still creep and fall: see
// the README's known limitations.)
TEST(WorldTest, StacksOfBoxesStand) {
  for (int height : {2, 3}) {
    SCOPED_TRACE(testing::Message() << height << " boxes");
    std::vector<Vector3> positions(height);
    for (int i = 0; i < height; ++i)
      positions[i] = Vector3(0, 0.5 + i, 0);
    BoxWorld scene(positions);
    EXPECT_LT(scene.run(30), 2.0);
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "box " << i);
      const RigidBody &box = *scene.bodies[i];
      EXPECT_NEAR(box.getPosition().x, 0, 0.05);
      EXPECT_NEAR(box.getPosition().z, 0, 0.05);
      EXPECT_NEAR(box.getPosition().y, 0.5 + i, 0.03);
    }
  }
}

// The centres of `height` unit cubes stacked from the ground up
std::vector<Vector3> stackOf(int height) {
  std::vector<Vector3> positions(height);
  for (int i = 0; i < height; ++i)
    positions[i] = Vector3(0, 0.5 + i, 0);
  return positions;
}

// Lets the scene's bodies sleep (bodyAt makes them unable to, so that other
// tests run them the whole time)
void letSleep(BoxWorld &scene) {
  for (const auto &body : scene.bodies)
    body->setCanSleep(true);
}

bool allAsleep(const BoxWorld &scene) {
  return std::none_of(scene.bodies.begin(), scene.bodies.end(),
                      [](const auto &body) { return body->getAwake(); });
}

// A box on its own on the ground still falls asleep by itself.
TEST(SleepTest, LoneBoxFallsAsleep) {
  BoxWorld scene(stackOf(1));
  letSleep(scene);
  scene.run(5);
  EXPECT_FALSE(scene.bodies[0]->getAwake());
  EXPECT_FALSE(scene.bodies[0]->getInContactGroup());
}

// A stack falls asleep as one, a few seconds after it's built, and stays
// put, each box resting less than the resolver's 1 cm position tolerance
// into the one below. (Box by box, each box that fell asleep was woken by
// the one on it still settling, so a stack never slept.)
TEST(SleepTest, StackFallsAsleepTogether) {
  for (int height : {2, 3, 5}) {
    SCOPED_TRACE(testing::Message() << height << " boxes");
    BoxWorld scene(stackOf(height));
    letSleep(scene);
    scene.run(5);
    EXPECT_TRUE(allAsleep(scene));
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "box " << i);
      EXPECT_TRUE(scene.bodies[i]->getInContactGroup());
      EXPECT_NEAR(scene.bodies[i]->getPosition().x, 0, 0.02);
      EXPECT_NEAR(scene.bodies[i]->getPosition().y, 0.5 + i, 0.01 * (i + 1));
    }
  }
}

// A box dropped onto a sleeping stack wakes the whole stack, which falls
// asleep again, four high, once it has settled.
TEST(SleepTest, BoxDroppedOnAStackWakesItAll) {
  // The fourth box waits on the ground off to one side
  std::vector<Vector3> positions = stackOf(3);
  positions.push_back(Vector3(10, 0.5, 0));
  BoxWorld scene(positions);
  letSleep(scene);
  scene.run(5);
  ASSERT_TRUE(allAsleep(scene));

  RigidBody &dropped = *scene.bodies[3];
  dropped.setPosition(0, 4, 0);
  dropped.setVelocity(0, -2, 0);
  dropped.setAwake();
  scene.run(0.5);
  for (int i = 0; i < 3; ++i)
    EXPECT_TRUE(scene.bodies[i]->getAwake()) << "box " << i;

  scene.run(5);
  EXPECT_TRUE(allAsleep(scene));
  for (size_t i = 0; i < scene.bodies.size(); ++i)
    EXPECT_NEAR(scene.bodies[i]->getPosition().y, 0.5 + i, 0.01 * (i + 1)) << "box " << i;
}

// Knocking the bottom box out of a sleeping stack wakes the box on it at
// once, and it falls.
TEST(SleepTest, KnockingOutTheBottomBoxDropsTheTop) {
  BoxWorld scene(stackOf(2));
  letSleep(scene);
  scene.run(5);
  ASSERT_TRUE(allAsleep(scene));

  scene.bodies[0]->setAwake();
  scene.bodies[0]->setVelocity(10, 0, 0);
  scene.run(1.0 / 60);
  EXPECT_TRUE(scene.bodies[1]->getAwake());
  scene.run(2);
  EXPECT_LT(scene.bodies[1]->getPosition().y, 1);
}

// A tall stack set up asleep, as the book suggests for a level, stays up
// until something hits it: awake from the start, it rocks and falls within
// seconds.
TEST(SleepTest, TallStackSetUpAsleepStaysUp) {
  BoxWorld scene(stackOf(8));
  letSleep(scene);
  for (const auto &body : scene.bodies)
    body->setAwake(false);
  scene.run(30);
  EXPECT_TRUE(allAsleep(scene));
  for (size_t i = 0; i < scene.bodies.size(); ++i)
    EXPECT_NEAR(scene.bodies[i]->getPosition().y, 0.5 + i, 1e-9) << "box " << i;
}

// A body that can't sleep keeps everything it touches awake.
TEST(SleepTest, BodyThatCantSleepKeepsItsGroupAwake) {
  BoxWorld scene(stackOf(2));
  letSleep(scene);
  scene.bodies[1]->setCanSleep(false);
  scene.run(5);
  EXPECT_TRUE(scene.bodies[0]->getAwake());
  EXPECT_TRUE(scene.bodies[1]->getAwake());
}

// A body that leaves its group sleeps on its own again, even with no
// contacts at all (the world then skips the resolver). Two boxes float
// touching, without gravity, then one is moved away.
TEST(SleepTest, BodyThatLeavesItsGroupSleepsAlone) {
  BoxWorld scene({Vector3(0, 5, 0), Vector3(1, 5, 0)});
  letSleep(scene);
  for (const auto &body : scene.bodies)
    scene.world.getForceRegistry().remove(body.get(), &scene.gravity);
  scene.run(1.0 / 60);
  ASSERT_TRUE(scene.bodies[0]->getInContactGroup());

  scene.bodies[1]->setPosition(10, 5, 0);
  scene.run(5);
  EXPECT_TRUE(allAsleep(scene));
}

// The world's contact array is shared by its generators in turn: once it's
// full, the rest aren't asked, and generateContacts reports what fitted.
TEST(WorldTest, ContactsStopWhenTheArrayIsFull) {
  struct Fixed : ContactGenerator {
    unsigned count;
    mutable unsigned calls = 0, lastLimit = 0;
    explicit Fixed(unsigned count) : count(count) {}
    unsigned addContact(Contact *, unsigned limit) const override {
      ++calls;
      lastLimit = limit;
      return std::min(count, limit);
    }
  };
  Fixed first(3), second(5), third(1);
  World world(6);
  world.getContactGenerators() = {&first, &second, &third};
  EXPECT_EQ(world.generateContacts(), 6u);
  EXPECT_EQ(second.lastLimit, 3u);
  EXPECT_EQ(third.calls, 0u);
}

// ---------------------------------------------------------------------------
// Resolving contacts together (ContactResolver::Method::sequentialImpulses)

namespace {
constexpr ContactResolver::Method together = ContactResolver::Method::sequentialImpulses;

// The farthest any box is from its place in the stack, sideways
double farthestSideways(const BoxWorld &scene) {
  double farthest = 0;
  for (const auto &body : scene.bodies)
    farthest = std::max<double>(farthest, std::hypot(body->getPosition().x, body->getPosition().z));
  return farthest;
}

double fastest(const BoxWorld &scene) {
  double fastest = 0;
  for (const auto &body : scene.bodies)
    fastest = std::max<double>(fastest, body->getVelocity().magnitude());
  return fastest;
}
} // namespace

// Awake stacks of up to ten boxes stand for 30 s. They lean a little at
// first, while the contact cache gathers each pair's corners, settle within
// 10 s, and then don't move: no creep. (The book's resolver drops five boxes after
// about 20 s, eight after 7 s and ten after 5 s.)
TEST(ResolveTogetherTest, AwakeStacksStandStill) {
  for (int height : {3, 5, 8, 10}) {
    SCOPED_TRACE(testing::Message() << height << " boxes");
    BoxWorld scene(stackOf(height), together);
    EXPECT_LT(scene.run(10), 2.0);
    std::vector<Vector3> settled;
    settled.reserve(scene.bodies.size());
    for (const auto &body : scene.bodies)
      settled.push_back(body->getPosition());
    EXPECT_LT(scene.run(20), 2.0);
    EXPECT_LT(farthestSideways(scene), 0.1);
    EXPECT_LT(fastest(scene), 0.01);
    for (size_t i = 0; i < scene.bodies.size(); ++i) {
      SCOPED_TRACE(testing::Message() << "box " << i);
      const Vector3 position = scene.bodies[i]->getPosition();
      EXPECT_LT((position - settled[i]).magnitude(), 0.005); // no creep
      EXPECT_NEAR(position.y, 0.5 + i, 0.004 * (i + 1));     // a few mm into the one below
    }
  }
}

// A new stack doesn't lean: each pair of boxes is held at the whole area
// where their faces touch from the first frame. (Gathering a corner a
// frame, ten boxes leant about 15 cm in their first seconds.)
TEST(ResolveTogetherTest, NewStackDoesntLean) {
  BoxWorld scene(stackOf(10), together);
  double leant = 0;
  for (int i = 0; i < 120; ++i) {
    scene.run(1.0 / 60);
    leant = std::max(leant, farthestSideways(scene));
  }
  EXPECT_LT(leant, 0.03);
}

// The same tall stack with the book's resolver falls: the limitation the
// method is for (README, known limitations).
TEST(ResolveTogetherTest, BooksResolverDropsTheSameStack) {
  BoxWorld scene(stackOf(8));
  scene.run(30);
  EXPECT_LT(scene.bodies.back()->getPosition().y, 6.0);
}

// A box dropped from 3 m lands flat and comes to rest, as with the book's
// resolver.
TEST(ResolveTogetherTest, DroppedBoxComesToRest) {
  BoxWorld scene({Vector3(0, 3, 0)}, together);
  EXPECT_LT(scene.run(5), 1.0);
  const RigidBody &box = *scene.bodies[0];
  EXPECT_NEAR(box.getPosition().y, 0.5, 0.005);
  EXPECT_NEAR(box.getVelocity().magnitude(), 0, 0.01);
}

// Stacks still fall asleep as one, and a box dropped on a sleeping stack
// wakes it all. After the impact each box may rest up to the resolver's 1 cm
// position tolerance into the one below, as with the book's resolver.
TEST(ResolveTogetherTest, StacksSleepAndWake) {
  std::vector<Vector3> positions = stackOf(5);
  positions.push_back(Vector3(10, 0.5, 0));
  BoxWorld scene(positions, together);
  letSleep(scene);
  scene.run(5);
  ASSERT_TRUE(allAsleep(scene));

  RigidBody &dropped = *scene.bodies[5];
  dropped.setPosition(0, 6, 0);
  dropped.setVelocity(0, -2, 0);
  dropped.setAwake();
  scene.run(0.5);
  for (int i = 0; i < 5; ++i)
    EXPECT_TRUE(scene.bodies[i]->getAwake()) << "box " << i;
  scene.run(5);
  EXPECT_TRUE(allAsleep(scene));
  for (size_t i = 0; i < scene.bodies.size(); ++i)
    EXPECT_NEAR(scene.bodies[i]->getPosition().y, 0.5 + i, 0.01 * (i + 1)) << "box " << i;
}

// Knocking the bottom box out still drops the one on it.
TEST(ResolveTogetherTest, KnockingOutTheBottomBoxDropsTheTop) {
  BoxWorld scene(stackOf(2), together);
  scene.run(2);
  scene.bodies[0]->setVelocity(10, 0, 0);
  scene.run(2);
  EXPECT_LT(scene.bodies[1]->getPosition().y, 1);
}

// A box sliding along the ground slows at friction x g, as with one contact
// at a time: friction is limited by the push at each contact.
TEST(ResolveTogetherTest, SlidingBoxSlowsAtFrictionTimesG) {
  BoxWorld scene({Vector3(0, 0.5, 0)}, together);
  scene.run(0.5);
  scene.bodies[0]->setVelocity(5, 0, 0);
  scene.run(0.2);
  const double before = scene.bodies[0]->getVelocity().x;
  scene.run(0.2);
  const double slowing = (before - scene.bodies[0]->getVelocity().x) / 0.2;
  EXPECT_NEAR(slowing, 0.9 * 9.81, 0.5); // the scene's friction is 0.9
}

// A ball dropped onto the ground in the game loop bounces at restitution x
// its speed when it lands, less the speed gravity added during the step (the
// book's rule, which the book's resolver follows too).
TEST(ResolveTogetherTest, BounceKeepsRestitutionTimesSpeed) {
  auto ball = bodyAt(Vector3(0, 3, 0));
  ball->setAcceleration(Vector3::GRAVITY);
  CollisionSphere sphere;
  sphere.body = ball.get();
  sphere.radius = 1;
  ContactResolver resolver(10);
  resolver.setMethod(together);
  Contact contacts[4];
  double landing = 0, leaving = 0;
  for (int step = 0; step < 120 && leaving == 0; ++step) {
    ball->clearAccumulators();
    ball->calculateDerivedData();
    ball->integrate(1.0 / 60);
    sphere.calculateInternals();
    CollisionData data;
    data.contactArray = contacts;
    data.friction = 0;
    data.restitution = 0.5;
    data.tolerance = 0.1;
    data.reset(4);
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    if (CollisionDetector::sphereAndHalfSpace(sphere, ground, &data) == 0)
      continue;
    landing = ball->getVelocity().y;
    resolver.resolveContacts(contacts, data.contactCount, 1.0 / 60);
    if (ball->getVelocity().y > 0)
      leaving = ball->getVelocity().y;
  }
  ASSERT_LT(landing, -5);
  EXPECT_NEAR(leaving, 0.5 * (-landing - 9.81 / 60), 1e-3);
}

// ---------------------------------------------------------------------------
// Joints as constraints, when resolving contacts together

namespace {

// A body fixed in place (infinite mass), to hang joints from
std::unique_ptr<RigidBody> fixedAt(const Vector3 &position) {
  auto body = bodyAt(position);
  body->setInverseMass(0);
  body->setInverseInertiaTensor(Matrix3(0, 0, 0, 0, 0, 0, 0, 0, 0));
  body->calculateDerivedData();
  return body;
}

// How far apart a joint's two anchor points are. (The resolver moves
// bodies without updating their transforms, which the next step's
// calculateDerivedData does, so update them first.)
double anchorGap(const Joint &joint) {
  joint.body[0]->calculateDerivedData();
  joint.body[1]->calculateDerivedData();
  return (joint.body[1]->getPointInWorldSpace(joint.position[1]) -
          joint.body[0]->getPointInWorldSpace(joint.position[0]))
      .magnitude();
}

// Bodies hanging from a fixed pivot at the origin by a chain of joints, in
// a World with no contacts at all: link i is a 1 kg body 1 m further along
// x, joined to the one before at the point halfway between them. Gravity
// pulls them down from level.
struct Chain {
  World world{16};
  std::unique_ptr<RigidBody> pivot = fixedAt(Vector3());
  std::vector<std::unique_ptr<RigidBody>> links;
  std::vector<Joint> joints;

  Chain(int count, double slack) {
    world.getResolver().setMethod(together);
    joints.resize(count);
    for (int i = 0; i < count; ++i) {
      links.push_back(bodyAt(Vector3(i + 1.0, 0, 0)));
      RigidBody &link = *links.back();
      Matrix3 inertia;
      inertia.setDiagonal(0.01, 0.01, 0.01); // almost a point mass
      link.setInertiaTensor(inertia);
      link.setAcceleration(Vector3::GRAVITY);
      link.calculateDerivedData();
      world.getBodies().push_back(&link);
      RigidBody &previous = i == 0 ? *pivot : *links[i - 1];
      joints[i].set(&previous, Vector3(i == 0 ? 0.0 : 0.5, 0, 0), &link, Vector3(-0.5, 0, 0),
                    slack);
      if (i == 0)
        joints[i].position[1] = Vector3(-1, 0, 0); // the first hangs right from the pivot
      world.getJoints().push_back(&joints[i]);
    }
  }

  // Runs for `seconds` in steps of `step`, returning the widest any joint
  // opened
  double run(double seconds, double step = 1.0 / 60) {
    double widest = 0;
    for (int i = 0; i < static_cast<int>(std::lround(seconds / step)); ++i) {
      world.startFrame();
      world.runPhysics(step);
      for (const Joint &joint : joints)
        widest = std::max(widest, anchorGap(joint));
    }
    return widest;
  }
};

} // namespace

// A ball-and-socket joint (no slack) holds a pendulum to its pivot: the
// anchors stay together and the bob swings down through the bottom. Each
// step takes out the speed that would carry the bob off its circle, which
// also takes a little of its swing (numerical damping, as in any
// velocity-level solver; Box2D v3 takes smaller sub-steps against it), so
// it reaches the bottom about 6% slower than a real pendulum at 60 steps a
// second, and closer to it with smaller steps.
TEST(JointConstraintTest, PendulumHoldsToItsPivot) {
  // Falling 1 m: v = sqrt(2 g h / (1 + I / m L^2)), I being the bob's own
  // inertia (0.01)
  const double ideal = std::sqrt(2 * 9.81 / 1.01);
  double shortfall[2];
  for (int stepsPerSecond : {60, 240}) {
    SCOPED_TRACE(testing::Message() << stepsPerSecond << " steps a second");
    Chain pendulum(1, 0);
    RigidBody &bob = *pendulum.links[0];
    double widest = 0, fastest = 0, lowest = 0;
    for (int step = 0; step < 2 * stepsPerSecond; ++step) {
      widest = std::max(widest, pendulum.run(1.0 / stepsPerSecond, 1.0 / stepsPerSecond));
      fastest = std::max<double>(fastest, bob.getVelocity().magnitude());
      lowest = std::min<double>(lowest, bob.getPosition().y);
    }
    EXPECT_LT(widest, 0.002);
    EXPECT_NEAR(lowest, -1.0, 0.01);
    shortfall[stepsPerSecond == 60 ? 0 : 1] = 1 - fastest / ideal;
  }
  EXPECT_LT(shortfall[0], 0.07);
  EXPECT_LT(shortfall[1], shortfall[0] / 2);
}

// Sub-steps (World::setSubSteps) are smaller steps: four a step at 60 steps a
// second swing the pendulum just as 240 whole steps a second do, so it
// loses less of its swing.
TEST(JointConstraintTest, SubStepsAreSmallerSteps) {
  Chain inSubSteps(1, 0), inSmallSteps(1, 0);
  inSubSteps.world.setSubSteps(4);
  inSubSteps.run(1, 1.0 / 60);
  inSmallSteps.run(1, 1.0 / 240);
  const RigidBody &a = *inSubSteps.links[0], &b = *inSmallSteps.links[0];
  EXPECT_NEAR((a.getPosition() - b.getPosition()).magnitude(), 0, 1e-9);
  EXPECT_NEAR((a.getVelocity() - b.getVelocity()).magnitude(), 0, 1e-9);
}

// A force added by hand before runPhysics acts through all its sub-steps:
// the frame gives the same push however it's split.
TEST(WorldTest, ForcesAddedByHandActThroughEverySubStep) {
  for (unsigned subSteps : {1u, 4u}) {
    SCOPED_TRACE(testing::Message() << subSteps << " sub-steps");
    World world(4);
    world.setSubSteps(subSteps);
    auto body = bodyAt(Vector3());
    world.getBodies().push_back(body.get());
    world.startFrame();
    body->addForce(Vector3(6, 0, 0));
    body->addTorque(Vector3(0, 6, 0));
    world.runPhysics(1.0 / 6); // 1 m/s, and 1 rad/s, from 1 kg
    EXPECT_NEAR(body->getVelocity().x, 1.0, 1e-9);
    EXPECT_NEAR(body->getRotation().y, 1.0, 1e-9);
  }
}

// Stacks stand just as still in sub-steps.
TEST(ResolveTogetherTest, StacksStandInSubSteps) {
  BoxWorld scene(stackOf(10), together);
  scene.world.setSubSteps(4);
  EXPECT_LT(scene.run(10), 2.0);
  EXPECT_LT(farthestSideways(scene), 0.1);
  EXPECT_LT(fastest(scene), 0.01);
}

// A chain of five links stays joined as it swings down and whips about.
TEST(JointConstraintTest, ChainStaysJoined) {
  Chain chain(5, 0);
  EXPECT_LT(chain.run(3), 0.01);
}

// A joint with slack is a cable: the bob falls freely until the anchors are
// the slack apart, and then the cable holds it there.
TEST(JointConstraintTest, CableIsFreeUntilTaut) {
  World world(4);
  world.getResolver().setMethod(together);
  auto pivot = fixedAt(Vector3());
  auto bob = bodyAt(Vector3(0, -0.5, 0));
  bob->setAcceleration(Vector3::GRAVITY);
  world.getBodies().push_back(bob.get());
  Joint cable;
  cable.set(pivot.get(), Vector3(), bob.get(), Vector3(), 2.0);
  world.getJoints().push_back(&cable);

  // Falling the first 1.5 m takes about 0.55 s: still falling freely at
  // 0.5 s, held from 1 s on
  for (int step = 0; step < 30; ++step) {
    world.startFrame();
    world.runPhysics(1.0 / 60);
  }
  EXPECT_NEAR(bob->getVelocity().y, -9.81 * 0.5, 0.05);
  double longest = 0;
  for (int step = 0; step < 90; ++step) {
    world.startFrame();
    world.runPhysics(1.0 / 60);
    longest = std::max(longest, anchorGap(cable));
  }
  EXPECT_LT(longest, 2.0 + 0.01);
  EXPECT_NEAR(bob->getPosition().y, -2.0, 0.01);
  EXPECT_NEAR(bob->getVelocity().magnitude(), 0, 0.01);
}

// Given joints, the book's resolver resolves the contacts they make, as if
// they had been added as contacts.
TEST(JointConstraintTest, BooksResolverTakesJointsAsContacts) {
  auto viaJoints = bodyAt(Vector3(2, 0, 0)), viaContacts = bodyAt(Vector3(2, 0, 0));
  auto pivotA = fixedAt(Vector3()), pivotB = fixedAt(Vector3());
  Joint jointA, jointB;
  jointA.set(pivotA.get(), Vector3(), viaJoints.get(), Vector3(), 0.5);
  jointB.set(pivotB.get(), Vector3(), viaContacts.get(), Vector3(), 0.5);

  ContactResolver resolver = booksResolver(10);
  Joint *joints[] = {&jointA};
  resolver.resolveContacts(nullptr, 0, 1.0 / 60, joints);
  Contact contact;
  ASSERT_EQ(jointB.addContact(&contact, 1), 1u);
  booksResolver(10).resolveContacts(&contact, 1, 1.0 / 60);
  EXPECT_NEAR(viaJoints->getPosition().x, viaContacts->getPosition().x, kTolerance);
  EXPECT_NEAR(viaJoints->getPosition().y, viaContacts->getPosition().y, kTolerance);
  EXPECT_LT(viaJoints->getPosition().x, 2.0); // pulled back towards the pivot
}

// ---------------------------------------------------------------------------
// Hinges and cones: joints that limit turning (angular constraints)

namespace {

// A 1 kg rod 1 m from a fixed pivot at the origin, joined to it by
// `joint` (a HingeJoint or ConeJoint) at the pivot, in a World of its own:
// a pendulum, or with no gravity a rotor
template <typename JointType> struct Swing {
  World world{8};
  std::unique_ptr<RigidBody> pivot = fixedAt(Vector3());
  std::unique_ptr<RigidBody> bob;
  JointType joint;

  Swing(const Vector3 &bobAt, bool gravity) : bob(bodyAt(bobAt)) {
    world.getResolver().setMethod(together);
    Matrix3 inertia;
    inertia.setDiagonal(0.1, 0.1, 0.1);
    bob->setInertiaTensor(inertia);
    if (gravity)
      bob->setAcceleration(Vector3::GRAVITY);
    bob->calculateDerivedData();
    world.getBodies().push_back(bob.get());
    joint.set(pivot.get(), Vector3(), bob.get(), bobAt * -1, 0);
    world.getJoints().push_back(&joint);
  }

  void run(double seconds, const std::function<void()> &afterEachStep = [] {}) {
    for (int step = 0; step < static_cast<int>(std::lround(seconds * 60)); ++step) {
      world.startFrame();
      world.runPhysics(1.0 / 60);
      bob->calculateDerivedData();
      afterEachStep();
    }
  }
};

// How far past an angle limit the position sweeps let a joint go before
// turning it back (ContactResolver::angularSlop, 2 degrees)
constexpr double kAngularSlop = 0.035;

// The angle (rad) between a hinge's or cone's axis on each body, now
template <typename JointType> double axesApart(const JointType &joint) {
  const Vector3 one = joint.body[0]->getDirectionInWorldSpace(joint.axis[0]);
  const Vector3 two = joint.body[1]->getDirectionInWorldSpace(joint.axis[1]);
  return std::atan2((one % two).magnitude(), one * two);
}

} // namespace

// A hinge lets its bodies turn only about its axis: a pendulum hinged about
// z, set spinning about x and moving along z as well, still swings down in
// the x-y plane, its axis held in line and its anchor held to the pivot.
TEST(HingeJointTest, TurnsOnlyAboutItsAxis) {
  Swing<HingeJoint> swing(Vector3(1, 0, 0), true);
  swing.joint.setAxis(Vector3(0, 0, 1));
  swing.bob->setRotation(Vector3(3, 0, 0));
  swing.bob->setVelocity(Vector3(0, 0, 1));
  double worstApart = 0, farthestOff = 0, lowest = 0;
  swing.run(2, [&] {
    worstApart = std::max(worstApart, axesApart(swing.joint));
    farthestOff = std::max<double>(farthestOff, std::abs(swing.bob->getPosition().z));
    lowest = std::min<double>(lowest, swing.bob->getPosition().y);
  });
  EXPECT_LT(worstApart, 0.02);
  EXPECT_LT(farthestOff, 0.02);
  EXPECT_LT(anchorGap(swing.joint), 0.002);
  EXPECT_NEAR(lowest, -1.0, 0.02);
}

// The angle is zero as the hinge is set up and grows turning about its axis
// by the right-hand rule: the pendulum falling from level turns about -z.
TEST(HingeJointTest, AngleFollowsTheRightHandRule) {
  Swing<HingeJoint> swing(Vector3(1, 0, 0), true);
  swing.joint.setAxis(Vector3(0, 0, 1));
  EXPECT_NEAR(swing.joint.getAngle(), 0, kTolerance);
  swing.run(0.2);
  EXPECT_LT(swing.joint.getAngle(), -0.1);
}

// A limit stops the swing: falling from level, the pendulum stops at its
// lower limit (-0.5 rad) rather than swinging down, going past it by no
// more than the slop the position sweeps allow, and comes to rest there.
TEST(HingeJointTest, LimitStopsTheSwing) {
  Swing<HingeJoint> swing(Vector3(1, 0, 0), true);
  swing.joint.setAxis(Vector3(0, 0, 1));
  swing.joint.setLimits(-0.5, 0.5);
  double lowestAngle = 0;
  swing.run(3, [&] { lowestAngle = std::min<double>(lowestAngle, swing.joint.getAngle()); });
  EXPECT_GT(lowestAngle, -0.5 - kAngularSlop);
  EXPECT_NEAR(swing.joint.getAngle(), -0.5, 0.01);
  EXPECT_LT(swing.bob->getRotation().magnitude(), 0.05);
}

// A motor drives the hinge at its speed when it's strong enough, and when
// it isn't, turns it as fast as its torque can: torque / moment of inertia
// about the pivot (the rod's own 0.1 plus 1 kg at 1 m).
TEST(HingeJointTest, MotorReachesItsSpeedOrItsTorqueLimit) {
  Swing<HingeJoint> strong(Vector3(1, 0, 0), false), weak(Vector3(1, 0, 0), false);
  for (HingeJoint *joint : {&strong.joint, &weak.joint})
    joint->setAxis(Vector3(0, 0, 1));
  strong.joint.setMotor(2, 100);
  weak.joint.setMotor(2, 0.5);
  strong.run(0.1);
  weak.run(1);
  EXPECT_NEAR(strong.bob->getRotation().z, 2, 0.01);
  EXPECT_NEAR(weak.bob->getRotation().z, 0.5 / 1.1, 0.01);
}

// A cone joint lets the bob swing any way, but no further than its angle
// from the axis: hanging straight down and knocked sideways hard enough to
// swing to 1.2 rad, it stops at 0.3, past it by no more than the slop.
TEST(ConeJointTest, LimitsTheTilt) {
  Swing<ConeJoint> swing(Vector3(0, -1, 0), true);
  swing.joint.setAxis(Vector3(0, -1, 0), 0.3);
  swing.bob->setVelocity(Vector3(2, 0, 2));
  double widest = 0;
  swing.run(2, [&] { widest = std::max<double>(widest, swing.joint.getAngle()); });
  EXPECT_LT(widest, 0.3 + kAngularSlop + 0.01);
  EXPECT_GT(widest, 0.25);
  EXPECT_LT(anchorGap(swing.joint), 0.002);
}
