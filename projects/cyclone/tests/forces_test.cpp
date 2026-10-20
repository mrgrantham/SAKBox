// Cyclone's force generators, checked against the formulas they implement,
// and the registries that apply them.
// Each test applies one force and reads it back: from a rigid body's
// acceleration over the step (mass 1), or from a particle's velocity change
// (mass 1, one 1 s step, no damping).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/pfgen.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

void expectNear(const Vector3 &actual, const Vector3 &expected, double tolerance = kTolerance) {
  EXPECT_NEAR(actual.x, expected.x, tolerance);
  EXPECT_NEAR(actual.y, expected.y, tolerance);
  EXPECT_NEAR(actual.z, expected.z, tolerance);
}

std::unique_ptr<RigidBody> bodyAt(const Vector3 &position, double mass = 1) {
  auto body = std::make_unique<RigidBody>();
  body->setMass(mass);
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
  body->clearAccumulators();
  return body;
}

// The force `generator` puts on `body`, read back as mass x acceleration.
Vector3 forceOn(RigidBody &body, ForceGenerator &generator) {
  body.clearAccumulators();
  generator.updateForce(&body, 0.01);
  body.integrate(0.01);
  Vector3 acceleration;
  body.getLastFrameAcceleration(&acceleration);
  return acceleration * body.getMass();
}

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

// The force `generator` puts on `particle` (mass 1): its velocity change
// over a 1 s step.
Vector3 forceOn(Particle &particle, ParticleForceGenerator &generator) {
  const Vector3 before = particle.getVelocity();
  particle.clearAccumulator();
  generator.updateForce(&particle, 1.0);
  particle.integrate(1.0);
  return particle.getVelocity() - before;
}

} // namespace

// ---------------------------------------------------------------------------
// Rigid bodies

TEST(RigidForceTest, GravityIsMassTimesG) {
  auto body = bodyAt(Vector3(), 3.0);
  Gravity gravity(Vector3(0, -9.81, 0));
  expectNear(forceOn(*body, gravity), Vector3(0, -3 * 9.81, 0));
}

// A spring of rest length 2 and stiffness 10 between two bodies: stretched to
// 3 it pulls them together with 10 N; squashed to 1 it pushes them apart
// with 10 N; at 2 it does nothing.
TEST(RigidForceTest, SpringPullsWhenStretchedAndPushesWhenSquashed) {
  auto anchor = bodyAt(Vector3(0, 0, 0));
  for (auto [x, expected] : {std::pair{3.0, -10.0}, std::pair{1.0, 10.0}, std::pair{2.0, 0.0}}) {
    auto body = bodyAt(Vector3(x, 0, 0));
    Spring spring(Vector3(), anchor.get(), Vector3(), 10, 2);
    expectNear(forceOn(*body, spring), Vector3(expected, 0, 0));
  }
}

// Buoyancy with the water surface at 0, a 1 m depth band either side of it
// and 2 m^3 of volume (Cyclone's buoyancy is density x volume x the fraction
// submerged: kilograms of water displaced, without g).
TEST(RigidForceTest, BuoyancyRisesSmoothlyWithDepth) {
  struct Case {
    double y, expectedUp;
  };
  for (Case c : {Case{1.5, 0}, Case{1.0, 0}, Case{0.5, 500}, Case{0.0, 1000}, Case{-0.5, 1500},
                 Case{-1.0, 2000}, Case{-3.0, 2000}}) {
    auto body = bodyAt(Vector3(0, c.y, 0));
    Buoyancy buoyancy(Vector3(), /*maxDepth=*/1, /*volume=*/2,
                      /*waterHeight=*/0);
    expectNear(forceOn(*body, buoyancy), Vector3(0, c.expectedUp, 0), 1e-6);
  }
}

