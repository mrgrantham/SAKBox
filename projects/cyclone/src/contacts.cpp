/*
 * Implementation file for the contact resolution system.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under licence. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software licence.
 */

#include <algorithm>
#include <assert.h>
#include <cyclone/contacts.h>
#include <cyclone/joints.h>
#include <memory.h>
#include <tuple>
#include <utility>

using namespace cyclone;

// Contact implementation

void Contact::setBodyData(RigidBody *one, RigidBody *two, real friction, real restitution) {
  Contact::body[0] = one;
  Contact::body[1] = two;
  Contact::friction = friction;
  Contact::restitution = restitution;
}

void Contact::matchAwakeState() {
  // Collisions with the world never cause a body to wake up.
  if (!body[1])
    return;

  bool body0awake = body[0]->getAwake();
  bool body1awake = body[1]->getAwake();

  // Wake up only the sleeping one
  if (body0awake ^ body1awake) {
    if (body0awake)
      body[1]->setAwake();
    else
      body[0]->setAwake();
  }
}

/*
 * Swaps the bodies in the current contact, so body 0 is at body 1 and
 * vice versa. This also changes the direction of the contact normal,
 * but doesn't update any calculated internal data. If you are calling
 * this method manually, then call calculateInternals afterwards to
 * make sure the internal data is up to date.
 */
void Contact::swapBodies() {
  contactNormal *= -1;

  RigidBody *temp = body[0];
  body[0] = body[1];
  body[1] = temp;
}

/*
 * Constructs an arbitrary orthonormal basis for the contact.  This is
 * stored as a 3x3 matrix, where each vector is a column (in other
 * words the matrix transforms contact space into world space). The x
 * direction is generated from the contact normal, and the y and z
 * directionss are set so they are at right angles to it.
 */
inline void Contact::calculateContactBasis() {
  Vector3 contactTangent[2];

  // Check whether the Z-axis is nearer to the X or Y axis
  if (real_abs(contactNormal.x) > real_abs(contactNormal.y)) {
    // Scaling factor to ensure the results are normalised
    const real s = (real)1.0f /
                   real_sqrt(contactNormal.z * contactNormal.z + contactNormal.x * contactNormal.x);

    // The new X-axis is at right angles to the world Y-axis
    contactTangent[0].x = contactNormal.z * s;
    contactTangent[0].y = 0;
    contactTangent[0].z = -contactNormal.x * s;

    // The new Y-axis is at right angles to the new X- and Z- axes
    contactTangent[1].x = contactNormal.y * contactTangent[0].x;
    contactTangent[1].y =
        contactNormal.z * contactTangent[0].x - contactNormal.x * contactTangent[0].z;
    contactTangent[1].z = -contactNormal.y * contactTangent[0].x;
  } else {
    // Scaling factor to ensure the results are normalised
    const real s = (real)1.0 /
                   real_sqrt(contactNormal.z * contactNormal.z + contactNormal.y * contactNormal.y);

    // The new X-axis is at right angles to the world X-axis
    contactTangent[0].x = 0;
    contactTangent[0].y = -contactNormal.z * s;
    contactTangent[0].z = contactNormal.y * s;

    // The new Y-axis is at right angles to the new X- and Z- axes
    contactTangent[1].x =
        contactNormal.y * contactTangent[0].z - contactNormal.z * contactTangent[0].y;
    contactTangent[1].y = -contactNormal.x * contactTangent[0].z;
    contactTangent[1].z = contactNormal.x * contactTangent[0].y;
  }

  // Make a matrix from the three vectors.
  contactToWorld.setComponents(contactNormal, contactTangent[0], contactTangent[1]);
}

Vector3 Contact::calculateLocalVelocity(unsigned bodyIndex, real duration) {
  RigidBody *thisBody = body[bodyIndex];

  // Work out the velocity of the contact point.
  Vector3 velocity = thisBody->getRotation() % relativeContactPosition[bodyIndex];
  velocity += thisBody->getVelocity();

  // Turn the velocity into contact-coordinates.
  Vector3 contactVelocity = contactToWorld.transformTranspose(velocity);

  // Calculate the ammount of velocity that is due to forces without
  // reactions.
  Vector3 accVelocity = thisBody->getLastFrameAcceleration() * duration;

  // Calculate the velocity in contact-coordinates.
  accVelocity = contactToWorld.transformTranspose(accVelocity);

  // We ignore any component of acceleration in the contact normal
  // direction, we are only interested in planar acceleration
  accVelocity.x = 0;

  // Add the planar velocities - if there's enough friction they will
  // be removed during velocity resolution
  contactVelocity += accVelocity;

  // And return it
  return contactVelocity;
}

void Contact::calculateDesiredDeltaVelocity(real duration) {
  const static real velocityLimit = (real)0.25f;

  // Calculate the acceleration induced velocity accumulated this frame
  real velocityFromAcc = 0;

  if (body[0]->getAwake()) {
    velocityFromAcc += body[0]->getLastFrameAcceleration() * duration * contactNormal;
  }

  if (body[1] && body[1]->getAwake()) {
    velocityFromAcc -= body[1]->getLastFrameAcceleration() * duration * contactNormal;
  }

  // If the velocity is very slow, limit the restitution
  real thisRestitution = restitution;
  if (real_abs(contactVelocity.x) < velocityLimit) {
    thisRestitution = (real)0.0f;
  }

  // Combine the bounce velocity with the removed
  // acceleration velocity.
  desiredDeltaVelocity =
      -contactVelocity.x - thisRestitution * (contactVelocity.x - velocityFromAcc);
}

void Contact::keepAnchors() {
  anchor[0] = body[0]->getPointInLocalSpace(contactPoint);
  anchor[1] = body[1] ? body[1]->getPointInLocalSpace(contactPoint) : contactPoint;
  foundPenetration = penetration;
}

void Contact::followAnchors(real duration) {
  const Vector3 onOne = body[0]->getPointInWorldSpace(anchor[0]);
  const Vector3 onTwo = body[1] ? body[1]->getPointInWorldSpace(anchor[1]) : anchor[1];
  // The normal points from body two to body one, so body one moving along
  // it, away from body two, takes away from the penetration
  penetration = foundPenetration - (onOne - onTwo) * contactNormal;
  // The point and its offsets from the bodies stay as found, matching how
  // the resolver worked out the bodies respond there for the step (as
  // Box2D keeps its anchors for a step: working out the responses afresh
  // each sub-step costs as much, and responses that don't match the
  // offsets make the sweeps overshoot, more each sweep)
  contactVelocity = calculateLocalVelocity(0, duration);
  if (body[1])
    contactVelocity -= calculateLocalVelocity(1, duration);
  calculateDesiredDeltaVelocity(duration);
}

void Contact::calculateInternals(real duration) {
  // Check if the first object is NULL, and swap if it is.
  if (!body[0])
    swapBodies();
  assert(body[0]);

  // Calculate an set of axis at the contact point.
  calculateContactBasis();

  // Store the relative position of the contact relative to each body
  relativeContactPosition[0] = contactPoint - body[0]->getPosition();
  if (body[1]) {
    relativeContactPosition[1] = contactPoint - body[1]->getPosition();
  }

  // Find the relative velocity of the bodies at the contact point.
  contactVelocity = calculateLocalVelocity(0, duration);
  if (body[1]) {
    contactVelocity -= calculateLocalVelocity(1, duration);
  }

  // Calculate the desired change in velocity for resolution
  calculateDesiredDeltaVelocity(duration);
}

