/*
 * Implementation file for the rigid body world.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cyclone/world.h>

#include <algorithm>
#include <chrono>

using namespace cyclone;

namespace {
using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Whether a joint between `one` and `two` keeps their shapes apart
bool joinedApart(std::span<Joint *const> joints, const RigidBody *one, const RigidBody *two) {
  return std::ranges::any_of(joints, [&](const Joint *joint) {
    return !joint->collideConnected && ((joint->body[0] == one && joint->body[1] == two) ||
                                        (joint->body[0] == two && joint->body[1] == one));
  });
}

// The contacts between two shapes, by the test for their kinds of shape
unsigned collide(const CollisionPrimitive &one, const CollisionPrimitive &two,
                 BoxContactCache &boxContacts, CollisionData &data) {
  using Shape = CollisionPrimitive::Shape;
  // In the order sphere, box, capsule, so each pair has one case
  if (two.getShape() < one.getShape())
    return collide(two, one, boxContacts, data);
  if (one.getShape() == Shape::sphere) {
    const auto &sphere = static_cast<const CollisionSphere &>(one);
    switch (two.getShape()) {
    case Shape::sphere:
      return CollisionDetector::sphereAndSphere(sphere, static_cast<const CollisionSphere &>(two),
                                                &data);
    case Shape::box:
      return CollisionDetector::boxAndSphere(static_cast<const CollisionBox &>(two), sphere, &data);
    case Shape::capsule:
      return CollisionDetector::capsuleAndSphere(static_cast<const CollisionCapsule &>(two), sphere,
                                                 &data);
    }
  }
  if (one.getShape() == Shape::box) {
    const auto &box = static_cast<const CollisionBox &>(one);
    if (two.getShape() == Shape::box)
      return boxContacts.boxAndBox(box, static_cast<const CollisionBox &>(two), &data);
    return CollisionDetector::capsuleAndBox(static_cast<const CollisionCapsule &>(two), box, &data);
  }
  return CollisionDetector::capsuleAndCapsule(static_cast<const CollisionCapsule &>(one),
                                              static_cast<const CollisionCapsule &>(two), &data);
}

// The contacts between a shape and a plane
unsigned collide(const CollisionPrimitive &shape, const CollisionPlane &plane,
                 CollisionData &data) {
  using Shape = CollisionPrimitive::Shape;
  switch (shape.getShape()) {
  case Shape::sphere:
    return CollisionDetector::sphereAndHalfSpace(static_cast<const CollisionSphere &>(shape), plane,
                                                 &data);
  case Shape::box:
    return CollisionDetector::boxAndHalfSpace(static_cast<const CollisionBox &>(shape), plane,
                                              &data);
  case Shape::capsule:
    return CollisionDetector::capsuleAndHalfSpace(static_cast<const CollisionCapsule &>(shape),
                                                  plane, &data);
  }
  return 0;
}
} // namespace

World::World() : World(Settings{}) {}

World::World(const Settings &settings)
    : calculateIterations(settings.iterations == 0), resolver(settings.iterations),
      contacts(settings.maxContacts), settings(settings) {}

World::World(unsigned maxContacts, unsigned iterations)
    : calculateIterations(iterations == 0), resolver(iterations), contacts(maxContacts) {
  settings.maxContacts = maxContacts;
}

void World::startFrame() {
  for (RigidBody *body : bodies) {
    // Remove all forces from the accumulator
    body->clearAccumulators();
    body->calculateDerivedData();
  }
}

unsigned World::generateContacts() { return findContacts(0); }

unsigned World::findContacts(real_duration stepDuration) {
  const bool aheadOfTheStep = stepDuration > 0;
  // SAKBox: the shapes where their bodies are now
  for (CollisionPrimitive *shape : shapes)
    shape->calculateInternals();

  CollisionData data;
  data.contactArray = contacts.data();
  data.reset(static_cast<unsigned>(contacts.size()));
  data.friction = settings.friction;
  data.restitution = settings.restitution;
  data.tolerance = settings.boxContactTolerance;

  // Resolving contacts together in sub-steps, contacts found in the first
  // are followed through the rest (`stepDuration` of them), so shapes a
  // little apart make contacts too (see Settings::margin). Against a plane,
  // also as far as a shape can move in them: following a contact with a
  // plane is exact (its penetration is how far its anchor is past the
  // plane, however far the shape has moved), so a shape falling fast has
  // its contact before it lands.
  const bool followed =
      aheadOfTheStep && resolver.getMethod() == ContactResolver::Method::sequentialImpulses;
  const real margin = followed ? settings.margin : (real)0;
  auto travel = [&](const CollisionPrimitive &shape) {
    if (!followed)
      return (real)0;
    return shape.body->getVelocity().magnitude() * (real)stepDuration;
  };

  for (const CollisionPrimitive *shape : shapes) {
    for (const CollisionPlane &plane : planes) {
      if (!data.hasMoreContacts())
        break;
      if (!shape->filter.collidesWith(plane.filter))
        continue;
      data.margin = margin + travel(*shape);
      collide(*shape, plane, data);
    }
  }

  for (size_t i = 0; i < shapes.size(); i++) {
    for (size_t j = i + 1; j < shapes.size() && data.hasMoreContacts(); j++) {
      const CollisionPrimitive &one = *shapes[i], &two = *shapes[j];
      if (one.body == two.body || !one.filter.collidesWith(two.filter) ||
          joinedApart(joints, one.body, two.body))
        continue;
      data.margin = margin;
      // Only shapes whose bounding spheres come within the margin
      const real reach = one.boundingRadius() + two.boundingRadius() + margin;
      if ((one.getAxis(3) - two.getAxis(3)).squareMagnitude() > reach * reach)
        continue;
      collide(one, two, boxContacts, data);
    }
  }

  // The contact generators' (the book's interface)
  unsigned used = data.contactCount;
  for (ContactGenerator *generator : contactGenerators) {
    const unsigned limit = static_cast<unsigned>(contacts.size()) - used;
    // We've run out of contacts to fill. This means we're missing
    // contacts.
    if (limit == 0)
      break;
    used += generator->addContact(contacts.data() + used, limit);
  }
  return used;
}

void World::integrate(real_duration duration, bool firstSubStep) {
  if (!firstSubStep) {
    // What startFrame does between steps (integrating cleared the
    // accumulators; resolving moved the bodies), keeping the forces added by
    // hand
    for (size_t i = 0; i < bodies.size(); ++i) {
      RigidBody *body = bodies[i];
      body->calculateDerivedData();
      // (Added again only if there are any: adding wakes a body)
      const auto &[force, torque] = forcesAddedByHand[i];
      body->clearAccumulators();
      if (force != Vector3())
        body->addForce(force);
      if (torque != Vector3())
        body->addTorque(torque);
    }
  }
  if (beforeSubStep)
    beforeSubStep(duration);

  // First apply the force generators
  registry.updateForces(duration);

  // Then integrate the objects (SAKBox: their sleep is updated once a
  // step, in runPhysics)
  for (RigidBody *body : bodies)
    body->integrateWithoutSleep(duration);
  for (CollisionPrimitive *shape : shapes)
    shape->calculateInternals();
}

void World::updateSleep(real_duration duration) {
  for (RigidBody *body : bodies)
    body->updateSleep(duration);
}

void World::rememberPoses() {
  for (RigidBody *body : bodies)
    body->rememberPose();
}

void World::runPhysics(real_duration duration) {
  // SAKBox: in sub-steps (see setSubSteps)
  lastProfile = {};
  rememberPoses();
  // With every body asleep nothing moves, so there's nothing to find or
  // resolve until something wakes one (a push, a hand-added force, or a
  // body placed by hand)
  if (std::ranges::none_of(bodies, [](const RigidBody *body) { return body->getAwake(); })) {
    contactCount = 0;
    return;
  }
  const unsigned subSteps = settings.subSteps;
  const real_duration subStep = duration / subSteps;
  if (subSteps > 1) {
    forcesAddedByHand.clear();
    for (RigidBody *body : bodies)
      forcesAddedByHand.emplace_back(body->getForceAccumulator(), body->getTorqueAccumulator());
  }
  auto timed = [](double &total, auto &&work) {
    const Clock::time_point start = Clock::now();
    work();
    total += millisecondsSince(start);
  };

  if (resolver.getMethod() == ContactResolver::Method::sequentialImpulses) {
    // The contacts are found once, after the first sub-step's integration,
    // where the book's loop finds them, and followed through the rest (the
    // book's way of updating contacts as bodies move, section 14.4, as
    // Box2D's sub-steps do), with their set-up kept for the step. (Found
    // before integrating instead and followed through the first sub-step
    // too, a new stack of ten boxes leant 3 cm in its first seconds, where
    // finding them after leant 1.8 cm.) With one sub-step, nothing is
    // followed. (It saves less than it seems: the sweeps in each sub-step
    // are most of the work, and the margin adds contacts to them.)
    for (unsigned i = 0; i < subSteps; ++i) {
      timed(lastProfile.integrate, [&] { integrate(subStep, i == 0); });
      if (i == 0) {
        timed(lastProfile.findContacts, [&] {
          contactCount = findContacts(subSteps > 1 ? duration - subStep : (real_duration)0);
        });
        if (contactsFound)
          contactsFound(getStepContacts(), duration);
        if (calculateIterations)
          resolver.setIterations((contactCount + (unsigned)joints.size()) * 4 + 1);
        timed(lastProfile.resolve, [&] {
          resolver.beginStep(contacts.data(), contactCount, (real)subStep, subSteps, joints);
        });
      }
      timed(lastProfile.resolve, [&] { resolver.resolveSubStep((real)subStep, i > 0); });
    }
    updateSleep(duration);
    timed(lastProfile.resolve, [&] { resolver.endStep(); });
    return;
  }

  // The book's resolver: each sub-step finds the contacts afresh
  for (unsigned i = 0; i < subSteps; ++i) {
    timed(lastProfile.integrate, [&] {
      integrate(subStep, i == 0);
      // As the book's step does: each sub-step is a whole step to it
      updateSleep(subStep);
    });
    timed(lastProfile.findContacts, [&] { contactCount = findContacts(0); });
    if (contactsFound)
      contactsFound(getStepContacts(), subStep);
    if (contactCount || !joints.empty()) {
      if (calculateIterations)
        resolver.setIterations((contactCount + (unsigned)joints.size()) * 4);
      timed(lastProfile.resolve, [&] {
        resolver.resolveContacts(contacts.data(), contactCount, (real)subStep, joints);
      });
    }
  }
}

void World::removeShape(const CollisionPrimitive *shape) {
  std::erase(shapes, shape);
  if (shape->getShape() == CollisionPrimitive::Shape::box)
    boxContacts.forget(*static_cast<const CollisionBox *>(shape));
}

void World::removeBody(const RigidBody *body) {
  std::erase(bodies, body);
  std::vector<const CollisionPrimitive *> onBody;
  for (const CollisionPrimitive *shape : shapes)
    if (shape->body == body)
      onBody.push_back(shape);
  for (const CollisionPrimitive *shape : onBody)
    removeShape(shape);
  registry.remove(body);
}

void World::removeJoint(const Joint *joint) { std::erase(joints, joint); }

World::RigidBodies &World::getBodies() { return bodies; }

World::ContactGenerators &World::getContactGenerators() { return contactGenerators; }

ForceRegistry &World::getForceRegistry() { return registry; }

const Contact *World::getContacts() const { return contacts.data(); }
