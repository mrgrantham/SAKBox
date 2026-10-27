/*
 * Interface file for the particle / mass aggregate world structure.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

/**
 * @file
 *
 * This file contains the definitions for a structure to hold any number o
 * particle masses, and their connections.
 */
#ifndef CYCLONE_PWORLD_H
#define CYCLONE_PWORLD_H

#include "pfgen.h"
#include "plinks.h"

#include <functional>
#include <span>
#include <vector>

namespace cyclone {

/**
 * Keeps track of a set of particles, and provides the means to
 * update them all.
 */
class ParticleWorld {
public:
  typedef std::vector<Particle *> Particles;
  typedef std::vector<ParticleContactGenerator *> ContactGenerators;

protected:
  /**
   * Holds the particles
   */
  Particles particles;

  /**
   * True if the world should calculate the number of iterations
   * to give the contact resolver at each frame.
   */
  bool calculateIterations;

  /**
   * Holds the force generators for the particles in this world.
   */
  ParticleForceRegistry registry;

  /**
   * Holds the resolver for contacts.
   */
  ParticleContactResolver resolver;

  /**
   * Contact generators.
   */
  ContactGenerators contactGenerators;

  /**
   * Holds the list of contacts.
   */
  ParticleContact *contacts;

  /**
   * Holds the maximum number of contacts allowed (i.e. the
   * size of the contacts array).
   */
  unsigned maxContacts;

  /** SAKBox: see setSubSteps. */
  unsigned subSteps = 1;

  /** SAKBox: each particle's forces added by hand, kept for every sub-step. */
  std::vector<Vector3> forcesAddedByHand;

public:
  /**
   * Creates a new particle simulator that can handle up to the
   * given number of contacts per frame. You can also optionally
   * give a number of contact-resolution iterations to use. If you
   * don't give a number of iterations, then twice the number of
   * contacts will be used.
   */
  ParticleWorld(unsigned maxContacts, unsigned iterations = 0);

  /**
   * Deletes the simulator.
   */
  ~ParticleWorld();

  /**
   * Calls each of the registered contact generators to report
   * their contacts. Returns the number of generated contacts.
   */
  unsigned generateContacts();

  /**
   * Integrates all the particles in this world forward in time
   * by the given duration.
   */
  void integrate(real_duration duration);

  /**
   * Processes all the physics for the particle world.
   *
   * SAKBox: one step of `duration`, in sub-steps (setSubSteps), each
   * applying the force generators, integrating, and finding and resolving
   * the contacts afresh: the book's step, run that many times shorter.
   * (The rigid-body World finds its contacts once a step and follows them;
   * particle contacts are cheap to find, and their generators are the
   * demos' own code, so here each sub-step simply finds them again.) First
   * it remembers each particle's position (Particle::rememberPosition), to
   * draw it between this step and the next.
   */
  void runPhysics(real_duration duration);

  /**
   * SAKBox: runs each step in this many sub-steps of duration / count (1
   * or more; 1 by default). Forces added to particles by hand before
   * runPhysics act through every sub-step.
   */
  void setSubSteps(unsigned count) { subSteps = count > 0 ? count : 1; }
  unsigned getSubSteps() const { return subSteps; }

  /**
   * SAKBox: remembers every particle's position as it is now (see
   * Particle::rememberPosition), after placing them by hand.
   */
  void rememberPositions();

  /**
   * Initializes the world for a simulation frame. This clears
   * the force accumulators for particles in the world. After
   * calling this, the particles can have their forces for this
   * frame added.
   */
  void startFrame();

  /**
   *  Returns the list of particles.
   */
  Particles &getParticles();

  /**
   * Returns the list of contact generators.
   */
  ContactGenerators &getContactGenerators();

  /**
   * Returns the force registry.
   */
  ParticleForceRegistry &getForceRegistry();

  /**
   * SAKBox: called each sub-step with the contacts found, before they're
   * resolved, as World's contactsFound is: for code that listens for hits,
   * such as sounds.
   */
  std::function<void(std::span<const ParticleContact>, real_duration)> contactsFound;
};

/**
 * A contact generator that takes an STL vector of particle pointers and
 * collides them against the ground.
 */
class GroundContacts : public cyclone::ParticleContactGenerator {
  cyclone::ParticleWorld::Particles *particles;

public:
  void init(cyclone::ParticleWorld::Particles *particles);

  unsigned addContact(cyclone::ParticleContact *contact, unsigned limit) const override;
};

} // namespace cyclone

#endif // CYCLONE_PWORLD_H
