// SAKBox: Cyclone under stress. Random scenes of boxes and spheres (light
// and heavy, small and large, slow and very fast, spinning, some joined by
// joints) run for ten seconds with both resolvers, at 60 and 20 steps a
// second; and random scenes of particles joined by rods, cables and springs
// (the particle engine) likewise. After every step, every body must still make sense: finite
// position, velocity, spin and orientation, a unit orientation, no
// unbounded speed or flight. It finds the kind of bug that a scene built by
// hand misses: a sphere whose centre lands inside a box gave a zero contact
// normal and turned both bodies to NaN, which this would have caught.
//
// Each scene comes from a seed; a failure names the seed, resolver, step
// and body, to replay it.
#include <gtest/gtest.h>

#include <cyclone/cyclone.h>
#include <cyclone/plinks.h>
#include <cyclone/pworld.h>
#include <cyclone/world.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace cyclone;

namespace {

constexpr int kScenes = 200;
constexpr double kSeconds = 10;
constexpr double kFastestSane = 1000;  // m/s: shots start at up to 100
constexpr double kFarthestSane = 1e4;  // m
constexpr double kMostMassRatio = 100; // between particles joined by a rod or cable

// A random scene, the same for the same seed: boxes and spheres on the
// ground (y = 0) and against each other, the World's shapes
struct Scene {
  World world{World::Settings{.maxContacts = 1024, .friction = 0.6, .restitution = 0.3}};
  Gravity gravity{Vector3::GRAVITY};
  std::vector<std::unique_ptr<RigidBody>> bodies;
  std::vector<std::unique_ptr<Joint>> joints;
  // (Deques, so adding one moves none)
  std::deque<CollisionBox> boxes;
  std::deque<CollisionSphere> spheres;

  Scene(unsigned seed, ContactResolver::Method method) {
    world.getResolver().setMethod(method);
    Random random(seed);
    // Masses and speeds spread over orders of magnitude: e to the power of
    // a value spread evenly between the logarithms
    const auto logUniform = [&](double low, double high) {
      return std::exp(random.randomReal(std::log(low), std::log(high)));
    };
    const int count = 12 + static_cast<int>(random.randomInt(13));
    for (int i = 0; i < count; ++i) {
      auto body = std::make_unique<RigidBody>();
      const double mass = logUniform(0.05, 500);
      body->setMass(mass);
      Matrix3 inertia;
      if (random.randomInt(10) < 6) {
        const Vector3 half(random.randomReal(0.1, 1.5), random.randomReal(0.1, 1.5),
                           random.randomReal(0.1, 1.5));
        inertia.setBlockInertiaTensor(half, mass);
        CollisionBox &box = boxes.emplace_back();
        box.body = body.get();
        box.halfSize = half;
        world.addShape(&box);
      } else {
        const double radius = random.randomReal(0.1, 1.5);
        const double coeff = 0.4 * mass * radius * radius;
        inertia.setInertiaTensorCoeffs(coeff, coeff, coeff);
        CollisionSphere &sphere = spheres.emplace_back();
        sphere.body = body.get();
        sphere.radius = radius;
        world.addShape(&sphere);
      }
      body->setInertiaTensor(inertia);
      body->setDamping(0.99, 0.8);
      body->setPosition(random.randomVector(Vector3(-6, 0.5, -6), Vector3(6, 12, 6)));
      body->setOrientation(random.randomQuaternion());
      Vector3 direction = random.randomVector(1);
      direction.normalise();
      body->setVelocity(direction * logUniform(0.1, 100));
      body->setRotation(random.randomVector(20));
      body->setAcceleration(Vector3());
      body->setCanSleep(random.randomInt(2) == 0);
      body->setAwake();
      body->clearAccumulators();
      body->calculateDerivedData();
      world.getBodies().push_back(body.get());
      world.getForceRegistry().add(body.get(), &gravity);
      bodies.push_back(std::move(body));
    }
    // Half the scenes join a few pairs of bodies: tight (a ball joint) or
    // with slack (a cable)
    if (random.randomInt(2) == 0) {
      const unsigned pairs = 1 + random.randomInt(5);
      for (unsigned i = 0; i < pairs; ++i) {
        RigidBody *a = bodies[random.randomInt(bodies.size())].get();
        RigidBody *b = bodies[random.randomInt(bodies.size())].get();
        if (a == b)
          continue;
        auto joint = std::make_unique<Joint>();
        joint->set(a, random.randomVector(0.3), b, random.randomVector(0.3),
                   random.randomInt(2) == 0 ? (real)0 : random.randomReal(0.1, 1.0));
        joint->collideConnected = true; // the harder case: the bodies still collide
        world.getJoints().push_back(joint.get());
        joints.push_back(std::move(joint));
      }
    }
    CollisionPlane ground;
    ground.direction = Vector3(0, 1, 0);
    ground.offset = 0;
    world.addPlane(ground);
  }
};

bool finite(const Vector3 &v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// What's wrong with `body`, or an empty string if nothing
std::string problemWith(const RigidBody &body) {
  const Quaternion q = body.getOrientation();
  const double length = std::sqrt(q.r * q.r + q.i * q.i + q.j * q.j + q.k * q.k);
  if (!finite(body.getPosition()))
    return "position isn't finite";
  if (!finite(body.getVelocity()))
    return "velocity isn't finite";
  if (!finite(body.getRotation()))
    return "spin isn't finite";
  if (!std::isfinite(length))
    return "orientation isn't finite";
  if (std::abs(length - 1) > 1e-3)
    return "orientation isn't unit length (" + std::to_string(length) + ")";
  if (body.getVelocity().magnitude() > kFastestSane)
    return "speed " + std::to_string(body.getVelocity().magnitude()) + " m/s";
  if (body.getPosition().magnitude() > kFarthestSane)
    return "flew " + std::to_string(body.getPosition().magnitude()) + " m away";
  return "";
}

// Runs every scene with `method` at `steps` a second, each in `subSteps`
// (see World::setSubSteps); returns the first problem, or an empty string
std::string runAll(ContactResolver::Method method, int steps, unsigned subSteps = 1) {
  for (unsigned seed = 1; seed <= kScenes; ++seed) {
    Scene scene(seed, method);
    scene.world.setSubSteps(subSteps);
    for (int step = 0; step < kSeconds * steps; ++step) {
      scene.world.startFrame();
      scene.world.runPhysics(1.0 / steps);
      for (size_t i = 0; i < scene.bodies.size(); ++i) {
        const std::string problem = problemWith(*scene.bodies[i]);
        if (!problem.empty()) {
          std::ostringstream report;
          report << "seed " << seed << ", " << steps << " steps a second in " << subSteps
                 << " sub-steps, step " << step << " (" << static_cast<double>(step) / steps
                 << " s), body " << i << " of " << scene.bodies.size() << ": " << problem;
          return report.str();
        }
      }
    }
  }
  return "";
}

} // namespace

// ---------------------------------------------------------------------------
// The particle engine

namespace {

// A random scene of particles: some joined by rods and cables, some held to
// fixed points, some by springs and bungees, all falling onto the ground
struct ParticleScene {
  ParticleWorld world{1024};
  ParticleGravity gravity{Vector3::GRAVITY};
  std::vector<std::unique_ptr<Particle>> particles;
  std::vector<std::unique_ptr<ParticleContactGenerator>> links;
  std::vector<std::unique_ptr<ParticleForceGenerator>> springs;
  GroundContacts ground;

