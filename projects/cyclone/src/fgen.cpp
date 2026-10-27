/*
 * Implementation file for the rigid body force generators.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under license. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software license.
 */

#include <cyclone/fgen.h>

using namespace cyclone;

void ForceRegistry::updateForces(real duration) {
  Registry::iterator i = registrations.begin();
  for (; i != registrations.end(); i++) {
    i->fg->updateForce(i->body, duration);
  }
}

void ForceRegistry::add(RigidBody *body, ForceGenerator *fg) {
  ForceRegistry::ForceRegistration registration;
  registration.body = body;
  registration.fg = fg;
  registrations.push_back(registration);
}

// SAKBox: remove and clear were declared but never written.
void ForceRegistry::remove(RigidBody *body, ForceGenerator *fg) {
  std::erase_if(registrations, [&](const ForceRegistration &registration) {
    return registration.body == body && registration.fg == fg;
  });
}

void ForceRegistry::remove(const RigidBody *body) {
  std::erase_if(registrations,
                [&](const ForceRegistration &registration) { return registration.body == body; });
}

void ForceRegistry::clear() { registrations.clear(); }

Buoyancy::Buoyancy(const Vector3 &cOfB, real maxDepth, real volume, real waterHeight,
                   real liquidDensity /* = 1000.0f */) {
  centreOfBuoyancy = cOfB;
  Buoyancy::liquidDensity = liquidDensity;
  Buoyancy::maxDepth = maxDepth;
  Buoyancy::volume = volume;
  Buoyancy::waterHeight = waterHeight;
}

void Buoyancy::updateForce(RigidBody *body, real duration) {
  // Calculate the submersion depth
  Vector3 pointInWorld = body->getPointInWorldSpace(centreOfBuoyancy);
  real depth = pointInWorld.y;

  // Check if we're out of the water
  if (depth >= waterHeight + maxDepth)
    return;
  Vector3 force(0, 0, 0);

  // Check if we're at maximum depth
  if (depth <= waterHeight - maxDepth) {
    force.y = liquidDensity * volume;
    body->addForceAtBodyPoint(force, centreOfBuoyancy);
    return;
  }

  // Otherwise we are partly submerged
  // SAKBox: the submerged fraction runs from 0 at the top of the band to 1
  // at the bottom; this was (depth - maxDepth - waterHeight) / 2 * maxDepth,
  // negative throughout (a partly submerged body was pushed down).
  force.y = liquidDensity * volume * (waterHeight + maxDepth - depth) / (2 * maxDepth);
  body->addForceAtBodyPoint(force, centreOfBuoyancy);
}

Gravity::Gravity(const Vector3 &gravity) : gravity(gravity) {}

void Gravity::updateForce(RigidBody *body, real duration) {
  // Check that we do not have infinite mass
  if (!body->hasFiniteMass())
    return;

  // SAKBox: gravity is always there, so it doesn't wake a sleeping body
  // (adding a force does); a sleeping body isn't moved, so needs none
  if (!body->getAwake())
    return;

  // Apply the mass-scaled force to the body
  body->addForce(gravity * body->getMass());
}

Spring::Spring(const Vector3 &localConnectionPt, RigidBody *other, const Vector3 &otherConnectionPt,
               real springConstant, real restLength)
    : connectionPoint(localConnectionPt), otherConnectionPoint(otherConnectionPt), other(other),
      springConstant(springConstant), restLength(restLength) {}

void Spring::updateForce(RigidBody *body, real duration) {
  // Calculate the two ends in world space
  Vector3 lws = body->getPointInWorldSpace(connectionPoint);
  Vector3 ows = other->getPointInWorldSpace(otherConnectionPoint);

  // Calculate the vector of the spring
  Vector3 force = lws - ows;

  // Calculate the magnitude of the force
  // SAKBox: signed, so a squashed spring pushes (real_abs made it always pull)
  real magnitude = force.magnitude();
  magnitude = (magnitude - restLength) * springConstant;

  // Calculate the final force and apply it
  force.normalise();
  force *= -magnitude;
  body->addForceAtPoint(force, lws);
}

Aero::Aero(const Matrix3 &tensor, const Vector3 &position, const Vector3 *windspeed) {
  Aero::tensor = tensor;
  Aero::position = position;
  Aero::windspeed = windspeed;
}

void Aero::updateForce(RigidBody *body, real duration) {
  Aero::updateForceFromTensor(body, duration, tensor);
}

void Aero::updateForceFromTensor(RigidBody *body, real duration, const Matrix3 &tensor) {
  // Calculate the velocity of the body through the air.
  // SAKBox: the wind is subtracted; upstream added it, so the air seemed to
  // flow the wrong way and a still body was pushed into the wind.
  Vector3 velocity = body->getVelocity();
  velocity -= *windspeed;

  // Calculate the velocity in body coordinates
  Vector3 bodyVel = body->getTransform().transformInverseDirection(velocity);

  // Calculate the force in body coordinates
  Vector3 bodyForce = tensor.transform(bodyVel);
  Vector3 force = body->getTransform().transformDirection(bodyForce);

  // Apply the force
  body->addForceAtBodyPoint(force, position);
}

