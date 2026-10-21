/*
 * Interface file for the rigid body world structure.
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
 * This file contains the definitions for a structure to hold any
 * number of rigid bodies, and to manage their simulation.
 */
#ifndef CYCLONE_WORLD_H
#define CYCLONE_WORLD_H

#include "body.h"
#include "contacts.h"
#include "fgen.h"
#include "joints.h"

#include <vector>

namespace cyclone {
/**
 * The world represents an independent simulation of physics.  It
 * keeps track of a set of rigid bodies, and provides the means to
 * update them all.
 */
class World {
  // SAKBox: bodies and contact generators are held in vectors the caller
  // fills, and force generators in a registry, as in ParticleWorld. Upstream
  // kept them in private linked lists with no way to add to them, and left the
  // force registry commented out, so a World could never simulate anything.
public:
  typedef std::vector<RigidBody *> RigidBodies;
  typedef std::vector<ContactGenerator *> ContactGenerators;
  typedef std::vector<Joint *> Joints;

protected:
  /**
   * Holds the rigid bodies being simulated.
   */
  RigidBodies bodies;

  /**
   * True if the world should calculate the number of iterations
   * to give the contact resolver at each frame.
   */
  bool calculateIterations;

  /**
   * Holds the force generators for the bodies in this world.
   */
  ForceRegistry registry;

  /**
   * Holds the resolver for sets of contacts.
   */
  ContactResolver resolver;

  /**
   * Holds the contact generators.
   */
  ContactGenerators contactGenerators;

  /**
   * SAKBox: holds the joints, which the resolver is given with the contacts
   * (see ContactResolver::resolveContacts).
   */
  Joints joints;

  /**
   * Holds an array of contacts, for filling by the contact
   * generators.
   */
  std::vector<Contact> contacts;

public:
  /**
   * Creates a new simulator that can handle up to the given
   * number of contacts per frame. You can also optionally give
   * a number of contact-resolution iterations to use. If you
   * don't give a number of iterations, then four times the
   * number of detected contacts will be used for each frame.
   */
  World(unsigned maxContacts, unsigned iterations = 0);

  /**
   * Calls each of the registered contact generators to report
   * their contacts. Returns the number of generated contacts.
   */
  unsigned generateContacts();

  /**
   * Processes all the physics for the world: applies the force
   * generators, integrates the bodies and resolves their contacts.
   */
  void runPhysics(real_duration duration);

  /**
   * Initialises the world for a simulation frame. This clears
   * the force and torque accumulators for bodies in the
   * world. After calling this, the bodies can have their forces
   * and torques for this frame added.
   */
  void startFrame();

  /**
   * Returns the bodies in the world, to add or remove them.
   */
  RigidBodies &getBodies();

  /**
   * Returns the contact generators, to add or remove them.
   */
  ContactGenerators &getContactGenerators();

  /**
   * SAKBox: returns the joints, to add or remove them. A joint can also be
   * added as a contact generator, as the book does; with contacts resolved
   * together, adding it here holds it between its two anchor points rather
   * than at the contact between them. Don't add a joint both ways.
   */
  Joints &getJoints() { return joints; }

  /**
   * Returns the force registry, to add force generators.
   */
  ForceRegistry &getForceRegistry();

  /**
   * SAKBox: returns the contact resolver, to choose how it works (see
   * ContactResolver::setMethod).
   */
  ContactResolver &getResolver() { return resolver; }

  /**
   * Returns the contacts found in the last call to
   * generateContacts (or runPhysics).
   */
  const Contact *getContacts() const;
};

} // namespace cyclone

#endif // CYCLONE_WORLD_H