void Contact::applyVelocityChange(Vector3 velocityChange[2], Vector3 rotationChange[2]) {
  // Get hold of the inverse mass and inverse inertia tensor, both in
  // world coordinates.
  Matrix3 inverseInertiaTensor[2];
  body[0]->getInverseInertiaTensorWorld(&inverseInertiaTensor[0]);
  if (body[1])
    body[1]->getInverseInertiaTensorWorld(&inverseInertiaTensor[1]);

  // We will calculate the impulse for each contact axis
  Vector3 impulseContact;

  if (friction == (real)0.0) {
    // Use the short format for frictionless contacts
    impulseContact = calculateFrictionlessImpulse(inverseInertiaTensor);
  } else {
    // Otherwise we may have impulses that aren't in the direction of the
    // contact, so we need the more complex version.
    impulseContact = calculateFrictionImpulse(inverseInertiaTensor);
  }

  // Convert impulse to world coordinates
  Vector3 impulse = contactToWorld.transform(impulseContact);

  // Split in the impulse into linear and rotational components
  Vector3 impulsiveTorque = relativeContactPosition[0] % impulse;
  rotationChange[0] = inverseInertiaTensor[0].transform(impulsiveTorque);
  velocityChange[0].clear();
  velocityChange[0].addScaledVector(impulse, body[0]->getInverseMass());

  // Apply the changes
  body[0]->addVelocity(velocityChange[0]);
  body[0]->addRotation(rotationChange[0]);

  if (body[1]) {
    // Work out body one's linear and angular changes
    Vector3 impulsiveTorque = impulse % relativeContactPosition[1];
    rotationChange[1] = inverseInertiaTensor[1].transform(impulsiveTorque);
    velocityChange[1].clear();
    velocityChange[1].addScaledVector(impulse, -body[1]->getInverseMass());

    // And apply them.
    body[1]->addVelocity(velocityChange[1]);
    body[1]->addRotation(rotationChange[1]);
  }
}

inline Vector3 Contact::calculateFrictionlessImpulse(Matrix3 *inverseInertiaTensor) {
  Vector3 impulseContact;

  // Build a vector that shows the change in velocity in
  // world space for a unit impulse in the direction of the contact
  // normal.
  Vector3 deltaVelWorld = relativeContactPosition[0] % contactNormal;
  deltaVelWorld = inverseInertiaTensor[0].transform(deltaVelWorld);
  deltaVelWorld = deltaVelWorld % relativeContactPosition[0];

  // Work out the change in velocity in contact coordiantes.
  real deltaVelocity = deltaVelWorld * contactNormal;

  // Add the linear component of velocity change
  deltaVelocity += body[0]->getInverseMass();

  // Check if we need to the second body's data
  if (body[1]) {
    // Go through the same transformation sequence again
    Vector3 deltaVelWorld = relativeContactPosition[1] % contactNormal;
    deltaVelWorld = inverseInertiaTensor[1].transform(deltaVelWorld);
    deltaVelWorld = deltaVelWorld % relativeContactPosition[1];

    // Add the change in velocity due to rotation
    deltaVelocity += deltaVelWorld * contactNormal;

    // Add the change in velocity due to linear motion
    deltaVelocity += body[1]->getInverseMass();
  }

  // Calculate the required size of the impulse
  impulseContact.x = desiredDeltaVelocity / deltaVelocity;
  impulseContact.y = 0;
  impulseContact.z = 0;
  return impulseContact;
}

inline Vector3 Contact::calculateFrictionImpulse(Matrix3 *inverseInertiaTensor) {
  Vector3 impulseContact;
  real inverseMass = body[0]->getInverseMass();

  // The equivalent of a cross product in matrices is multiplication
  // by a skew symmetric matrix - we build the matrix for converting
  // between linear and angular quantities.
  Matrix3 impulseToTorque;
  impulseToTorque.setSkewSymmetric(relativeContactPosition[0]);

  // Build the matrix to convert contact impulse to change in velocity
  // in world coordinates.
  Matrix3 deltaVelWorld = impulseToTorque;
  deltaVelWorld *= inverseInertiaTensor[0];
  deltaVelWorld *= impulseToTorque;
  deltaVelWorld *= -1;

  // Check if we need to add body two's data
  if (body[1]) {
    // Set the cross product matrix
    impulseToTorque.setSkewSymmetric(relativeContactPosition[1]);

    // Calculate the velocity change matrix
    Matrix3 deltaVelWorld2 = impulseToTorque;
    deltaVelWorld2 *= inverseInertiaTensor[1];
    deltaVelWorld2 *= impulseToTorque;
    deltaVelWorld2 *= -1;

    // Add to the total delta velocity.
    deltaVelWorld += deltaVelWorld2;

    // Add to the inverse mass
    inverseMass += body[1]->getInverseMass();
  }

  // Do a change of basis to convert into contact coordinates.
  Matrix3 deltaVelocity = contactToWorld.transpose();
  deltaVelocity *= deltaVelWorld;
  deltaVelocity *= contactToWorld;

  // Add in the linear velocity change
  deltaVelocity.data[0] += inverseMass;
  deltaVelocity.data[4] += inverseMass;
  deltaVelocity.data[8] += inverseMass;

  // Invert to get the impulse needed per unit velocity
  Matrix3 impulseMatrix = deltaVelocity.inverse();

  // Find the target velocities to kill
  Vector3 velKill(desiredDeltaVelocity, -contactVelocity.y, -contactVelocity.z);

  // Find the impulse to kill target velocities
  impulseContact = impulseMatrix.transform(velKill);

  // Check for exceeding friction
  real planarImpulse =
      real_sqrt(impulseContact.y * impulseContact.y + impulseContact.z * impulseContact.z);
  if (planarImpulse > impulseContact.x * friction) {
    // We need to use dynamic friction
    impulseContact.y /= planarImpulse;
    impulseContact.z /= planarImpulse;

    impulseContact.x = deltaVelocity.data[0] + deltaVelocity.data[1] * friction * impulseContact.y +
                       deltaVelocity.data[2] * friction * impulseContact.z;
    impulseContact.x = desiredDeltaVelocity / impulseContact.x;
    impulseContact.y *= friction * impulseContact.x;
    impulseContact.z *= friction * impulseContact.x;
  }
  return impulseContact;
}

