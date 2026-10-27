/*
 * Implementation file for random number generation.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cstddef>
#include <cyclone/pworld.h>

using namespace cyclone;

ParticleWorld::ParticleWorld(unsigned maxContacts, unsigned iterations)
    : resolver(iterations), maxContacts(maxContacts) {
  contacts = new ParticleContact[maxContacts];
  calculateIterations = (iterations == 0);
}

ParticleWorld::~ParticleWorld() { delete[] contacts; }

void ParticleWorld::startFrame() {
  for (Particles::iterator p = particles.begin(); p != particles.end(); p++) {
    // Remove all forces from the accumulator
    (*p)->clearAccumulator();
  }
}

unsigned ParticleWorld::generateContacts() {
  unsigned limit = maxContacts;
  ParticleContact *nextContact = contacts;

  for (ContactGenerators::iterator g = contactGenerators.begin(); g != contactGenerators.end();
       g++) {
    unsigned used = (*g)->addContact(nextContact, limit);
    limit -= used;
    nextContact += used;

    // We've run out of contacts to fill. This means we're missing
    // contacts.
    if (limit <= 0)
      break;
  }

  // Return the number of contacts used.
  return maxContacts - limit;
}

void ParticleWorld::integrate(real_duration duration) {
  for (Particles::iterator p = particles.begin(); p != particles.end(); p++) {
    // Remove all forces from the accumulator
    (*p)->integrate(duration);
  }
}

void ParticleWorld::rememberPositions() {
  for (Particle *particle : particles)
    particle->rememberPosition();
}

void ParticleWorld::runPhysics(real_duration duration) {
  rememberPositions();
  // SAKBox: in sub-steps (see setSubSteps), each with the forces added by
  // hand (integrating clears them)
  const real_duration subStep = duration / subSteps;
  if (subSteps > 1) {
    forcesAddedByHand.clear();
    for (Particle *particle : particles)
      forcesAddedByHand.push_back(particle->getForceAccumulator());
  }
  for (unsigned i = 0; i < subSteps; ++i) {
    if (i > 0)
      for (size_t p = 0; p < particles.size(); ++p) {
        particles[p]->clearAccumulator();
        particles[p]->addForce(forcesAddedByHand[p]);
      }

    // First apply the force generators
    registry.updateForces(subStep);

    // Then integrate the objects
    integrate(subStep);

    // Generate contacts
    unsigned usedContacts = generateContacts();
    if (contactsFound)
      contactsFound({contacts, usedContacts}, subStep);

    // And process them
    if (usedContacts) {
      if (calculateIterations)
        resolver.setIterations(usedContacts * 2);
      resolver.resolveContacts(contacts, usedContacts, subStep);
    }
  }
}

ParticleWorld::Particles &ParticleWorld::getParticles() { return particles; }

ParticleWorld::ContactGenerators &ParticleWorld::getContactGenerators() {
  return contactGenerators;
}

ParticleForceRegistry &ParticleWorld::getForceRegistry() { return registry; }

void GroundContacts::init(cyclone::ParticleWorld::Particles *particles) {
  GroundContacts::particles = particles;
}

unsigned GroundContacts::addContact(cyclone::ParticleContact *contact, unsigned limit) const {
  unsigned count = 0;
  for (cyclone::ParticleWorld::Particles::iterator p = particles->begin(); p != particles->end();
       p++) {
    cyclone::real y = (*p)->getPosition().y;
    if (y < 0.0f) {
      contact->contactNormal = cyclone::Vector3::UP;
      contact->particle[0] = *p;
      contact->particle[1] = nullptr;
      contact->penetration = -y;
      contact->restitution = 0.2f;
      contact++;
      count++;
    }

    if (count >= limit)
      return count;
  }
  return count;
}