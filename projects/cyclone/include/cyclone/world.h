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
#include "collide_fine.h"
#include "contacts.h"
#include "fgen.h"
#include "joints.h"

#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace cyclone {
/**
 * The world represents an independent simulation of physics.  It
 * keeps track of a set of rigid bodies, and provides the means to
 * update them all.
 *
 * SAKBox: a World runs the whole simulation: add bodies, their collision
 * shapes (spheres, boxes, capsules) and the planes they rest on, joints and
 * force generators, then call startFrame() and runPhysics() each step. It
 * finds the contacts between the shapes itself (any pair whose bounding
 * spheres meet, then the test for that pair of shapes; shapes on the same
 * body, pairs a joint connects and pairs their filters keep apart don't
 * collide), and resolves them with the joints. Contact generators (the
 * book's interface) add contacts of their own.
 *
 * Its bodies, shapes and joints are the caller's: it holds pointers to
 * them, and they must outlive it or be removed.
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

  /** SAKBox: how a World simulates (see the members). */
  struct Settings {
    /** The most contacts a step can have. */
    unsigned maxContacts = 256;

    /**
     * Each step is taken in this many sub-steps (see runPhysics). The
     * contacts are found once a step and followed through its sub-steps.
     */
    unsigned subSteps = 1;

    /** The friction and restitution (bounciness) of every contact. */
    real friction = (real)0.9;
    real restitution = 0;

    /**
     * How close shapes must be to make a contact, resolving contacts
     * together in sub-steps: the book's pessimistic collision detection
     * (section 16.1.4, see CollisionData::margin), so that contacts found in
     * a step's first sub-step are there when its later ones bring shapes
     * together. 2 cm, as
     * Box2D's speculative distance. A contact is followed through the step
     * with its normal and anchors as found, which grows less true the
     * farther apart the shapes were and the more they turn, so it's kept
     * small: a margin as wide as bodies can move in a step let a light box
     * spinning between heavy ones be pushed by contacts that no longer
     * described where they touched, until it flew off. So a body faster
     * than this margin a step can sink up to a step's travel into another
     * before their contact is found, where finding contacts in every
     * sub-step limited it to a sub-step's. Against a plane, a contact is
     * followed exactly, so there the margin also covers how far the shape
     * can move in the step, and a falling body's contact with the ground is
     * there before it lands.
     *
     * It costs time too: contacts within the margin are swept like any
     * other, about a fifth more of them for the ragdoll, which costs about
     * what finding contacts once a step saves (see the README's Sub-steps).
     */
    real margin = (real)0.02;

    /**
     * How close box-on-box contacts are kept from step to step
     * (BoxContactCache; CollisionData::tolerance), as the demos kept them.
     */
    real boxContactTolerance = (real)0.1;

    /**
     * How many contacts the book's resolver resolves a sub-step (see
     * ContactResolver::setIterations); 0 for four for each contact and joint
     * found.
     */
    unsigned iterations = 0;
  };

  /**
   * SAKBox: where the time went in the last step (ms): finding contacts,
   * applying forces and integrating the bodies, and resolving the contacts
   * and joints (all sub-steps together)
   */
  struct Profile {
    double findContacts = 0, integrate = 0, resolve = 0;
  };

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
   * SAKBox: holds the joints, which the resolver takes alongside the
   * contacts (see ContactResolver::resolveContacts).
   */
  Joints joints;

  /**
   * Holds an array of contacts, for filling by the contact
   * generators.
   */
  std::vector<Contact> contacts;

  /** SAKBox: how many of them the last step found. */
  unsigned contactCount = 0;

  /** SAKBox: the shapes and planes to find contacts between. */
  std::vector<CollisionPrimitive *> shapes;
  std::vector<CollisionPlane> planes;

  /** SAKBox: keeps box-on-box contacts from step to step. */
  BoxContactCache boxContacts;

  Settings settings;
  Profile lastProfile;

  /**
   * SAKBox: the force and torque added to each body by hand before
   * runPhysics, to apply again in each sub-step.
   */
  std::vector<std::pair<Vector3, Vector3>> forcesAddedByHand;

  /**
   * SAKBox: finds the contacts between the shapes, and the generators'.
   * Given a duration, they're to be followed through that much of the step
   * (its sub-steps after the first), so they're found with the margin (see
   * Settings::margin).
   */
  unsigned findContacts(real_duration stepDuration);

  /**
   * SAKBox: applies the forces and integrates the bodies for one sub-step
   * (restoring the forces added by hand after the first).
   */
  void integrate(real_duration duration, bool firstSubStep);

  /**
   * SAKBox: updates every body's sleep measure (RigidBody::updateSleep) over
   * `duration`: once a step resolving together, from the velocities the
   * step ends with; after each sub-step's integration with the book's
   * resolver, whose sub-steps are whole steps to it.
   */
  void updateSleep(real_duration duration);