void Contact::applyPositionChange(Vector3 linearChange[2], Vector3 angularChange[2],
                                  real penetration) {
  const real angularLimit = (real)0.2f;
  real angularMove[2];
  real linearMove[2];

  real totalInertia = 0;
  real linearInertia[2];
  real angularInertia[2];

  // We need to work out the inertia of each object in the direction
  // of the contact normal, due to angular inertia only.
  for (unsigned i = 0; i < 2; i++)
    if (body[i]) {
      Matrix3 inverseInertiaTensor;
      body[i]->getInverseInertiaTensorWorld(&inverseInertiaTensor);

      // Use the same procedure as for calculating frictionless
      // velocity change to work out the angular inertia.
      Vector3 angularInertiaWorld = relativeContactPosition[i] % contactNormal;
      angularInertiaWorld = inverseInertiaTensor.transform(angularInertiaWorld);
      angularInertiaWorld = angularInertiaWorld % relativeContactPosition[i];
      angularInertia[i] = angularInertiaWorld * contactNormal;

      // The linear component is simply the inverse mass
      linearInertia[i] = body[i]->getInverseMass();

      // Keep track of the total inertia from all components
      totalInertia += linearInertia[i] + angularInertia[i];

      // We break the loop here so that the totalInertia value is
      // completely calculated (by both iterations) before
      // continuing.
    }

  // Loop through again calculating and applying the changes
  for (unsigned i = 0; i < 2; i++)
    if (body[i]) {
      // The linear and angular movements required are in proportion to
      // the two inverse inertias.
      real sign = (i == 0) ? 1 : -1;
      angularMove[i] = sign * penetration * (angularInertia[i] / totalInertia);
      linearMove[i] = sign * penetration * (linearInertia[i] / totalInertia);

      // To avoid angular projections that are too great (when mass is large
      // but inertia tensor is small) limit the angular move.
      Vector3 projection = relativeContactPosition[i];
      projection.addScaledVector(contactNormal,
                                 -relativeContactPosition[i].scalarProduct(contactNormal));

      // Use the small angle approximation for the sine of the angle (i.e.
      // the magnitude would be sine(angularLimit) * projection.magnitude
      // but we approximate sine(angularLimit) to angularLimit).
      real maxMagnitude = angularLimit * projection.magnitude();

      if (angularMove[i] < -maxMagnitude) {
        real totalMove = angularMove[i] + linearMove[i];
        angularMove[i] = -maxMagnitude;
        linearMove[i] = totalMove - angularMove[i];
      } else if (angularMove[i] > maxMagnitude) {
        real totalMove = angularMove[i] + linearMove[i];
        angularMove[i] = maxMagnitude;
        linearMove[i] = totalMove - angularMove[i];
      }

      // We have the linear amount of movement required by turning
      // the rigid body (in angularMove[i]). We now need to
      // calculate the desired rotation to achieve that.
      if (angularMove[i] == 0) {
        // Easy case - no angular movement means no rotation.
        angularChange[i].clear();
      } else {
        // Work out the direction we'd like to rotate in.
        Vector3 targetAngularDirection = relativeContactPosition[i].vectorProduct(contactNormal);

        Matrix3 inverseInertiaTensor;
        body[i]->getInverseInertiaTensorWorld(&inverseInertiaTensor);

        // Work out the direction we'd need to rotate to achieve that
        angularChange[i] = inverseInertiaTensor.transform(targetAngularDirection) *
                           (angularMove[i] / angularInertia[i]);
      }

      // Velocity change is easier - it is just the linear movement
      // along the contact normal.
      linearChange[i] = contactNormal * linearMove[i];

      // Now we can start to apply the values we've calculated.
      // Apply the linear movement
      Vector3 pos;
      body[i]->getPosition(&pos);
      pos.addScaledVector(contactNormal, linearMove[i]);
      body[i]->setPosition(pos);

      // And the change in orientation
      Quaternion q;
      body[i]->getOrientation(&q);
      q.addScaledVector(angularChange[i], ((real)1.0));
      body[i]->setOrientation(q);

      // We need to calculate the derived data for any body that is
      // asleep, so that the changes are reflected in the object's
      // data. Otherwise the resolution will not change the position
      // of the object, and the next collision detection round will
      // have the same penetration.
      if (!body[i]->getAwake())
        body[i]->calculateDerivedData();
    }
}

// Contact resolver implementation

ContactResolver::ContactResolver(unsigned iterations, real velocityEpsilon, real positionEpsilon) {
  setIterations(iterations, iterations);
  setEpsilon(velocityEpsilon, positionEpsilon);
}

ContactResolver::ContactResolver(unsigned velocityIterations, unsigned positionIterations,
                                 real velocityEpsilon, real positionEpsilon) {
  setIterations(velocityIterations);
  setEpsilon(velocityEpsilon, positionEpsilon);
}

void ContactResolver::setIterations(unsigned iterations) { setIterations(iterations, iterations); }

void ContactResolver::setIterations(unsigned velocityIterations, unsigned positionIterations) {
  ContactResolver::velocityIterations = velocityIterations;
  ContactResolver::positionIterations = positionIterations;
}

void ContactResolver::setSweeps(unsigned velocitySweeps, unsigned settleSweeps,
                                unsigned positionSweeps) {
  ContactResolver::velocitySweeps = velocitySweeps;
  ContactResolver::settleSweeps = settleSweeps;
  ContactResolver::positionSweeps = positionSweeps;
}

void ContactResolver::setEpsilon(real velocityEpsilon, real positionEpsilon) {
  ContactResolver::velocityEpsilon = velocityEpsilon;
  ContactResolver::positionEpsilon = positionEpsilon;
}

void ContactResolver::resolveContacts(Contact *contacts, unsigned numContacts, real duration) {
  resolveContacts(contacts, numContacts, duration, {});
}

void ContactResolver::resolveContacts(Contact *contacts, unsigned numContacts, real duration,
                                      std::span<Joint *const> joints) {
  if (!isValid())
    return;
  bodySlots.clear();

  if (method == Method::sequentialImpulses) {
    if (numContacts == 0 && joints.empty())
      return;
    beginStep(contacts, numContacts, duration, joints);
    resolveSubStep(duration, false);
    endStep();
    return;
  }

  // SAKBox: the book's resolver takes each joint as the contact it makes
  if (!joints.empty()) {
    contactsWithJoints.assign(contacts, contacts + numContacts);
    for (Joint *joint : joints) {
      Contact contact;
      if (joint->addContact(&contact, 1))
        contactsWithJoints.push_back(contact);
    }
    contacts = contactsWithJoints.data();
    numContacts = (unsigned)contactsWithJoints.size();
  }

  // Make sure we have something to do.
  if (numContacts == 0)
    return;

  // Prepare the contacts for processing
  prepareContacts(contacts, numContacts, duration);

  // Resolve the interpenetration problems with the contacts.
  adjustPositions(contacts, numContacts, duration);

  // Resolve the velocity problems with the contacts.
  adjustVelocities(contacts, numContacts, duration);

  sleepInGroups(contacts, numContacts);
}

void ContactResolver::prepareContacts(Contact *contacts, unsigned numContacts, real duration) {
  // Generate contact velocity and axis information.
  Contact *lastContact = contacts + numContacts;
  for (Contact *contact = contacts; contact < lastContact; contact++) {
    // Calculate the internal contact data (inertia, basis, etc).
    contact->calculateInternals(duration);
  }
}

void ContactResolver::adjustVelocities(Contact *c, unsigned numContacts, real duration) {
  Vector3 velocityChange[2], rotationChange[2];
  Vector3 deltaVel;

  // iteratively handle impacts in order of severity.
  velocityIterationsUsed = 0;
  while (velocityIterationsUsed < velocityIterations) {
    // Find contact with maximum magnitude of probable velocity change.
    real max = velocityEpsilon;
    unsigned index = numContacts;
    for (unsigned i = 0; i < numContacts; i++) {
      if (c[i].desiredDeltaVelocity > max) {
        max = c[i].desiredDeltaVelocity;
        index = i;
      }
    }
    if (index == numContacts)
      break;

    // Match the awake state at the contact
    c[index].matchAwakeState();

    // Do the resolution on the contact that came out top.
    c[index].applyVelocityChange(velocityChange, rotationChange);

    // With the change in velocity of the two bodies, the update of
    // contact velocities means that some of the relative closing
    // velocities need recomputing.
    for (unsigned i = 0; i < numContacts; i++) {
      // Check each body in the contact
      for (unsigned b = 0; b < 2; b++)
        if (c[i].body[b]) {
          // Check for a match with each body in the newly
          // resolved contact
          for (unsigned d = 0; d < 2; d++) {
            if (c[i].body[b] == c[index].body[d]) {
              deltaVel = velocityChange[d] +
                         rotationChange[d].vectorProduct(c[i].relativeContactPosition[b]);

              // The sign of the change is negative if we're dealing
              // with the second body in a contact.
              c[i].contactVelocity +=
                  c[i].contactToWorld.transformTranspose(deltaVel) * (b ? -1 : 1);
              c[i].calculateDesiredDeltaVelocity(duration);
            }
          }
        }
    }
    velocityIterationsUsed++;
  }
}

