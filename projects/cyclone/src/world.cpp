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

using namespace cyclone;

World::World(unsigned maxContacts, unsigned iterations)
    : calculateIterations(iterations == 0), resolver(iterations), contacts(maxContacts) {}

void World::startFrame() {
  for (RigidBody *body : bodies) {
    // Remove all forces from the accumulator
    body->clearAccumulators();
    body->calculateDerivedData();
  }
}

unsigned World::generateContacts() {
  unsigned limit = static_cast<unsigned>(contacts.size());
  Contact *nextContact = contacts.data();

  for (ContactGenerator *generator : contactGenerators) {
    // We've run out of contacts to fill. This means we're missing
    // contacts.
    if (limit == 0)
      break;

    unsigned used = generator->addContact(nextContact, limit);
    limit -= used;
    nextContact += used;
  }

  // Return the number of contacts used.
  return static_cast<unsigned>(contacts.size()) - limit;
}

void World::runPhysics(real_duration duration) {
  // SAKBox: in sub-steps (see setSubSteps)
  const real_duration step = duration / subSteps;
  if (subSteps > 1) {
    forcesAddedByHand.clear();
    for (RigidBody *body : bodies)
      forcesAddedByHand.emplace_back(body->getForceAccumulator(), body->getTorqueAccumulator());
  }
  for (unsigned subStep = 0; subStep < subSteps; ++subStep) {
    if (subStep > 0) {
      // What startFrame does between steps (integrating cleared the
      // accumulators; resolving moved the bodies), keeping the forces
      // added by hand
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
    runStep(step);
  }
}

void World::runStep(real_duration duration) {
  // First apply the force generators
  registry.updateForces(duration);

  // Then integrate the objects
  for (RigidBody *body : bodies)
    body->integrate(duration);

  // Generate contacts
  unsigned usedContacts = generateContacts();

  // And process them, with the joints (SAKBox)
  if (usedContacts || !joints.empty()) {
    if (calculateIterations)
      resolver.setIterations((usedContacts + (unsigned)joints.size()) * 4);
    resolver.resolveContacts(contacts.data(), usedContacts, duration, joints);
  }
}

World::RigidBodies &World::getBodies() { return bodies; }

World::ContactGenerators &World::getContactGenerators() { return contactGenerators; }

ForceRegistry &World::getForceRegistry() { return registry; }

const Contact *World::getContacts() const { return contacts.data(); }
