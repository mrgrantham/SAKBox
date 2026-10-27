/*
 * Implementation file for joints.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <cyclone/cyclone.h>

using namespace cyclone;

unsigned Joint::addContact(Contact *contact, unsigned limit) const {
  // Calculate the position of each connection point in world coordinates
  Vector3 a_pos_world = body[0]->getPointInWorldSpace(position[0]);
  Vector3 b_pos_world = body[1]->getPointInWorldSpace(position[1]);

  // Calculate the length of the joint
  Vector3 a_to_b = b_pos_world - a_pos_world;
  Vector3 normal = a_to_b;
  normal.normalise();
  real length = a_to_b.magnitude();

  // Check if it is violated
  if (real_abs(length) > error) {
    contact->body[0] = body[0];
    contact->body[1] = body[1];
    contact->contactNormal = normal;
    contact->contactPoint = (a_pos_world + b_pos_world) * 0.5f;
    contact->penetration = length - error;
    contact->friction = 1.0f;
    contact->restitution = 0;
    return 1;
  }

  return 0;
}

void Joint::set(RigidBody *a, const Vector3 &a_pos, RigidBody *b, const Vector3 &b_pos,
                real error) {
  body[0] = a;
  body[1] = b;

  position[0] = a_pos;
  position[1] = b_pos;

  Joint::error = error;
}

// SAKBox: angular constraints, for hinges and cones

unsigned Joint::getAngularConstraints(AngularConstraint *) const { return 0; }

namespace {
// The direction `local` on `body` points in now, in world coordinates
Vector3 directionNow(const RigidBody *body, const Vector3 &local) {
  return body->getOrientationNow().transform(local);
}

// The direction `world` points in, in `body`'s coordinates
Vector3 directionInBody(const RigidBody *body, const Vector3 &world) {
  return body->getOrientationNow().transformTranspose(world);
}

// A unit direction at right angles to `direction` (unit length)
Vector3 perpendicularTo(const Vector3 &direction) {
  // Crossed with whichever world axis is furthest from it
  const Vector3 other = real_abs(direction.x) < (real)0.6 ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
  Vector3 perpendicular = direction % other;
  perpendicular.normalise();
  return perpendicular;
}

// The angle (rad) from unit direction `from` to unit direction `to`, about
// `axis`, both at right angles to it
real angleAbout(const Vector3 &axis, const Vector3 &from, const Vector3 &to) {
  return real_atan2((from % to) * axis, from * to);
}
} // namespace

void HingeJoint::setAxis(const Vector3 &worldAxis) {
  Vector3 unitAxis = worldAxis;
  unitAxis.normalise();
  const Vector3 worldReference = perpendicularTo(unitAxis);
  for (int b = 0; b < 2; b++) {
    axis[b] = directionInBody(body[b], unitAxis);
    reference[b] = directionInBody(body[b], worldReference);
  }
}

void HingeJoint::setLimits(real lower, real upper) {
  limited = true;
  lowerLimit = lower;
  upperLimit = upper;
}

void HingeJoint::setMotor(real speed, real maxTorque) {
  motorSpeed = speed;
  motorTorque = maxTorque;
}

real HingeJoint::getAngle() const {
  return angleAbout(directionNow(body[0], axis[0]), directionNow(body[0], reference[0]),
                    directionNow(body[1], reference[1]));
}

unsigned HingeJoint::getAngularConstraints(AngularConstraint *constraints) const {
  const Vector3 axisOne = directionNow(body[0], axis[0]);
  const Vector3 axisTwo = directionNow(body[1], axis[1]);
  const Vector3 referenceOne = directionNow(body[0], reference[0]);
  unsigned count = 0;

  // Hold the axes in line: body two's axis has turned off body one's by
  // about axisOne x axisTwo (its length the sine of the angle between them),
  // undone about two directions at right angles to the axis
  const Vector3 turnedOff = axisOne % axisTwo;
  for (const Vector3 &across : {referenceOne, axisOne % referenceOne})
    constraints[count++] = {
        .kind = AngularConstraint::Kind::hold, .axis = across, .angle = turnedOff * across};

  // The limits: turning about the axis raises the angle, so away from the
  // lower limit; turning the other way is away from the upper one
  if (limited) {
    const real angle = getAngle();
    constraints[count++] = {
        .kind = AngularConstraint::Kind::limit, .axis = axisOne, .angle = angle - lowerLimit};
    constraints[count++] = {
        .kind = AngularConstraint::Kind::limit, .axis = axisOne * -1, .angle = upperLimit - angle};
  }

  if (motorTorque > 0)
    constraints[count++] = {.kind = AngularConstraint::Kind::motor,
                            .axis = axisOne,
                            .speed = motorSpeed,
                            .maxTorque = motorTorque};
  return count;
}

void ConeJoint::setAxis(const Vector3 &worldAxis, real maxAngle) {
  Vector3 unitAxis = worldAxis;
  unitAxis.normalise();
  for (int b = 0; b < 2; b++)
    axis[b] = directionInBody(body[b], unitAxis);
  ConeJoint::maxAngle = maxAngle;
}

real ConeJoint::getAngle() const {
  const Vector3 axisOne = directionNow(body[0], axis[0]);
  const Vector3 axisTwo = directionNow(body[1], axis[1]);
  return real_atan2((axisOne % axisTwo).magnitude(), axisOne * axisTwo);
}

unsigned ConeJoint::getAngularConstraints(AngularConstraint *constraints) const {
  // Tilting body two about axisOne x axisTwo tilts its axis further, so
  // the limit turns it the other way. Lined up, there's no way it's tilting,
  // and it's as far from the limit as it can be.
  const Vector3 tilt = directionNow(body[0], axis[0]) % directionNow(body[1], axis[1]);
  const real sine = tilt.magnitude();
  if (sine < (real)1e-6)
    return 0;
  constraints[0] = {.kind = AngularConstraint::Kind::limit,
                    .axis = tilt * ((real)-1 / sine),
                    .angle = maxAngle - getAngle()};
  return 1;
}