void ContactResolver::adjustPositions(Contact *c, unsigned numContacts, real duration) {
  unsigned i, index;
  Vector3 linearChange[2], angularChange[2];
  real max;
  Vector3 deltaPosition;

  // iteratively resolve interpenetrations in order of severity.
  positionIterationsUsed = 0;
  while (positionIterationsUsed < positionIterations) {
    // Find biggest penetration
    max = positionEpsilon;
    index = numContacts;
    for (i = 0; i < numContacts; i++) {
      if (c[i].penetration > max) {
        max = c[i].penetration;
        index = i;
      }
    }
    if (index == numContacts)
      break;

    // Match the awake state at the contact
    c[index].matchAwakeState();

    // Resolve the penetration.
    c[index].applyPositionChange(linearChange, angularChange, max);

    // Again this action may have changed the penetration of other
    // bodies, so we update contacts.
    for (i = 0; i < numContacts; i++) {
      // Check each body in the contact
      for (unsigned b = 0; b < 2; b++)
        if (c[i].body[b]) {
          // Check for a match with each body in the newly
          // resolved contact
          for (unsigned d = 0; d < 2; d++) {
            if (c[i].body[b] == c[index].body[d]) {
              deltaPosition =
                  linearChange[d] + angularChange[d].vectorProduct(c[i].relativeContactPosition[b]);

              // The sign of the change is positive if we're
              // dealing with the second body in a contact
              // and negative otherwise (because we're
              // subtracting the resolution)..
              c[i].penetration += deltaPosition.scalarProduct(c[i].contactNormal) * (b ? 1 : -1);
            }
          }
        }
    }
    positionIterationsUsed++;
  }
}

ContactResolver::BodySlot &ContactResolver::slotOf(RigidBody *body) {
  const unsigned slot = body->getResolverSlot();
  if (slot < bodySlots.size() && bodySlots[slot].body == body)
    return bodySlots[slot];
  body->setResolverSlot(static_cast<unsigned>(bodySlots.size()));
  return bodySlots.emplace_back(BodySlot{.body = body});
}

void ContactResolver::rememberStart(RigidBody *body) {
  BodySlot &slot = slotOf(body);
  if (!slot.hasStart) {
    slot.start = Motion{body->getVelocity(), body->getRotation()};
    slot.hasStart = true;
  }
}

void ContactResolver::sleepInGroups(Contact *contacts, unsigned numContacts,
                                    std::span<Joint *const> joints) {
  // Join the movable bodies each contact touches, union-find style: each
  // body's slot points towards the slot of the body that names its group
  auto addToGroups = [this](RigidBody *body) {
    if (!body || !body->hasFiniteMass())
      return;
    BodySlot &slot = slotOf(body);
    if (!slot.inGroups) {
      slot.inGroups = true;
      slot.groupParent = body->getResolverSlot();
      slot.groupSize = 0;
      slot.groupSettled = true;
    }
  };
  auto groupOf = [this](unsigned slot) {
    unsigned root = slot;
    while (bodySlots[root].groupParent != root)
      root = bodySlots[root].groupParent;
    // Point each slot on the way straight at the root, so later lookups
    // are quick
    while (slot != root)
      slot = std::exchange(bodySlots[slot].groupParent, root);
    return root;
  };
  for (unsigned i = 0; i < numContacts; i++)
    for (RigidBody *body : contacts[i].body)
      addToGroups(body);
  // SAKBox: bodies joined by a joint are in the same group too
  for (const Joint *joint : joints)
    for (RigidBody *body : joint->body)
      addToGroups(body);
  auto join = [&](RigidBody *one, RigidBody *two) {
    if (one && two && one->hasFiniteMass() && two->hasFiniteMass())
      bodySlots[groupOf(one->getResolverSlot())].groupParent = groupOf(two->getResolverSlot());
  };
  for (unsigned i = 0; i < numContacts; i++)
    join(contacts[i].body[0], contacts[i].body[1]);
  for (const Joint *joint : joints)
    join(joint->body[0], joint->body[1]);

  // Tally each group, in the slot that names it: its size, and whether
  // every body in it has settled
  for (unsigned i = 0; i < bodySlots.size(); i++) {
    if (!bodySlots[i].inGroups)
      continue;
    BodySlot &group = bodySlots[groupOf(i)];
    group.groupSize++;
    const RigidBody *body = bodySlots[i].body;
    if (!body->getCanSleep() || (body->getAwake() && body->getMotion() >= sleepEpsilon))
      group.groupSettled = false;
  }

  for (unsigned i = 0; i < bodySlots.size(); i++) {
    if (!bodySlots[i].inGroups)
      continue;
    const BodySlot &group = bodySlots[groupOf(i)];
    RigidBody *body = bodySlots[i].body;
    body->setInContactGroup(group.groupSize > 1);
    if (group.groupSize == 1)
      continue;
    if (group.groupSettled && body->getAwake())
      body->setAwake(false);
  }
}

// SAKBox: Method::sequentialImpulses

namespace {
// A body moves in response to a contact unless it's missing (the ground),
// immovable, or asleep
RigidBody *movableBody(RigidBody *body) {
  return body && body->hasFiniteMass() && body->getAwake() ? body : nullptr;
}

// How much the speed of `body` along `direction`, at `offset` from its
// centre, changes for a unit impulse along `direction` there
real responseAlong(const RigidBody *body, const Vector3 &offset, const Vector3 &direction) {
  if (!body)
    return 0;
  const Vector3 turn = body->getInverseInertiaTensorWorld().transform(offset % direction);
  return body->getInverseMass() + (turn % offset) * direction;
}

// The velocity of the point at `offset` from the centre of a body moving
// with `linear` and `angular` velocity
Vector3 pointVelocity(const Vector3 &offset, const Vector3 &linear, const Vector3 &angular) {
  return linear + angular % offset;
}

// The change in `body`'s linear and angular velocity from `impulse` at
// `offset` from its centre
void addImpulse(const RigidBody *body, const Vector3 &offset, const Vector3 &impulse,
                Vector3 &linear, Vector3 &angular) {
  linear += impulse * body->getInverseMass();
  angular += body->getInverseInertiaTensorWorld().transform(offset % impulse);
}

// Scales `tangentImpulse` down, if need be, so friction stays within its
// limit: friction coefficient x push (the friction cone)
void limitFriction(real tangentImpulse[2], real limit) {
  const real friction =
      real_sqrt(tangentImpulse[0] * tangentImpulse[0] + tangentImpulse[1] * tangentImpulse[1]);
  if (friction > limit) {
    tangentImpulse[0] *= limit / friction;
    tangentImpulse[1] *= limit / friction;
  }
}

// Moves `body` as if it had moved with `linear` and `angular` velocity for
// `duration`
void moveBy(RigidBody *body, const Vector3 &linear, const Vector3 &angular, real duration) {
  body->setPosition(body->getPosition() + linear * duration);
  Quaternion orientation;
  body->getOrientation(&orientation);
  orientation.addScaledVector(angular, duration);
  body->setOrientation(orientation);
}

// Where the point at `local` on `body` is now, from its position and
// orientation (its transform matrix is only brought up to date by
// calculateDerivedData, which the resolver leaves to the next step:
// updating it mid-step made awake stacks creep)
Vector3 pointNow(const RigidBody *body, const Vector3 &local) {
  Quaternion orientation;
  body->getOrientation(&orientation);
  orientation.normalise();
  Matrix4 transform;
  transform.setOrientationAndPos(orientation, body->getPosition());
  return transform.transform(local);
}
} // namespace

Vector3 ContactResolver::takeKeptImpulse(const Contact &contact) {
  const auto kept = std::ranges::equal_range(
      lastImpulses,
      std::pair<const RigidBody *, const RigidBody *>(contact.body[0], contact.body[1]),
      std::less{}, &KeptImpulse::bodies);
  if (kept.empty())
    return Vector3();
  const Vector3 localPoint = contact.body[0]->getPointInLocalSpace(contact.contactPoint);
  KeptImpulse *nearest = nullptr;
  real nearestDistance = warmStartMatchDistance * warmStartMatchDistance;
  for (KeptImpulse &candidate : kept) {
    const real distance = (candidate.localPoint - localPoint).squareMagnitude();
    if (!candidate.taken && distance < nearestDistance) {
      nearestDistance = distance;
      nearest = &candidate;
    }
  }
  if (!nearest)
    return Vector3();
  nearest->taken = true;
  return nearest->impulse;
}

