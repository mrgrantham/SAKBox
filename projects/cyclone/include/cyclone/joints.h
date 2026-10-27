/*
 * Interface file for joints between rigid bodies.
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
 * This file contains the definitions for joints that link together
 * different rigid bodies.
 */
#ifndef CYCLONE_JOINTS_H
#define CYCLONE_JOINTS_H

#include "contacts.h"

namespace cyclone {

/**
 * Joints link together two rigid bodies and make sure they do not
 * separate.  In a general phyiscs engine there may be many
 * different types of joint, that reduce the number of relative
 * degrees of freedom between two objects. This joint is a common
 * position joint: each object has a location (given in
 * body-coordinates) that will be kept at the same point in the
 * simulation.
 *
 * SAKBox: when contacts are resolved together, an `error` of zero holds
 * the two points together exactly (a ball-and-socket joint), and above
 * zero lets them move up to `error` apart, as if joined by a cable.
 */
class Joint : public ContactGenerator {
public:
  /**
   * Holds the two rigid bodies that are connected by this joint.
   */
  RigidBody *body[2];

  /**
   * Holds the relative location of the connection for each
   * body, given in local coordinates.
   */
  Vector3 position[2];

  /**
   * Holds the maximum displacement at the joint before the
   * joint is considered to be violated. This is normally a
   * small, epsilon value.  It can be larger, however, in which
   * case the joint will behave as if an inelastic cable joined
   * the bodies at their joint locations.
   */
  real error;

  /**
   * SAKBox: the impulse the joint applied last step when contacts are
   * resolved together (ContactResolver::Method::sequentialImpulses), which
   * starts the next step (warm starting): on body two, in world
   * coordinates; body one gets the opposite. Set it to zero after moving
   * the bodies by hand.
   */
  Vector3 impulse;

  /**
   * Configures the joint in one go.
   */
  void set(RigidBody *a, const Vector3 &a_pos, RigidBody *b, const Vector3 &b_pos, real error);

  /**
   * Generates the contacts required to restore the joint if it
   * has been violated. (SAKBox: for the book's resolver. Resolving contacts
   * together takes the joints themselves; see
   * ContactResolver::resolveContacts.)
   */
  unsigned addContact(Contact *contact, unsigned limit) const override;

  /** SAKBox: the most angular constraints a joint has (a hinge's). */
  static constexpr unsigned maxAngularConstraints = 5;

  /**
   * SAKBox: the limits on how the two bodies turn relative to each other,
   * where they are now (from their orientations, which the resolver keeps
   * up to date as it moves them): written into `constraints`, which has
   * room for maxAngularConstraints, returning how many. A joint returns
   * them in the same order every step, so each can start from its last
   * impulse (angularImpulse). The resolver adds them to the anchor's when
   * resolving contacts together; the book's resolver doesn't use them.
   *
   * A plain Joint has none: its bodies turn freely about it, a ball and
   * socket. HingeJoint and ConeJoint add them.
   */
  virtual unsigned getAngularConstraints(AngularConstraint *constraints) const;

  /**
   * SAKBox: each angular constraint's impulse last step, in the order
   * getAngularConstraints gives them, to start the next step from (warm
   * starting, as `impulse` is for the anchor). Zero them after moving the
   * bodies by hand.
   */
  real angularImpulse[maxAngularConstraints] = {};
};

/**
 * SAKBox: a hinge, which the book names among the joints that "restrict
 * the ability of one object to rotate with respect to another" (section
 * 17.2.1): a ball and socket (set its `error` to zero) whose bodies may
 * only turn relative to each other about one axis, like a door or a knee.
 * Optionally its angle is limited to a range, and a motor drives it.
 *
 * The book doesn't build one (with its one-contact-at-a-time resolver it's
 * "quite frankly, inconvenient"), and suggests the Jacobian rows of chapter
 * 18 instead, which is how this is made: two holds keep the axes lined up,
 * and two limits and a motor act about the axis (getAngularConstraints).
 * They're the rows Erin Catto gives for a revolute joint (Box2D's, and
 * Bullet's btHingeConstraint), solved as he describes ("Iterative Dynamics
 * with Temporal Coherence", 2005; "Modeling and Solving Constraints", GDC
 * 2009). It needs contacts resolved together (sequentialImpulses).
 */
class HingeJoint : public Joint {
public:
  /** The hinge's axis in each body's coordinates, of unit length. */
  Vector3 axis[2];

  /**
   * A direction at right angles to the axis in each body's coordinates:
   * the hinge's angle is how far body two's has turned from body one's,
   * about body one's axis.
   */
  Vector3 reference[2];

  /** Whether the angle is kept between lowerLimit and upperLimit. */
  bool limited = false;
  real lowerLimit = 0, upperLimit = 0;

  /**
   * The motor: drives the angle at motorSpeed (rad/s) with a twist of up to
   * motorTorque (N m). Off at zero torque.
   */
  real motorSpeed = 0, motorTorque = 0;

  /**
   * Sets the axis, given in world coordinates, as the bodies are now (put
   * them in place and set the anchors first, with set()): the angle is zero
   * as they are, and grows as body two turns about the axis by the right-
   * hand rule.
   */
  void setAxis(const Vector3 &worldAxis);

  /** Keeps the angle between `lower` and `upper` (rad). */
  void setLimits(real lower, real upper);

  /** Drives the angle at `speed` (rad/s) with up to `maxTorque` (N m). */
  void setMotor(real speed, real maxTorque);

  /** The angle (rad, from -pi to pi), as the bodies are now. */
  real getAngle() const;

  unsigned getAngularConstraints(AngularConstraint *constraints) const override;
};

/**
 * SAKBox: a ball and socket that only lets body two's axis tilt so far from
 * body one's, in any direction, like a neck or a shoulder (the "swing" of
 * Bullet's btConeTwistConstraint, without its twist limit). Built as one
 * limit row, as HingeJoint's are. It needs contacts resolved together.
 */
class ConeJoint : public Joint {
public:
  /** The axis in each body's coordinates, of unit length. */
  Vector3 axis[2];

  /** How far body two's axis may tilt from body one's (rad). */
  real maxAngle = 0;

  /**
   * Sets the axis, given in world coordinates, as the bodies are now (set
   * the anchors first, with set()), and how far it may tilt.
   */
  void setAxis(const Vector3 &worldAxis, real maxAngle);

  /** How far body two's axis is tilted from body one's (rad), now. */
  real getAngle() const;

  unsigned getAngularConstraints(AngularConstraint *constraints) const override;
};

} // namespace cyclone

#endif // CYCLONE_JOINTS_H