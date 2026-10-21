// Cyclone's contact handling: the rigid-body resolver (bounce and push
// apart), joints, the particle links, constraints and world, and the
// rigid-body world (boxes landing and stacking).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/plinks.h>
#include <cyclone/world.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

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

// Contacts for a world of boxes: each against the ground (y = 0) and every
// pair against each other, the pairs' contacts kept from frame to frame
class BoxContacts : public ContactGenerator {
public:
  std::vector<CollisionBox> boxes;
  mutable BoxContactCache cache;

  unsigned addContact(Contact *contact, unsigned limit) const override {
    CollisionData data;
    data.contactArray = contact;
    data.friction = 0.9;
    data.restitution = 0.1;
    data.tolerance = 0.1;
    data.reset(limit);
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    for (size_t i = 0; i < boxes.size() && data.hasMoreContacts(); ++i) {
      CollisionDetector::boxAndHalfSpace(boxes[i], ground, &data);
      for (size_t j = i + 1; j < boxes.size() && data.hasMoreContacts(); ++j)
        cache.boxAndBox(boxes[i], boxes[j], &data);
    }
    return data.contactCount;
  }
};

// Unit cubes (mass 1) at the given heights, falling under a gravity
// generator registered with the world
struct BoxWorld {
  World world{256};
  Gravity gravity{Vector3::GRAVITY};
  std::vector<std::unique_ptr<RigidBody>> bodies;
  BoxContacts contacts;

  explicit BoxWorld(const std::vector<Vector3> &positions) {
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
      CollisionBox box;
      box.body = &body;
      box.halfSize = Vector3(0.5, 0.5, 0.5);
      contacts.boxes.push_back(box);
    }
    world.getContactGenerators().push_back(&contacts);
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
    for (CollisionBox &box : contacts.boxes)
      box.calculateInternals();
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