void ContactResolver::wakeTouched(std::span<Contact> contacts, std::span<Joint *const> joints) {
  // Wake a sleeping body touched by an awake one where the contact needs
  // resolving, as the book's resolver does, until no more wake: a body
  // that's woken can wake the next, so a knock carries through a stack.
  for (bool woke = true; woke;) {
    woke = false;
    for (Contact &contact : contacts) {
      if (!contact.body[1] || contact.body[0]->getAwake() == contact.body[1]->getAwake())
        continue;
      if (contact.desiredDeltaVelocity > velocityEpsilon || contact.penetration > positionEpsilon) {
        contact.matchAwakeState();
        woke = true;
      }
    }
    // A joint always wakes the body at its other end
    for (Joint *joint : joints)
      if (joint->body[0]->getAwake() != joint->body[1]->getAwake()) {
        joint->body[0]->setAwake();
        joint->body[1]->setAwake();
        woke = true;
      }
  }
}

void ContactResolver::beginStep(Contact *contacts, unsigned numContacts, real duration,
                                std::span<Joint *const> joints) {
  bodySlots.clear();
  stepContacts = std::span<Contact>(contacts, numContacts);
  stepJoints = joints;
  prepareContacts(contacts, numContacts, duration);
  for (Contact &contact : stepContacts)
    contact.keepAnchors();

  wakeTouched(stepContacts, joints);

  // Set up each contact: its directions, and, if a movable body takes part,
  // where it starts from (last step's impulse), kept for the step's
  // sub-steps (as the book suggests keeping contact calculations, section
  // 16.2.4)
  solverContacts.clear();
  for (unsigned i = 0; i < numContacts; i++) {
    Contact &contact = contacts[i];
    SolverContact solverContact{
        .contact = &contact,
        .movable = {movableBody(contact.body[0]), movableBody(contact.body[1])}};

    solverContact.normal = contact.contactNormal;
    solverContact.tangent[0] = contact.contactToWorld.getAxisVector(1);
    solverContact.tangent[1] = contact.contactToWorld.getAxisVector(2);

    // Warm start: begin with last step's impulse, as far as it still fits
    // this contact (pushing only, friction within the cone)
    const Vector3 kept = solverContact.active() ? takeKeptImpulse(contact) : Vector3();
    solverContact.normalImpulse = std::max(kept * solverContact.normal, (real)0);
    solverContact.tangentImpulse[0] = kept * solverContact.tangent[0];
    solverContact.tangentImpulse[1] = kept * solverContact.tangent[1];
    limitFriction(solverContact.tangentImpulse, contact.friction * solverContact.normalImpulse);
    solverContacts.push_back(solverContact);
  }
}

void ContactResolver::resolveSubStep(real duration, bool contactsFollowBodies) {
  velocityIterationsUsed = 0;

  // Each contact where its bodies are now, if they've moved since it was
  // found, and each movable body's velocities: integrate() has already
  // moved it by them
  for (BodySlot &slot : bodySlots)
    slot.hasStart = false;
  if (contactsFollowBodies) {
    for (SolverContact &solverContact : solverContacts)
      solverContact.contact->followAnchors(duration);
    // A sleeping body may need waking now: pressed by an awake one that's
    // moved since the step began (gravity has just sped it up, say)
    wakeTouched(stepContacts, stepJoints);
    for (SolverContact &solverContact : solverContacts)
      for (int b = 0; b < 2; b++)
        solverContact.movable[b] = movableBody(solverContact.contact->body[b]);
  }
  for (SolverContact &solverContact : solverContacts) {
    if (!solverContact.active())
      continue;
    Contact &contact = *solverContact.contact;
    for (RigidBody *body : solverContact.movable)
      if (body)
        rememberStart(body);

    // How the bodies respond along each direction, as they're turned now
    // (their inertia in world coordinates turns with them, so it's worked
    // out each sub-step: the response from the start of a step no longer
    // matches the impulses a turned body takes, and the sweeps overshoot)
    const Vector3 *offset = contact.relativeContactPosition;
    auto response = [&](const Vector3 &direction) {
      return responseAlong(solverContact.movable[0], offset[0], direction) +
             responseAlong(solverContact.movable[1], offset[1], direction);
    };
    solverContact.normalResponse = response(solverContact.normal);
    solverContact.tangentResponse[0] = response(solverContact.tangent[0]);
    solverContact.tangentResponse[1] = response(solverContact.tangent[1]);

    // The bodies end the step where they started plus the resolved velocity
    // x duration (see below), so this is the overlap they'd have if they
    // didn't move along the normal; negative while they're apart. Apart,
    // they may close the gap this step but no further, so the contact only
    // pushes once they touch; touching, they mustn't close at all. Faster
    // than the book's bounce threshold they bounce, as the book works out.
    solverContact.startNormalVelocity = contact.contactVelocity.x;
    const real overlapAtStart = contact.penetration + solverContact.startNormalVelocity * duration;
    solverContact.targetNormalVelocity = std::min(overlapAtStart, (real)0) / duration;
    solverContact.bounceVelocity =
        std::max(contact.contactVelocity.x + contact.desiredDeltaVelocity, (real)0);
    if (solverContact.bounceVelocity > 0)
      solverContact.targetNormalVelocity =
          std::max(solverContact.targetNormalVelocity, solverContact.bounceVelocity);
  }

  setUpJoints(stepJoints, duration);
  for (const SolverContact &solverContact : solverContacts) {
    if (!solverContact.active())
      continue;
    const Vector3 *offset = solverContact.contact->relativeContactPosition;
    const Vector3 impulse = solverContact.impulse();
    if (RigidBody *body = solverContact.movable[0]) {
      Vector3 linear, angular;
      addImpulse(body, offset[0], impulse, linear, angular);
      body->addVelocity(linear);
      body->addRotation(angular);
    }
    if (RigidBody *body = solverContact.movable[1]) {
      Vector3 linear, angular;
      addImpulse(body, offset[1], impulse * -1, linear, angular);
      body->addVelocity(linear);
      body->addRotation(angular);
    }
  }

  sweepVelocities(&SolverContact::targetNormalVelocity, velocitySweeps);

  // integrate() moved each body by its starting velocities; move it as if
  // it had had the resolved ones instead (semi-implicit Euler, as in
  // integrate()). This also undoes the step's fall into whatever it rests
  // on, and since the resolved velocities respect friction, so does the
  // move: there's no sideways slip for friction to miss.
  for (const BodySlot &slot : bodySlots)
    if (slot.hasStart)
      moveBy(slot.body, slot.body->getVelocity() - slot.start.linear,
             slot.body->getRotation() - slot.start.angular, duration);

  // Settle: contacts the move has just brought together end the step still
  // (or bouncing), rather than closing as fast as they did to get there;
  // those still apart may close the gap that's left.
  for (SolverContact &solverContact : solverContacts) {
    if (!solverContact.active())
      continue;
    if (solverContact.bounceVelocity > 0) {
      solverContact.targetNormalVelocity = solverContact.bounceVelocity;
    } else {
      const real gap = -overlapNow(solverContact, duration);
      solverContact.targetNormalVelocity = gap > 0 ? -gap / duration : (real)0;
    }
  }
  settleJoints(duration);
  sweepVelocities(&SolverContact::targetNormalVelocity, settleSweeps);

  removeOverlap(duration);

  // Keep the joints' impulses to start the next step from
  for (const SolverJoint &solverJoint : solverJoints)
    if (solverJoint.cable)
      solverJoint.joint->impulse = solverJoint.direction * -solverJoint.cableImpulse;
  for (const SolverAngularConstraint &solver : solverAngularConstraints)
    solver.joint->angularImpulse[solver.index] = solver.impulse;
}