// The sailboat demo's hull and buoyancy: dropped onto the water it settles
// where buoyancy holds up its weight (2000 x fraction = 200 x 9.81 kg-force),
// instead of sinking and bobbing below the surface.
TEST(RigidForceTest, SailboatFloats) {
  RigidBody boat;
  boat.setPosition(0, 1.6, 0);
  boat.setOrientation(1, 0, 0, 0);
  boat.setVelocity(0, 0, 0);
  boat.setRotation(0, 0, 0);
  boat.setMass(200);
  Matrix3 inertia;
  inertia.setBlockInertiaTensor(Vector3(2, 1, 1), 100);
  boat.setInertiaTensor(inertia);
  boat.setDamping(0.8, 0.8);
  boat.setAcceleration(Vector3::GRAVITY);
  boat.setCanSleep(false);
  boat.calculateDerivedData();
  Buoyancy buoyancy(Vector3(0, 0.5, 0), 1.0, 3.0, 1.6);
  ForceRegistry registry;
  registry.add(&boat, &buoyancy);

  double lowest = 1.6;
  // It bobs every ~2.3 s; the demo's light damping (0.8/s) needs a minute
  for (int i = 0; i < 60 * 60; ++i) {
    boat.clearAccumulators();
    registry.updateForces(1.0 / 60);
    boat.integrate(1.0 / 60);
    lowest = std::min<double>(lowest, boat.getPosition().y);
  }
  // Settled: 3000 x fraction = 200 x 9.81, fraction = (1.6 + 1 - depth) / 2
  const double depth = 2.6 - 2 * (200 * 9.81 / 3000);
  EXPECT_NEAR(boat.getPosition().y, depth - 0.5, 0.01);
  EXPECT_NEAR(boat.getVelocity().y, 0, 0.01);
  EXPECT_GT(lowest, 0.0); // never went under (it used to sink to -4)
}

// ---------------------------------------------------------------------------
// Aerodynamics: force = tensor x the body's velocity through the air, both in
// body coordinates.

namespace {
const Vector3 kStill(0, 0, 0);

// A body rotated `degrees` about `axis`
std::unique_ptr<RigidBody> bodyFacing(const Vector3 &axis, double degrees) {
  auto body = bodyAt(Vector3());
  const double half = degrees * R_PI / 360;
  Vector3 unit = axis.unit();
  body->setOrientation(Quaternion(std::cos(half), unit.x * std::sin(half), unit.y * std::sin(half),
                                  unit.z * std::sin(half)));
  body->calculateDerivedData();
  return body;
}
} // namespace

// Drag the same in every direction (tensor -2 I) opposes the motion, however
// the body is turned.
TEST(AeroTest, DragOpposesTheMotionWhicheverWayTheBodyFaces) {
  Matrix3 drag;
  drag.setDiagonal(-2, -2, -2);
  Aero aero(drag, Vector3(), &kStill);
  for (double degrees : {0.0, 30.0, 90.0, 180.0}) {
    SCOPED_TRACE(testing::Message() << "turned " << degrees << " degrees");
    auto body = bodyFacing(Vector3(1, 1, 0), degrees);
    body->setVelocity(3, 0, 0);
    expectNear(forceOn(*body, aero), Vector3(-6, 0, 0));
  }
}

// A still body in a 5 m/s wind is pushed downwind. (Upstream added the wind
// to the body's velocity, pushing it into the wind.)
TEST(AeroTest, WindPushesAStillBodyDownwind) {
  Matrix3 drag;
  drag.setDiagonal(-2, -2, -2);
  const Vector3 wind(5, 0, 0);
  Aero aero(drag, Vector3(), &wind);
  auto body = bodyAt(Vector3());
  expectNear(forceOn(*body, aero), Vector3(10, 0, 0));

  // Moving with the wind, there's no airflow and no force
  body->setVelocity(5, 0, 0);
  expectNear(forceOn(*body, aero), Vector3(0, 0, 0));
}

// A wing whose lift (body y) is its forward speed (body x): rolled 90
// degrees about its forward axis, the lift points sideways.
TEST(AeroTest, LiftTurnsWithTheBody) {
  const Matrix3 wing(0, 0, 0, 1, 0, 0, 0, 0, 0);
  Aero aero(wing, Vector3(), &kStill);
  auto level = bodyFacing(Vector3(1, 0, 0), 0);
  level->setVelocity(2, 0, 0);
  expectNear(forceOn(*level, aero), Vector3(0, 2, 0));

  auto rolled = bodyFacing(Vector3(1, 0, 0), 90);
  rolled->setVelocity(2, 0, 0);
  expectNear(forceOn(*rolled, aero), Vector3(0, 0, 2));
}

// The surface 1 m out along x: its 2 N of lift also turns the body, with
// torque r x F = (1, 0, 0) x (0, 2, 0) about z (inertia 1, one 0.01 s step).
TEST(AeroTest, ASurfaceOffTheCentreTurnsTheBody) {
  const Matrix3 wing(0, 0, 0, 1, 0, 0, 0, 0, 0);
  Aero aero(wing, Vector3(1, 0, 0), &kStill);
  auto body = bodyAt(Vector3());
  body->setVelocity(2, 0, 0);
  expectNear(forceOn(*body, aero), Vector3(0, 2, 0));
  expectNear(body->getRotation(), Vector3(0, 0, 2 * 0.01));
}