AeroControl::AeroControl(const Matrix3 &base, const Matrix3 &min, const Matrix3 &max,
                         const Vector3 &position, const Vector3 *windspeed)
    : Aero(base, position, windspeed) {
  AeroControl::minTensor = min;
  AeroControl::maxTensor = max;
  controlSetting = 0.0f;
}

Matrix3 AeroControl::getTensor() {
  if (controlSetting <= -1.0f)
    return minTensor;
  else if (controlSetting >= 1.0f)
    return maxTensor;
  else if (controlSetting < 0) {
    return Matrix3::linearInterpolate(minTensor, tensor, controlSetting + 1.0f);
  } else if (controlSetting > 0) {
    return Matrix3::linearInterpolate(tensor, maxTensor, controlSetting);
  } else
    return tensor;
}

void AeroControl::setControl(real value) { controlSetting = value; }

void AeroControl::updateForce(RigidBody *body, real duration) {
  Matrix3 tensor = getTensor();
  Aero::updateForceFromTensor(body, duration, tensor);
}

// SAKBox: Explosion, from the book's description (see fgen.h).
Explosion::Explosion()
    : timePassed(0), random(1), detonation(0, 0, 0), implosionMaxRadius(4), implosionMinRadius(1),
      implosionDuration(0.1), implosionForce(20), shockwaveSpeed(15), shockwaveThickness(1),
      peakConcussionForce(300), concussionDuration(1), concussionOffset(0.3),
      peakConvectionForce(5), chimneyRadius(2), chimneyHeight(10), convectionDuration(4) {}

void Explosion::advanceTime(real_duration duration) { timePassed += duration; }

real Explosion::getTimePassed() const { return timePassed; }

void Explosion::restart() {
  timePassed = 0;
  concussionPoints.clear();
}

bool Explosion::isActive() const {
  return timePassed < implosionDuration || timePassed < concussionDuration ||
         timePassed < convectionDuration;
}

Vector3 Explosion::implosionAt(const Vector3 &position) const {
  if (timePassed >= implosionDuration)
    return Vector3();
  Vector3 inwards = detonation - position;
  real distance = inwards.magnitude();
  if (distance < implosionMinRadius || distance > implosionMaxRadius)
    return Vector3();
  return inwards * (implosionForce / distance);
}

Vector3 Explosion::concussionAt(const Vector3 &position, const Vector3 &velocity) const {
  if (timePassed >= concussionDuration || shockwaveSpeed <= 0 || shockwaveThickness <= 0)
    return Vector3();
  Vector3 outwards = position - detonation;
  real distance = outwards.magnitude();
  if (distance == 0)
    return Vector3();
  outwards *= 1 / distance;

  // The wave's back edge is at back, its front at back + thickness, and its
  // force falls off over one thickness either side. (The book's formula has
  // the force ahead of the wave rising to the peak at the outer edge.)
  const real back = shockwaveSpeed * timePassed, thickness = shockwaveThickness;
  real share;
  if (distance < back - thickness || distance >= back + 2 * thickness)
    return Vector3();
  else if (distance < back)
    share = 1 - (back - distance) / thickness;
  else if (distance < back + thickness)
    share = 1;
  else
    share = 1 - (distance - back - thickness) / thickness;

  // Moving air: less push for an object already moving outwards, none for
  // one keeping up with the wave, more for one moving inwards
  real peak = peakConcussionForce * (1 - (velocity * outwards) / shockwaveSpeed);
  if (peak <= 0)
    return Vector3();
  real fade = 1 - timePassed / concussionDuration;
  return outwards * (peak * share * fade);
}

Vector3 Explosion::convectionAt(const Vector3 &position) const {
  if (timePassed >= convectionDuration || chimneyRadius <= 0)
    return Vector3();
  Vector3 outwards = position - detonation;
  real across = real_sqrt(outwards.x * outwards.x + outwards.z * outwards.z);
  if (outwards.y < 0 || outwards.y >= chimneyHeight || across >= chimneyRadius)
    return Vector3();
  if (outwards.squareMagnitude() == 0)
    outwards = Vector3(0, 1, 0);
  outwards.normalise();

  // Strongest up the middle of the chimney, nothing at its wall. (The
  // book's formula has it the other way round, contradicting its own
  // description of peakConvectionForce.)
  return outwards * (peakConvectionForce * (1 - across / chimneyRadius));
}

void Explosion::updateForce(RigidBody *body, real duration) {
  if (!body->hasFiniteMass())
    return;
  const Vector3 position = body->getPosition();
  body->addForce(implosionAt(position) + convectionAt(position));

  // The concussion pushes off-centre, at the same point of the body each
  // frame, so the body turns as it's thrown
  Vector3 push = concussionAt(position, body->getVelocity());
  if (push.squareMagnitude() == 0)
    return;
  auto point = concussionPoints.find(body);
  if (point == concussionPoints.end())
    point = concussionPoints.emplace(body, random.randomVector(concussionOffset)).first;
  body->addForceAtBodyPoint(push, point->second);
}

void Explosion::updateForce(Particle *particle, real duration) {
  if (!particle->hasFiniteMass())
    return;
  const Vector3 position = particle->getPosition();
  particle->addForce(implosionAt(position) + concussionAt(position, particle->getVelocity()) +
                     convectionAt(position));
}