void ContactResolver::endStep() {
  // Keep this step's impulses to start the next step from
  lastImpulses.clear();
  for (const SolverContact &solverContact : solverContacts) {
    if (!solverContact.active())
      continue;
    const Contact &contact = *solverContact.contact;
    lastImpulses.push_back(
        {.bodies = {contact.body[0], contact.body[1]},
         .order = static_cast<unsigned>(lastImpulses.size()),
         .localPoint = contact.body[0]->getPointInLocalSpace(contact.contactPoint),
         .impulse = solverContact.impulse()});
  }
  std::ranges::sort(lastImpulses, [](const KeptImpulse &a, const KeptImpulse &b) {
    return std::tie(a.bodies, a.order) < std::tie(b.bodies, b.order);
  });
  sleepInGroups(stepContacts.data(), static_cast<unsigned>(stepContacts.size()), stepJoints);
}

real ContactResolver::overlapNow(const SolverContact &solverContact, real duration) const {
  const Vector3 *offset = solverContact.contact->relativeContactPosition;
  Vector3 velocity;
  for (int b = 0; b < 2; b++)
    if (const RigidBody *body = solverContact.movable[b]) {
      const Vector3 point = pointVelocity(offset[b], body->getVelocity(), body->getRotation());
      velocity += b == 0 ? point : point * -1;
    }
  return solverContact.contact->penetration -
         (velocity * solverContact.normal - solverContact.startNormalVelocity) * duration;
}

void ContactResolver::sweepVelocities(real SolverContact::*target, unsigned sweeps) {
  // Each visit adjusts a contact's running totals by what it takes to reach
  // its target now, given what the other contacts have done, keeping the
  // totals within their limits: friction first, then the push.
  for (unsigned sweep = 0; sweep < sweeps; sweep++, velocityIterationsUsed++) {
    for (SolverContact &solverContact : solverContacts) {
      if (!solverContact.active())
        continue;
      const Vector3 *offset = solverContact.contact->relativeContactPosition;
      RigidBody *const *movable = solverContact.movable;
      auto relativeVelocity = [&] {
        Vector3 velocity;
        if (movable[0])
          velocity +=
              pointVelocity(offset[0], movable[0]->getVelocity(), movable[0]->getRotation());
        if (movable[1])
          velocity -=
              pointVelocity(offset[1], movable[1]->getVelocity(), movable[1]->getRotation());
        return velocity;
      };
      auto apply = [&](const Vector3 &impulse) {
        for (int b = 0; b < 2; b++)
          if (movable[b]) {
            Vector3 linear, angular;
            addImpulse(movable[b], offset[b], b == 0 ? impulse : impulse * -1, linear, angular);
            movable[b]->addVelocity(linear);
            movable[b]->addRotation(angular);
          }
      };

      // Friction: stop the sliding, up to friction x push
      Vector3 velocity = relativeVelocity();
      real tangentImpulse[2];
      for (int t = 0; t < 2; t++)
        tangentImpulse[t] =
            solverContact.tangentImpulse[t] -
            (velocity * solverContact.tangent[t]) / solverContact.tangentResponse[t];
      limitFriction(tangentImpulse, solverContact.contact->friction * solverContact.normalImpulse);
      apply(solverContact.tangent[0] * (tangentImpulse[0] - solverContact.tangentImpulse[0]) +
            solverContact.tangent[1] * (tangentImpulse[1] - solverContact.tangentImpulse[1]));
      solverContact.tangentImpulse[0] = tangentImpulse[0];
      solverContact.tangentImpulse[1] = tangentImpulse[1];

      // The push: reach the target normal velocity, pushing but never
      // pulling
      velocity = relativeVelocity();
      const real normalImpulse = std::max(
          solverContact.normalImpulse + (solverContact.*target - velocity * solverContact.normal) /
                                            solverContact.normalResponse,
          (real)0);
      apply(solverContact.normal * (normalImpulse - solverContact.normalImpulse));
      solverContact.normalImpulse = normalImpulse;
    }
    sweepJoints();
  }
}

void ContactResolver::removeOverlap(real duration) {
  // The overlap left after the move, beyond positionEpsilon, sets a
  // separating speed for each contact to reach on velocities of its own:
  // like the velocity sweeps, but along the normal only. They move the
  // bodies, and then are dropped, so removing overlap doesn't set bodies
  // moving.
  for (BodySlot &slot : bodySlots) {
    slot.separates = false;
    slot.separating = Motion{};
  }
  for (SolverContact &solverContact : solverContacts) {
    if (!solverContact.active())
      continue;
    for (RigidBody *body : solverContact.movable)
      if (body)
        slotOf(body).separates = true;
    solverContact.targetSeparatingSpeed =
        std::min(std::max(overlapNow(solverContact, duration) - positionEpsilon, (real)0) *
                     overlapCorrection,
                 maxCorrection) /
        duration;
  }
  setJointPositionTargets(duration);

  for (positionIterationsUsed = 0; positionIterationsUsed < positionSweeps;
       positionIterationsUsed++) {
    for (SolverContact &solverContact : solverContacts) {
      if (!solverContact.active() || solverContact.targetSeparatingSpeed <= 0)
        continue;
      const Vector3 *offset = solverContact.contact->relativeContactPosition;
      real separatingSpeed = 0;
      for (int b = 0; b < 2; b++)
        if (RigidBody *body = solverContact.movable[b]) {
          const Motion &motion = slotOf(body).separating;
          const real speed =
              pointVelocity(offset[b], motion.linear, motion.angular) * solverContact.normal;
          separatingSpeed += b == 0 ? speed : -speed;
        }
      const real positionImpulse = std::max(
          solverContact.positionImpulse + (solverContact.targetSeparatingSpeed - separatingSpeed) /
                                              solverContact.normalResponse,
          (real)0);
      const Vector3 change =
          solverContact.normal * (positionImpulse - solverContact.positionImpulse);
      for (int b = 0; b < 2; b++)
        if (RigidBody *body = solverContact.movable[b]) {
          Motion &motion = slotOf(body).separating;
          addImpulse(body, offset[b], b == 0 ? change : change * -1, motion.linear, motion.angular);
        }
      solverContact.positionImpulse = positionImpulse;
    }
    sweepJointPositions();
  }

  for (const BodySlot &slot : bodySlots)
    if (slot.separates)
      moveBy(slot.body, slot.separating.linear, slot.separating.angular, duration);
}

// SAKBox: joints when resolving contacts together

namespace {
// How much the velocity of the point at `offset` from the centre of `body`
// changes for a unit impulse there, in each direction: 1/mass, less
// [offset]x I^-1 [offset]x for the turning (zero for no body)
Matrix3 pointResponse(const RigidBody *body, const Vector3 &offset) {
  Matrix3 response(0, 0, 0, 0, 0, 0, 0, 0, 0);
  if (!body)
    return response;
  response.setDiagonal(body->getInverseMass(), body->getInverseMass(), body->getInverseMass());
  Matrix3 cross;
  cross.setSkewSymmetric(offset);
  Matrix3 turning = cross * body->getInverseInertiaTensorWorld() * cross;
  turning *= -1;
  response += turning;
  return response;
}

// The velocity of the joint's anchor `end` (0 or 1), moving with the
// body's current velocities (zero for no body)
Vector3 anchorVelocity(const RigidBody *body, const Vector3 &offset) {
  return body ? pointVelocity(offset, body->getVelocity(), body->getRotation()) : Vector3();
}

// Applies `impulse` to anchor two and the opposite to anchor one, changing
// the velocities each end of `motion` gives (the bodies' own, or the
// position sweeps')
template <typename ApplyTo>
void applyAcrossJoint(RigidBody *const movable[2], const Vector3 offset[2], const Vector3 &impulse,
                      ApplyTo applyTo) {
  for (int end = 0; end < 2; end++)
    if (movable[end]) {
      Vector3 linear, angular;
      addImpulse(movable[end], offset[end], end == 1 ? impulse : impulse * -1, linear, angular);
      applyTo(movable[end], linear, angular);
    }
}

void changeVelocity(RigidBody *body, const Vector3 &linear, const Vector3 &angular) {
  body->addVelocity(linear);
  body->addRotation(angular);
}

// How much the turning speed of `body` about `axis` changes for a unit
// twist about it (zero for no body)
real turnResponse(const RigidBody *body, const Vector3 &axis) {
  return body ? body->getInverseInertiaTensorWorld().transform(axis) * axis : (real)0;
}

// How fast body two turns relative to body one about `axis`, given each
// body's angular velocity by `spin` (the bodies' own, or the position
// sweeps')
template <typename Spin>
real relativeTurning(RigidBody *const movable[2], const Vector3 &axis, Spin spin) {
  real speed = 0;
  if (movable[1])
    speed += spin(movable[1]) * axis;
  if (movable[0])
    speed -= spin(movable[0]) * axis;
  return speed;
}

Vector3 ownSpin(const RigidBody *body) { return body->getRotation(); }

// Applies `twist` (an angular impulse) to body two and the opposite to body
// one, changing the velocities `applyTo` changes, as applyAcrossJoint does
template <typename ApplyTo>
void twistAcrossJoint(RigidBody *const movable[2], const Vector3 &twist, ApplyTo applyTo) {
  for (int end = 0; end < 2; end++)
    if (movable[end])
      applyTo(
          movable[end], Vector3(),
          movable[end]->getInverseInertiaTensorWorld().transform(end == 1 ? twist : twist * -1));
}
} // namespace