public:
  /** SAKBox: a world that simulates with the default Settings, or as `settings` says. */
  World();
  explicit World(const Settings &settings);

  /**
   * Creates a new simulator that can handle up to the given
   * number of contacts per frame. You can also optionally give
   * a number of contact-resolution iterations to use. If you
   * don't give a number of iterations, then twice the number of
   * contacts will be used.
   */
  World(unsigned maxContacts, unsigned iterations = 0);

  /**
   * Calls each of the registered contact generators to report
   * their contacts. Returns the number of generated contacts.
   * (SAKBox: and the contacts between the shapes, with no margin.)
   */
  unsigned generateContacts();

  /**
   * Processes all the physics for the world.
   *
   * SAKBox: one step of `duration`, in sub-steps (setSubSteps). Each
   * sub-step applies the forces, integrates the bodies and resolves the
   * contacts and joints. Resolving contacts together (the resolver's
   * default), the contacts are found once, after the first sub-step's
   * integration, and each later sub-step brings them up to where their
   * bodies are now (Contact::followAnchors) rather than finding them
   * afresh. With the book's resolver, each sub-step finds them afresh.
   * First it remembers each body's pose (RigidBody::rememberPose), to draw
   * it between this step and the next. With every body asleep, that's all
   * it does.
   */
  void runPhysics(real_duration duration);

  /**
   * SAKBox: remembers every body's pose as it is now (see
   * RigidBody::rememberPose): call it after placing the bodies by hand, as
   * a reset does, so they aren't drawn sliding from where they were.
   */
  void rememberPoses();

  /**
   * SAKBox: runs the physics in this many steps of duration / count each
   * (1 or more; 1 by default): each sub-step applies the force
   * generators, integrates and resolves. Forces added by hand before
   * runPhysics act through every sub-step.
   */
  void setSubSteps(unsigned count) { settings.subSteps = count > 0 ? count : 1; }
  unsigned getSubSteps() const { return settings.subSteps; }

  /**
   * Initialises the world for a simulation frame. This clears
   * the force and torque accumulators for bodies in the
   * world. After calling this, the bodies can have their forces
   * and torques for this frame added.
   */
  void startFrame();

  /** SAKBox: adds a body to simulate (the caller keeps it alive). */
  void addBody(RigidBody *body) { bodies.push_back(body); }

  /**
   * SAKBox: adds a collision shape on one of the bodies (the caller keeps
   * it alive). A body can have several, which don't collide with each other.
   */
  void addShape(CollisionPrimitive *shape) { shapes.push_back(shape); }

  /** SAKBox: adds an immovable plane (such as the ground) for shapes to rest on. */
  void addPlane(const CollisionPlane &plane) { planes.push_back(plane); }

  /** SAKBox: adds a joint between two bodies (the caller keeps it alive). */
  void addJoint(Joint *joint) { joints.push_back(joint); }

  /** SAKBox: removes a shape, a body (and its shapes), or a joint. */
  void removeShape(const CollisionPrimitive *shape);
  void removeBody(const RigidBody *body);
  void removeJoint(const Joint *joint);

  /**
   * SAKBox: called at the start of every sub-step, before the forces are
   * applied, with its duration: for code that acts as the simulation goes,
   * such as a controller setting motors.
   */
  std::function<void(real_duration)> beforeSubStep;

  /**
   * SAKBox: called each time contacts are found, before they're resolved,
   * with them and the time they're resolved over (the step, or with the
   * book's resolver, which finds them in each sub-step, the sub-step): for
   * code that listens for hits, such as sounds.
   */
  std::function<void(std::span<const Contact>, real_duration)> contactsFound;

  RigidBodies &getBodies();
  ContactGenerators &getContactGenerators();
  Joints &getJoints() { return joints; }
  std::span<CollisionPrimitive *const> getShapes() const { return shapes; }
  ForceRegistry &getForceRegistry();
  ContactResolver &getResolver() { return resolver; }
  Settings &getSettings() { return settings; }

  /**
   * SAKBox: the contacts of the last step, as found in its first sub-step and
   * followed through its sub-steps (penetration negative where shapes are
   * still apart, within the margin).
   */
  std::span<const Contact> getStepContacts() const { return {contacts.data(), contactCount}; }

  /** SAKBox: where the time went in the last step. */
  const Profile &profile() const { return lastProfile; }

  const Contact *getContacts() const;
};

} // namespace cyclone

#endif // CYCLONE_WORLD_H
