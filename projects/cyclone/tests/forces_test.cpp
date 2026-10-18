// Cyclone's force generators, checked against the formulas they implement.
// Each test applies one force and reads it back: from a rigid body's
// acceleration over the step (mass 1), or from a particle's velocity change
// (mass 1, one 1 s step, no damping).
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/pfgen.h>

#include <cmath>
#include <memory>

using namespace cyclone;

namespace {

constexpr double kTolerance = 1e-9;

void expectNear(const Vector3 &actual, const Vector3 &expected,
                double tolerance = kTolerance) {
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
  for (auto [x, expected] :
       {std::pair{3.0, -10.0}, std::pair{1.0, 10.0}, std::pair{2.0, 0.0}}) {
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
  for (Case c : {Case{1.5, 0}, Case{1.0, 0}, Case{0.5, 500}, Case{0.0, 1000},
                 Case{-0.5, 1500}, Case{-1.0, 2000}, Case{-3.0, 2000}}) {
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
  for (auto [x, expected] :
       {std::pair{3.0, -10.0}, std::pair{1.0, 10.0}, std::pair{2.0, 0.0}}) {
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
  for (Case c : {Case{1.5, 0}, Case{0.5, 0.25}, Case{0.0, 0.5},
                 Case{-0.5, 0.75}, Case{-3.0, 1}}) {
    auto particle = particleAt(Vector3(0, c.y, 0));
    ParticleBuoyancy buoyancy(/*maxDepth=*/1, /*volume=*/1, /*waterHeight=*/0,
                              /*liquidDensity=*/1);
    {
      SCOPED_TRACE(testing::Message() << "y " << c.y);
      expectNear(forceOn(*particle, buoyancy), Vector3(0, c.expectedUp, 0));
    }
  }
}