void ContactResolver::setUpJoints(std::span<Joint *const> joints, real duration) {
  solverJoints.clear();
  for (Joint *joint : joints) {
    SolverJoint solverJoint{.joint = joint,
                            .movable = {movableBody(joint->body[0]), movableBody(joint->body[1])}};
    if (!solverJoint.movable[0] && !solverJoint.movable[1])
      continue;
    for (RigidBody *body : solverJoint.movable)
      if (body)
        rememberStart(body);

    Vector3 anchor[2];
    for (int end = 0; end < 2; end++) {
      anchor[end] = joint->body[end]->getPointInWorldSpace(joint->position[end]);
      solverJoint.offset[end] = anchor[end] - joint->body[end]->getPosition();
    }
    // Anchor two's velocity relative to anchor one's when integrate() moved
    // the bodies: from the velocities kept then, since joints set up before
    // this one have already changed them with their warm starts
    const auto startVelocity = [this](RigidBody *body, const Vector3 &offset) {
      if (!body)
        return Vector3();
      const Motion &start = slotOf(body).start;
      return pointVelocity(offset, start.linear, start.angular);
    };
    const Vector3 relativeVelocity = startVelocity(solverJoint.movable[1], solverJoint.offset[1]) -
                                     startVelocity(solverJoint.movable[0], solverJoint.offset[0]);
    solverJoint.separation = anchor[1] - anchor[0];
    solverJoint.startRelativeVelocity = relativeVelocity;
    solverJoint.cable = joint->error > 0;

    Vector3 warmStart;
    if (solverJoint.cable) {
      // Along the line between the anchors. Anchors on top of each other
      // can't be stretched this step, so there's nothing to do.
      solverJoint.length = solverJoint.separation.magnitude();
      if (solverJoint.length <= (real)1e-9) {
        joint->impulse = Vector3();
        continue;
      }
      solverJoint.direction = solverJoint.separation * ((real)1 / solverJoint.length);
      solverJoint.response =
          responseAlong(solverJoint.movable[0], solverJoint.offset[0], solverJoint.direction) +
          responseAlong(solverJoint.movable[1], solverJoint.offset[1], solverJoint.direction);
      solverJoint.startSpeed = relativeVelocity * solverJoint.direction;
      // As for a contact: the anchors may move apart up to the slack this
      // step (from where they were before integrate()), but no further
      const real lengthAtStart = solverJoint.length - solverJoint.startSpeed * duration;
      solverJoint.allowedSpeed = std::max((joint->error - lengthAtStart) / duration, (real)0);
      solverJoint.cableImpulse = std::max(-(joint->impulse * solverJoint.direction), (real)0);
      warmStart = solverJoint.direction * -solverJoint.cableImpulse;
    } else {
      Matrix3 response = pointResponse(solverJoint.movable[0], solverJoint.offset[0]);
      response += pointResponse(solverJoint.movable[1], solverJoint.offset[1]);
      solverJoint.effectiveMass = response.inverse();
      warmStart = joint->impulse;
    }
    applyAcrossJoint(solverJoint.movable, solverJoint.offset, warmStart, changeVelocity);
    solverJoints.push_back(solverJoint);
  }
  setUpAngularConstraints(joints, duration);
}

void ContactResolver::setUpAngularConstraints(std::span<Joint *const> joints, real duration) {
  using Kind = AngularConstraint::Kind;
  solverAngularConstraints.clear();
  AngularConstraint constraints[Joint::maxAngularConstraints];
  for (Joint *joint : joints) {
    RigidBody *const movable[2] = {movableBody(joint->body[0]), movableBody(joint->body[1])};
    if (!movable[0] && !movable[1])
      continue;
    const unsigned count = joint->getAngularConstraints(constraints);
    for (unsigned i = 0; i < count; i++) {
      SolverAngularConstraint solver{.joint = joint,
                                     .index = i,
                                     .constraint = constraints[i],
                                     .movable = {movable[0], movable[1]}};
      const Vector3 &axis = solver.constraint.axis;
      solver.response = turnResponse(movable[0], axis) + turnResponse(movable[1], axis);
      if (solver.response <= 0)
        continue;
      // (From the velocities integrate() moved them by: the anchors' warm
      // start has already changed them)
      solver.startSpeed = relativeTurning(
          movable, axis, [this](RigidBody *body) { return slotOf(body).start.angular; });
      switch (solver.constraint.kind) {
      case Kind::hold:
        // No turning about the axis, twisting either way
        solver.targetSpeed = 0;
        solver.minImpulse = -REAL_MAX;
        solver.maxImpulse = REAL_MAX;
        break;
      case Kind::limit: {
        // As for a contact: the bodies may turn up to the room the limit
        // left before integrate() moved them, but no further, and the
        // limit only pushes
        const real roomAtStart = solver.constraint.angle - solver.startSpeed * duration;
        solver.targetSpeed = -std::max(roomAtStart, (real)0) / duration;
        solver.minImpulse = 0;
        solver.maxImpulse = REAL_MAX;
        break;
      }
      case Kind::motor:
        solver.targetSpeed = solver.constraint.speed;
        solver.maxImpulse = solver.constraint.maxTorque * duration;
        solver.minImpulse = -solver.maxImpulse;
        break;
      }
      // Warm start, as far as last step's twist still fits
      solver.impulse = std::clamp(joint->angularImpulse[i], solver.minImpulse, solver.maxImpulse);
      twistAcrossJoint(movable, axis * solver.impulse, changeVelocity);
      solverAngularConstraints.push_back(solver);
    }
  }
}

void ContactResolver::sweepJoints() {
  for (SolverJoint &solverJoint : solverJoints) {
    if (solverJoint.cable) {
      // Pull, never push, until the anchors separate no faster than allowed
      const Vector3 relativeVelocityNow =
          anchorVelocity(solverJoint.movable[1], solverJoint.offset[1]) -
          anchorVelocity(solverJoint.movable[0], solverJoint.offset[0]);
      const real speed = relativeVelocityNow * solverJoint.direction;
      const real impulse = std::max(solverJoint.cableImpulse +
                                        (speed - solverJoint.allowedSpeed) / solverJoint.response,
                                    (real)0);
      applyAcrossJoint(solverJoint.movable, solverJoint.offset,
                       solverJoint.direction * -(impulse - solverJoint.cableImpulse),
                       changeVelocity);
      solverJoint.cableImpulse = impulse;
    } else {
      // Stop the anchors moving apart in any direction, pushing or pulling
      const Vector3 relativeVelocityNow =
          anchorVelocity(solverJoint.movable[1], solverJoint.offset[1]) -
          anchorVelocity(solverJoint.movable[0], solverJoint.offset[0]);
      const Vector3 impulse = solverJoint.effectiveMass.transform(relativeVelocityNow * -1);
      applyAcrossJoint(solverJoint.movable, solverJoint.offset, impulse, changeVelocity);
      solverJoint.joint->impulse += impulse;
    }
  }

  // Each angular constraint: reach its target turning speed (at least, for
  // a limit), keeping its total twist within bounds
  for (SolverAngularConstraint &solver : solverAngularConstraints) {
    const Vector3 &axis = solver.constraint.axis;
    const real speed = relativeTurning(solver.movable, axis, ownSpin);
    const real impulse = std::clamp(solver.impulse + (solver.targetSpeed - speed) / solver.response,
                                    solver.minImpulse, solver.maxImpulse);
    twistAcrossJoint(solver.movable, axis * (impulse - solver.impulse), changeVelocity);
    solver.impulse = impulse;
  }
}