// A control surface's tensor runs from its minimum (control -1) through its
// base (0) to its maximum (+1), linearly in between and held at the ends.
TEST(AeroControlTest, ControlBlendsBetweenItsTensors) {
  Matrix3 base, min, max;
  base.setDiagonal(-2, 0, 0);
  min.setDiagonal(-1, 0, 0);
  max.setDiagonal(-4, 0, 0);
  AeroControl surface(base, min, max, Vector3(), &kStill);
  struct Case {
    double control, expectedForce;
  };
  for (Case c : {Case{-2, -1}, Case{-1, -1}, Case{-0.5, -1.5}, Case{0, -2}, Case{0.5, -3},
                 Case{1, -4}, Case{2, -4}}) {
    SCOPED_TRACE(testing::Message() << "control " << c.control);
    surface.setControl(c.control);
    auto body = bodyAt(Vector3());
    body->setVelocity(1, 0, 0);
    expectNear(forceOn(*body, surface), Vector3(c.expectedForce, 0, 0));
  }
}

// ---------------------------------------------------------------------------
// Explosions: an implosion, a shock wave and a convection chimney (the
// book's section 17.2.3). Each test turns off the stages it isn't about.

namespace {

// An explosion at the origin with only the given stages
Explosion explosionWith(bool implosion, bool concussion, bool convection) {
  Explosion explosion;
  if (!implosion)
    explosion.implosionForce = 0;
  if (!concussion)
    explosion.peakConcussionForce = 0;
  if (!convection)
    explosion.peakConvectionForce = 0;
  return explosion;
}

// The explosion's force on a still particle (mass 1) at `position`
Vector3 explosionForceAt(Explosion &explosion, const Vector3 &position,
                         const Vector3 &velocity = Vector3()) {
  auto particle = particleAt(position);
  particle->setVelocity(velocity);
  return forceOn(*particle, explosion);
}

} // namespace

// The implosion pulls objects between its two radii (1 and 4 m) towards
// the detonation with implosionForce, for implosionDuration (0.1 s).
TEST(ExplosionTest, ImplosionPullsInObjectsBetweenItsRadii) {
  Explosion explosion = explosionWith(true, false, false);
  expectNear(explosionForceAt(explosion, Vector3(2, 0, 0)), Vector3(-20, 0, 0));
  expectNear(explosionForceAt(explosion, Vector3(0, 3, 0)), Vector3(0, -20, 0));
  expectNear(explosionForceAt(explosion, Vector3(0.5, 0, 0)), Vector3()); // too close
  expectNear(explosionForceAt(explosion, Vector3(5, 0, 0)), Vector3());   // too far
  explosion.advanceTime(0.1);
  expectNear(explosionForceAt(explosion, Vector3(2, 0, 0)), Vector3()); // over
}

// Half a second in, a 10 m/s wave 1 m thick spans 5-6 m. Inside it a still
// object gets the full push outwards; it falls off linearly over 1 m either
// side.
TEST(ExplosionTest, ShockWavePushesHardestInsideTheWave) {
  Explosion explosion = explosionWith(false, true, false);
  explosion.shockwaveSpeed = 10;
  explosion.peakConcussionForce = 100;
  explosion.concussionDuration = 1e9; // (no fading)
  explosion.advanceTime(0.5);
  struct Case {
    double distance, expected;
  };
  for (Case c : {Case{3.9, 0}, Case{4.2, 20}, Case{4.5, 50}, Case{5.0, 100}, Case{5.5, 100},
                 Case{6.2, 80}, Case{6.5, 50}, Case{7.1, 0}}) {
    SCOPED_TRACE(testing::Message() << c.distance << " m out");
    expectNear(explosionForceAt(explosion, Vector3(c.distance, 0, 0)), Vector3(c.expected, 0, 0),
               1e-6);
    expectNear(explosionForceAt(explosion, Vector3(0, 0, -c.distance)), Vector3(0, 0, -c.expected),
               1e-6);
  }
}