  // `slowestSteps`: the fewest steps a second it will run at, which keeps
  // springs soft enough to be stable (see below)
  ParticleScene(unsigned seed, int slowestSteps) {
    Random random(seed);
    const auto logUniform = [&](double low, double high) {
      return std::exp(random.randomReal(std::log(low), std::log(high)));
    };
    const int count = 8 + static_cast<int>(random.randomInt(23));
    for (int i = 0; i < count; ++i) {
      auto particle = std::make_unique<Particle>();
      particle->setMass(logUniform(0.05, 100));
      particle->setDamping(0.99);
      particle->setPosition(random.randomVector(Vector3(-6, 0.5, -6), Vector3(6, 12, 6)));
      Vector3 direction = random.randomVector(1);
      direction.normalise();
      particle->setVelocity(direction * logUniform(0.1, 100));
      particle->setAcceleration(Vector3());
      particle->clearAccumulator();
      world.getParticles().push_back(particle.get());
      world.getForceRegistry().add(particle.get(), &gravity);
      particles.push_back(std::move(particle));
    }
    const auto any = [&]() { return particles[random.randomInt(particles.size())].get(); };
    const auto distance = [](const Particle *a, const Particle *b) {
      return (a->getPosition() - b->getPosition()).magnitude();
    };
    const unsigned joins = random.randomInt(2 * count);
    for (unsigned i = 0; i < joins; ++i) {
      Particle *a = any(), *b = any();
      // Rods and cables only between particles within 100 times each
      // other's mass: a heavy particle hanging from a much lighter one is
      // more than an iterative resolver can hold (see Known limitations in
      // the README)
      const double ratio = a->getMass() / b->getMass();
      if (a == b || ratio > kMostMassRatio || ratio < 1 / kMostMassRatio)
        continue;
      switch (random.randomInt(5)) {
      case 0: { // a rod, as long as they're apart now
        auto rod = std::make_unique<ParticleRod>();
        rod->particle[0] = a;
        rod->particle[1] = b;
        rod->length = distance(a, b);
        world.getContactGenerators().push_back(rod.get());
        links.push_back(std::move(rod));
        break;
      }
      case 1: { // a cable with some slack
        auto cable = std::make_unique<ParticleCable>();
        cable->particle[0] = a;
        cable->particle[1] = b;
        cable->maxLength = distance(a, b) * random.randomReal(1, 1.5);
        cable->restitution = random.randomReal(0, 0.5);
        world.getContactGenerators().push_back(cable.get());
        links.push_back(std::move(cable));
        break;
      }
      case 2: { // held to a fixed point by a rod or a cable
        const Vector3 anchor = a->getPosition() + random.randomVector(3);
        const double length = (a->getPosition() - anchor).magnitude();
        if (random.randomInt(2) == 0) {
          auto rod = std::make_unique<ParticleRodConstraint>();
          rod->particle = a;
          rod->anchor = anchor;
          rod->length = length;
          world.getContactGenerators().push_back(rod.get());
          links.push_back(std::move(rod));
        } else {
          auto cable = std::make_unique<ParticleCableConstraint>();
          cable->particle = a;
          cable->anchor = anchor;
          cable->maxLength = length * random.randomReal(1, 1.5);
          cable->restitution = random.randomReal(0, 0.5);
          world.getContactGenerators().push_back(cable.get());
          links.push_back(std::move(cable));
        }
        break;
      }
      default: { // a spring or a bungee between `a` and `b`
        // Cyclone's springs pull only the particle they're registered on,
        // so a real spring is two, one each way, alike: one alone isn't
        // physical (nothing pulls back on the other end), and can pump
        // energy into a scene. Springs are integrated step by step, so one
        // too stiff for the step gains energy and flies apart (a limit of
        // the method, not a bug): keep stiffness / mass x step^2 under 0.15
        // for the lighter end
        const double step = 1.0 / slowestSteps;
        const double stiffest = 0.15 * std::min(a->getMass(), b->getMass()) / (step * step);
        const double stiffness = random.randomReal(0.01, 1) * stiffest;
        const double rest = distance(a, b) * random.randomReal(0.5, 1.5);
        const bool bungee = random.randomInt(2) == 0;
        for (auto [from, to] : {std::pair{a, b}, std::pair{b, a}}) {
          std::unique_ptr<ParticleForceGenerator> spring;
          if (bungee)
            spring = std::make_unique<ParticleBungee>(to, stiffness, rest);
          else
            spring = std::make_unique<ParticleSpring>(to, stiffness, rest);
          world.getForceRegistry().add(from, spring.get());
          springs.push_back(std::move(spring));
        }
        break;
      }
      }
    }
    ground.init(&world.getParticles());
    world.getContactGenerators().push_back(&ground);
  }
};

// What's wrong with `particle`, or an empty string if nothing
std::string problemWith(const Particle &particle) {
  if (!finite(particle.getPosition()))
    return "position isn't finite";
  if (!finite(particle.getVelocity()))
    return "velocity isn't finite";
  if (particle.getVelocity().magnitude() > kFastestSane)
    return "speed " + std::to_string(particle.getVelocity().magnitude()) + " m/s";
  if (particle.getPosition().magnitude() > kFarthestSane)
    return "flew " + std::to_string(particle.getPosition().magnitude()) + " m away";
  return "";
}

// Runs every particle scene at `steps` a second; returns the first problem
std::string runAllParticles(int steps) {
  for (unsigned seed = 1; seed <= kScenes; ++seed) {
    ParticleScene scene(seed, 20);
    for (int step = 0; step < kSeconds * steps; ++step) {
      scene.world.startFrame();
      scene.world.runPhysics(1.0 / steps);
      for (size_t i = 0; i < scene.particles.size(); ++i) {
        const std::string problem = problemWith(*scene.particles[i]);
        if (!problem.empty()) {
          std::ostringstream report;
          report << "seed " << seed << ", " << steps << " steps a second, step " << step << " ("
                 << static_cast<double>(step) / steps << " s), particle " << i << " of "
                 << scene.particles.size() << ": " << problem;
          return report.str();
        }
      }
    }
  }
  return "";
}

} // namespace

TEST(StressTest, ParticlesAt60StepsASecond) { EXPECT_EQ(runAllParticles(60), ""); }

TEST(StressTest, ParticlesAt20StepsASecond) { EXPECT_EQ(runAllParticles(20), ""); }

// ---------------------------------------------------------------------------
// The rigid bodies

TEST(StressTest, ResolvingTogetherAt60StepsASecond) {
  EXPECT_EQ(runAll(ContactResolver::Method::sequentialImpulses, 60), "");
}

TEST(StressTest, ResolvingTogetherAt20StepsASecond) {
  EXPECT_EQ(runAll(ContactResolver::Method::sequentialImpulses, 20), "");
}

TEST(StressTest, TheBooksResolverAt60StepsASecond) {
  EXPECT_EQ(runAll(ContactResolver::Method::oneContactAtATime, 60), "");
}

TEST(StressTest, TheBooksResolverAt20StepsASecond) {
  EXPECT_EQ(runAll(ContactResolver::Method::oneContactAtATime, 20), "");
}

TEST(StressTest, ResolvingTogetherIn4SubSteps) {
  EXPECT_EQ(runAll(ContactResolver::Method::sequentialImpulses, 20, 4), "");
}

TEST(StressTest, TheBooksResolverIn4SubSteps) {
  EXPECT_EQ(runAll(ContactResolver::Method::oneContactAtATime, 20, 4), "");
}