void ContactResolver::settleJoints(real duration) {
  // As the settle sweeps do for contacts: a cable the move has pulled taut
  // ends the step no longer stretching
  for (SolverJoint &solverJoint : solverJoints) {
    if (!solverJoint.cable)
      continue;
    const Vector3 relativeVelocity = anchorVelocity(solverJoint.movable[1], solverJoint.offset[1]) -
                                     anchorVelocity(solverJoint.movable[0], solverJoint.offset[0]);
    const real lengthNow =
        solverJoint.length +
        (relativeVelocity * solverJoint.direction - solverJoint.startSpeed) * duration;
    solverJoint.allowedSpeed = std::max((solverJoint.joint->error - lengthNow) / duration, (real)0);
  }

  // And a limit the move has just brought the bodies to ends the step no
  // longer turning into it; one with room left may close it
  for (SolverAngularConstraint &solver : solverAngularConstraints) {
    if (solver.constraint.kind != AngularConstraint::Kind::limit)
      continue;
    const real roomNow =
        solver.constraint.angle +
        (relativeTurning(solver.movable, solver.constraint.axis, ownSpin) - solver.startSpeed) *
            duration;
    solver.targetSpeed = roomNow > 0 ? -roomNow / duration : (real)0;
  }
}

void ContactResolver::setJointPositionTargets(real duration) {
  // How far apart the anchors are now the bodies have moved, measured
  // where they are (as Box2D's position step does: a spinning body's
  // anchor moves on an arc, which the velocities alone would miss), and how
  // much of that to undo this step: all of it (up to maxCorrection), back
  // together for a ball and socket, back within the slack for a cable
  for (SolverJoint &solverJoint : solverJoints) {
    for (RigidBody *body : solverJoint.movable)
      if (body)
        slotOf(body).separates = true;
    solverJoint.positionImpulse = Vector3();
    solverJoint.positionImpulseAlong = 0;
    const Joint &joint = *solverJoint.joint;
    Vector3 anchor[2];
    for (int end = 0; end < 2; end++) {
      anchor[end] = pointNow(joint.body[end], joint.position[end]);
      solverJoint.offset[end] = anchor[end] - joint.body[end]->getPosition();
    }
    const Vector3 separationNow = anchor[1] - anchor[0];
    if (solverJoint.cable) {
      const real length = separationNow.magnitude();
      solverJoint.targetSpeed = 0;
      if (length <= (real)1e-9)
        continue;
      solverJoint.direction = separationNow * ((real)1 / length);
      solverJoint.response =
          responseAlong(solverJoint.movable[0], solverJoint.offset[0], solverJoint.direction) +
          responseAlong(solverJoint.movable[1], solverJoint.offset[1], solverJoint.direction);
      solverJoint.targetSpeed =
          -std::clamp(length - joint.error, (real)0, maxCorrection) / duration;
    } else {
      Matrix3 response = pointResponse(solverJoint.movable[0], solverJoint.offset[0]);
      response += pointResponse(solverJoint.movable[1], solverJoint.offset[1]);
      solverJoint.effectiveMass = response.inverse();
      Vector3 correction = separationNow;
      if (correction.magnitude() > maxCorrection)
        correction = correction * (maxCorrection / correction.magnitude());
      solverJoint.targetVelocity = correction * ((real)-1 / duration);
    }
  }

  // The angular constraints, likewise measured as the bodies are now: a
  // hold turns back all its bodies have turned off (up to
  // maxAngularCorrection), a limit all they've gone beyond it past
  // angularSlop; a motor has nothing to correct
  using Kind = AngularConstraint::Kind;
  AngularConstraint constraintsNow[Joint::maxAngularConstraints];
  const Joint *measured = nullptr;
  unsigned countNow = 0;
  for (SolverAngularConstraint &solver : solverAngularConstraints) {
    if (solver.joint != measured) {
      measured = solver.joint;
      countNow = solver.joint->getAngularConstraints(constraintsNow);
    }
    solver.positionImpulse = 0;
    solver.positionTargetSpeed = 0;
    if (solver.index >= countNow || solver.constraint.kind == Kind::motor)
      continue;
    const AngularConstraint &now = constraintsNow[solver.index];
    solver.constraint.axis = now.axis;
    solver.response =
        turnResponse(solver.movable[0], now.axis) + turnResponse(solver.movable[1], now.axis);
    const real correction =
        solver.constraint.kind == Kind::hold
            ? -std::clamp(now.angle, -maxAngularCorrection, maxAngularCorrection)
            : std::min(std::max(-now.angle - angularSlop, (real)0), maxAngularCorrection);
    solver.positionTargetSpeed = correction / duration;
  }
}

void ContactResolver::sweepJointPositions() {
  auto moveVelocity = [this](RigidBody *body, const Vector3 &linear, const Vector3 &angular) {
    Motion &motion = slotOf(body).separating;
    motion.linear += linear;
    motion.angular += angular;
  };
  for (SolverJoint &solverJoint : solverJoints) {
    Vector3 relativeVelocity;
    for (int end = 0; end < 2; end++)
      if (RigidBody *body = solverJoint.movable[end]) {
        const Motion &motion = slotOf(body).separating;
        const Vector3 velocity =
            pointVelocity(solverJoint.offset[end], motion.linear, motion.angular);
        relativeVelocity += end == 1 ? velocity : velocity * -1;
      }
    if (solverJoint.cable) {
      const real speed = relativeVelocity * solverJoint.direction;
      const real impulse = std::max(solverJoint.positionImpulseAlong +
                                        (speed - solverJoint.targetSpeed) / solverJoint.response,
                                    (real)0);
      applyAcrossJoint(solverJoint.movable, solverJoint.offset,
                       solverJoint.direction * -(impulse - solverJoint.positionImpulseAlong),
                       moveVelocity);
      solverJoint.positionImpulseAlong = impulse;
    } else {
      const Vector3 impulse =
          solverJoint.effectiveMass.transform(solverJoint.targetVelocity - relativeVelocity);
      applyAcrossJoint(solverJoint.movable, solverJoint.offset, impulse, moveVelocity);
    }
  }

  // Holds keep the axes in line (turning either way); limits only turn the
  // bodies back from beyond them
  const auto moveSpin = [this](RigidBody *body) { return slotOf(body).separating.angular; };
  for (SolverAngularConstraint &solver : solverAngularConstraints) {
    const bool hold = solver.constraint.kind == AngularConstraint::Kind::hold;
    if (!hold && solver.positionTargetSpeed <= 0)
      continue;
    const Vector3 &axis = solver.constraint.axis;
    const real speed = relativeTurning(solver.movable, axis, moveSpin);
    real impulse = solver.positionImpulse + (solver.positionTargetSpeed - speed) / solver.response;
    if (!hold)
      impulse = std::max(impulse, (real)0);
    twistAcrossJoint(solver.movable, axis * (impulse - solver.positionImpulse), moveVelocity);
    solver.positionImpulse = impulse;
  }
}