// The wave is moving air: an object already moving outwards gets less push
// (none if it keeps up with the wave), one moving inwards gets more.
TEST(ExplosionTest, ShockWavePushesLessOnObjectsMovingWithIt) {
  Explosion explosion = explosionWith(false, true, false);
  explosion.shockwaveSpeed = 10;
  explosion.peakConcussionForce = 100;
  explosion.concussionDuration = 1e9;
  explosion.advanceTime(0.5);
  const Vector3 inTheWave(5.5, 0, 0);
  expectNear(explosionForceAt(explosion, inTheWave, Vector3(5, 0, 0)), Vector3(50, 0, 0), 1e-6);
  expectNear(explosionForceAt(explosion, inTheWave, Vector3(10, 0, 0)), Vector3(), 1e-6);
  expectNear(explosionForceAt(explosion, inTheWave, Vector3(20, 0, 0)), Vector3(), 1e-6);
  expectNear(explosionForceAt(explosion, inTheWave, Vector3(-5, 0, 0)), Vector3(150, 0, 0), 1e-6);
  // Moving across the wave makes no difference
  expectNear(explosionForceAt(explosion, inTheWave, Vector3(0, 7, 0)), Vector3(100, 0, 0), 1e-6);
}

// The wave weakens as it goes, to nothing at concussionDuration.
TEST(ExplosionTest, ShockWaveFadesOverItsDuration) {
  Explosion explosion = explosionWith(false, true, false);
  explosion.shockwaveSpeed = 10;
  explosion.peakConcussionForce = 100;
  explosion.concussionDuration = 2;
  explosion.advanceTime(0.5); // a quarter of the way
  expectNear(explosionForceAt(explosion, Vector3(5.5, 0, 0)), Vector3(75, 0, 0), 1e-6);
  explosion.advanceTime(1.5);
  expectNear(explosionForceAt(explosion, Vector3(20.5, 0, 0)), Vector3(), 1e-6);
}

// The chimney (2 m across, 10 m tall) pushes up its middle with
// peakConvectionForce, along the line from the detonation off the middle
// (the mushroom shape), less towards its wall, and nothing outside it.
TEST(ExplosionTest, ChimneyLiftsObjectsAboveTheBlast) {
  Explosion explosion = explosionWith(false, false, true);
  expectNear(explosionForceAt(explosion, Vector3(0, 5, 0)), Vector3(0, 5, 0));
  // Halfway to the wall, 1 m up: half the force, at 45 degrees
  const Vector3 halfway = explosionForceAt(explosion, Vector3(1, 1, 0));
  expectNear(halfway, Vector3(1, 1, 0) * (2.5 / std::sqrt(2.0)));
  expectNear(explosionForceAt(explosion, Vector3(2.5, 1, 0)), Vector3()); // outside
  expectNear(explosionForceAt(explosion, Vector3(0, 11, 0)), Vector3());  // above
  expectNear(explosionForceAt(explosion, Vector3(0, -1, 0)), Vector3());  // below
  explosion.advanceTime(4);
  expectNear(explosionForceAt(explosion, Vector3(0, 5, 0)), Vector3()); // over
  EXPECT_FALSE(explosion.isActive());
  explosion.restart();
  EXPECT_TRUE(explosion.isActive());
  expectNear(explosionForceAt(explosion, Vector3(0, 5, 0)), Vector3(0, 5, 0));
}

// The wave pushes a rigid body off its centre, so it spins as well as
// flying outwards, at the same point of the body each time. With no offset
// it only flies.
TEST(ExplosionTest, ShockWaveSpinsRigidBodies) {
  Explosion explosion = explosionWith(false, true, false);
  auto body = bodyAt(Vector3(1.5, 0, 0)); // in the wave at the start
  const Vector3 push = forceOn(*body, explosion);
  EXPECT_GT(push.x, 0);
  const Vector3 spin = body->getRotation();
  EXPECT_GT(spin.magnitude(), 0);

  // Again from rest: the same push and the same spin
  body->setPosition(1.5, 0, 0);
  body->setVelocity(0, 0, 0);
  body->setRotation(Vector3());
  body->setOrientation(1, 0, 0, 0);
  body->calculateDerivedData();
  expectNear(forceOn(*body, explosion), push);
  expectNear(body->getRotation(), spin);

  explosion.concussionOffset = 0;
  auto other = bodyAt(Vector3(1.5, 0, 0));
  expectNear(forceOn(*other, explosion), push);
  expectNear(other->getRotation(), Vector3());
}

