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
  // First apply the force generators
  registry.updateForces(duration);

  // Then integrate the objects
  for (RigidBody *body : bodies)
    body->integrate(duration);

  // Generate contacts
  unsigned usedContacts = generateContacts();

  // And process them
  if (usedContacts) {
    if (calculateIterations)
      resolver.setIterations(usedContacts * 4);
    resolver.resolveContacts(contacts.data(), usedContacts, duration);
  }
}

World::RigidBodies &World::getBodies() { return bodies; }

World::ContactGenerators &World::getContactGenerators() { return contactGenerators; }

ForceRegistry &World::getForceRegistry() { return registry; }

const Contact *World::getContacts() const { return contacts.data(); }