// The whole explosion on a ring of particles 3 m out on the ground: pulled
// in a little, then thrown outwards by the wave, none faster than it.
TEST(ExplosionTest, ThrowsARingOfObjectsOutwards) {
  Explosion explosion;
  std::vector<std::unique_ptr<Particle>> ring;
  for (int i = 0; i < 8; ++i) {
    const double angle = i * R_PI / 4;
    ring.push_back(particleAt(Vector3(3 * std::cos(angle), 0, 3 * std::sin(angle))));
  }
  bool pulledIn = false;
  for (int step = 0; step < 60; ++step) {
    for (auto &particle : ring) {
      particle->clearAccumulator();
      explosion.updateForce(particle.get(), 1.0 / 60);
      particle->integrate(1.0 / 60);
    }
    explosion.advanceTime(1.0 / 60);
    pulledIn = pulledIn || ring[0]->getVelocity().x < 0;
  }
  EXPECT_TRUE(pulledIn);
  for (auto &particle : ring) {
    const Vector3 outwards = particle->getPosition().unit();
    const double speed = particle->getVelocity() * outwards;
    EXPECT_GT(speed, 1.0);
    EXPECT_LT(speed, explosion.shockwaveSpeed);
    EXPECT_NEAR((particle->getVelocity() - outwards * speed).magnitude(), 0, 1e-6);
  }
}

// ---------------------------------------------------------------------------
// The force registries

// Each registered pair applies its force; remove takes out just that pair
// (and ignores a pair that isn't registered); clear takes out all of them.
TEST(ForceRegistryTest, RemoveAndClear) {
  auto body = bodyAt(Vector3(0, 0, 0)), anchor = bodyAt(Vector3(-3, 0, 0));
  Gravity gravity(Vector3(0, -10, 0));
  Spring spring(Vector3(), anchor.get(), Vector3(), 10, 2); // 1 m stretched
  ForceRegistry registry;
  registry.add(body.get(), &gravity);
  registry.add(body.get(), &spring);
  auto forceNow = [&] {
    body->clearAccumulators();
    registry.updateForces(0.01);
    body->integrate(0.01);
    body->setPosition(0, 0, 0);
    body->setVelocity(0, 0, 0);
    body->calculateDerivedData();
    Vector3 acceleration;
    body->getLastFrameAcceleration(&acceleration);
    return acceleration;
  };
  expectNear(forceNow(), Vector3(-10, -10, 0));
  registry.remove(anchor.get(), &spring); // not registered
  expectNear(forceNow(), Vector3(-10, -10, 0));
  registry.remove(body.get(), &spring);
  expectNear(forceNow(), Vector3(0, -10, 0));
  registry.clear();
  expectNear(forceNow(), Vector3(0, 0, 0));
}

TEST(ForceRegistryTest, ParticleRemoveAndClear) {
  auto particle = particleAt(Vector3()), other = particleAt(Vector3());
  ParticleGravity gravity(Vector3(0, -10, 0));
  ParticleDrag drag(1, 0);
  ParticleForceRegistry registry;
  registry.add(particle.get(), &gravity);
  registry.add(particle.get(), &drag);
  auto forceNow = [&] {
    particle->setVelocity(1, 0, 0);
    particle->clearAccumulator();
    registry.updateForces(1);
    particle->integrate(1);
    return particle->getVelocity() - Vector3(1, 0, 0);
  };
  expectNear(forceNow(), Vector3(-1, -10, 0));
  registry.remove(other.get(), &drag); // not registered
  expectNear(forceNow(), Vector3(-1, -10, 0));
  registry.remove(particle.get(), &drag);
  expectNear(forceNow(), Vector3(0, -10, 0));
  registry.clear();
  expectNear(forceNow(), Vector3(0, 0, 0));
}

// ---------------------------------------------------------------------------
// Particles

TEST(ParticleForceTest, GravityIsMassTimesG) {
  auto particle = particleAt(Vector3());
  ParticleGravity gravity(Vector3(0, -9.81, 0));
  expectNear(forceOn(*particle, gravity), Vector3(0, -9.81, 0));
}

// Drag k1 x speed + k2 x speed^2, against the motion.
TEST(ParticleForceTest, DragOpposesMotion) {
  auto particle = particleAt(Vector3());
  ParticleDrag drag(0.1, 0.01);
  particle->setVelocity(0, 0, 10); // 0.1 x 10 + 0.01 x 100 = 2
  expectNear(forceOn(*particle, drag), Vector3(0, 0, -2));
}

TEST(ParticleForceTest, SpringsPullWhenStretchedAndPushWhenSquashed) {
  for (auto [x, expected] : {std::pair{3.0, -10.0}, std::pair{1.0, 10.0}, std::pair{2.0, 0.0}}) {
    auto other = particleAt(Vector3(0, 0, 0));
    auto particle = particleAt(Vector3(x, 0, 0));
    ParticleSpring spring(other.get(), 10, 2);
    {
      SCOPED_TRACE(testing::Message() << "spring at " << x);
      expectNear(forceOn(*particle, spring), Vector3(expected, 0, 0));
    }

    Vector3 anchor(0, 0, 0);
    particle->setPosition(x, 0, 0);
    particle->setVelocity(0, 0, 0);
    ParticleAnchoredSpring anchored(&anchor, 10, 2);
    {
      SCOPED_TRACE(testing::Message() << "anchored spring at " << x);
      expectNear(forceOn(*particle, anchored), Vector3(expected, 0, 0));
    }
  }
}

// A bungee is a spring that only pulls: stretched it pulls back, slack it
// does nothing.
TEST(ParticleForceTest, BungeesOnlyPull) {
  for (auto [x, expected] : {std::pair{3.0, -10.0}, std::pair{1.0, 0.0}}) {
    auto other = particleAt(Vector3(0, 0, 0));
    auto particle = particleAt(Vector3(x, 0, 0));
    ParticleBungee bungee(other.get(), 10, 2);
    {
      SCOPED_TRACE(testing::Message() << "bungee at " << x);
      expectNear(forceOn(*particle, bungee), Vector3(expected, 0, 0));
    }

    Vector3 anchor(0, 0, 0);
    particle->setPosition(x, 0, 0);
    particle->setVelocity(0, 0, 0);
    ParticleAnchoredBungee anchored;
    anchored.init(&anchor, 10, 2);
    {
      SCOPED_TRACE(testing::Message() << "anchored bungee at " << x);
      expectNear(forceOn(*particle, anchored), Vector3(expected, 0, 0));
    }
  }
}

TEST(ParticleForceTest, BuoyancyRisesSmoothlyWithDepth) {
  struct Case {
    double y, expectedUp;
  };
  for (Case c : {Case{1.5, 0}, Case{0.5, 0.25}, Case{0.0, 0.5}, Case{-0.5, 0.75}, Case{-3.0, 1}}) {
    auto particle = particleAt(Vector3(0, c.y, 0));
    ParticleBuoyancy buoyancy(/*maxDepth=*/1, /*volume=*/1, /*waterHeight=*/0,
                              /*liquidDensity=*/1);
    {
      SCOPED_TRACE(testing::Message() << "y " << c.y);
      expectNear(forceOn(*particle, buoyancy), Vector3(0, c.expectedUp, 0));
    }
  }
}

// A fake spring (stiffness 4, damping 2) pulls a particle 1 m out back onto
// its anchor, settling there. (A real spring would be within 1e-4 m in 5 s;
// this one lags, as Particle::integrate moves with the old velocity, and
// takes about 15.)
TEST(ParticleForceTest, FakeSpringSettlesOnItsAnchor) {
  Vector3 anchor(0, 0, 0);
  ParticleFakeSpring spring(&anchor, 4, 2);
  auto particle = particleAt(Vector3(1, 0, 0));
  double closest = 1;
  for (int i = 0; i < 60 * 20; ++i) {
    particle->clearAccumulator();
    spring.updateForce(particle.get(), 1.0 / 60);
    particle->integrate(1.0 / 60);
    closest = std::min(closest, particle->getPosition().x);
  }
  EXPECT_LT(closest, 0.0); // it swung through the anchor
  EXPECT_NEAR(particle->getPosition().x, 0, 1e-3);
  EXPECT_NEAR(particle->getVelocity().x, 0, 1e-3);
}

// Too much damping to oscillate (damping^2 > 4 x stiffness) is outside what
// the fake spring models: no force, rather than NaN.
TEST(ParticleForceTest, OverdampedFakeSpringGivesNoForce) {
  Vector3 anchor(0, 0, 0);
  ParticleFakeSpring spring(&anchor, 1, 3);
  auto particle = particleAt(Vector3(1, 0, 0));
  expectNear(forceOn(*particle, spring), Vector3(0, 0, 0));
}
