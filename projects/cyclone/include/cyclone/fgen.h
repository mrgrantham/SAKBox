/*
 * Interface file for the force generators.
 *
 * Part of the Cyclone physics system.
 *
 * Copyright (c) Icosagon 2003. All Rights Reserved.
 *
 * This software is distributed under license. Use of this software
 * implies agreement with all terms and conditions of the accompanying
 * software license.
 */

/**
 * @file
 *
 * This file contains the interface and sample force generators.
 */
#ifndef CYCLONE_FGEN_H
#define CYCLONE_FGEN_H

#include "body.h"
#include "pfgen.h"
#include "random.h"
#include <unordered_map>
#include <vector>

namespace cyclone {

/**
 * A force generator can be asked to add a force to one or more
 * bodies.
 */
class ForceGenerator {
public:
  /**
   * SAKBox: a virtual destructor, so a force generator deleted through this
   * interface is destroyed in full.
   */
  virtual ~ForceGenerator() = default;

  /**
   * Overload this in implementations of the interface to calculate
   * and update the force applied to the given rigid body.
   */
  virtual void updateForce(RigidBody *body, real duration) = 0;
};

/**
 * A force generator that applies a gravitational force. One instance
 * can be used for multiple rigid bodies.
 */
class Gravity : public ForceGenerator {
  /** Holds the acceleration due to gravity. */
  Vector3 gravity;

public:
  /** Creates the generator with the given acceleration. */
  Gravity(const Vector3 &gravity);

  /** Applies the gravitational force to the given rigid body. */
  void updateForce(RigidBody *body, real duration) override;
};

/**
 * A force generator that applies a Spring force.
 */
class Spring : public ForceGenerator {
  /**
   * The point of connection of the spring, in local
   * coordinates.
   */
  Vector3 connectionPoint;

  /**
   * The point of connection of the spring to the other object,
   * in that object's local coordinates.
   */
  Vector3 otherConnectionPoint;

  /** The particle at the other end of the spring. */
  RigidBody *other;

  /** Holds the sprint constant. */
  real springConstant;

  /** Holds the rest length of the spring. */
  real restLength;

public:
  /** Creates a new spring with the given parameters. */
  Spring(const Vector3 &localConnectionPt, RigidBody *other, const Vector3 &otherConnectionPt,
         real springConstant, real restLength);

  /** Applies the spring force to the given rigid body. */
  void updateForce(RigidBody *body, real duration) override;
};

/**
 * A force generator showing a three component explosion effect.
 * This force generator is intended to represent a single
 * explosion effect for multiple rigid bodies. The force generator
 * can also act as a particle force generator.
 *
 * SAKBox: upstream declared this (from the book, section 17.2.3) but never
 * wrote it. The three stages follow the book:
 *
 * - Implosion: for implosionDuration, objects between implosionMinRadius
 *   and implosionMaxRadius are pulled towards the detonation with
 *   implosionForce.
 * - Concussion: a shock wave, shockwaveThickness thick, travels out at
 *   shockwaveSpeed for concussionDuration. It pushes objects outwards with
 *   the peak force inside it, falling linearly to nothing one thickness
 *   either side. The peak is peakConcussionForce for a still object, less
 *   for one already moving outwards (nothing at the wave's speed) and more
 *   for one moving inwards, and fades to nothing over concussionDuration.
 *   Rigid bodies are pushed at a point off their centre so they spin.
 * - Convection: for convectionDuration, objects in a cylinder
 *   chimneyRadius wide and chimneyHeight tall above the detonation are
 *   pushed along the line from the detonation (up, and out a little, which
 *   makes the mushroom shape), with peakConvectionForce at the chimney's
 *   axis, falling linearly to nothing at its edge.
 *
 * Two of the book's formulas are corrected: as printed, the force ahead of
 * the shock wave and the chimney's force both rise towards the outside,
 * though the text and figure 17.7 describe them falling off.
 *
 * Time is moved on with advanceTime(), once per simulation step, not by
 * updateForce, which is called once per object.
 */
class Explosion : public ForceGenerator, public ParticleForceGenerator {
  /**
   * Tracks how long the explosion has been in operation, used
   * for time-sensitive effects.
   */
  real timePassed;

  /**
   * Picks each rigid body's off-centre point for the concussion wave.
   */
  Random random;

  /**
   * The point (in body coordinates) at which the concussion wave pushes
   * each rigid body, picked the first time the wave reaches it and kept
   * after, so the body turns until that point lines up with the push and
   * then stops, rather than spinning faster and faster.
   */
  std::unordered_map<const RigidBody *, Vector3> concussionPoints;

public:
  // Properties of the explosion, these are public because
  // there are so many and providing a suitable constructor
  // would be cumbersome:

  /**
   * The location of the detonation of the weapon.
   */
  Vector3 detonation;

  /**
   * The radius up to which objects implode in the first stage
   * of the explosion.
   */
  real implosionMaxRadius;

  /**
   * The radius within which objects don't feel the implosion
   * force. Objects near to the detonation aren't sucked in by
   * the air implosion.
   */
  real implosionMinRadius;

  /**
   * The length of time that objects spend imploding before the
   * concussion phase kicks in.
   */
  real implosionDuration;

  /**
   * The maximal force that the implosion can apply. This should
   * be relatively small to avoid the implosion pulling objects
   * through the detonation point and out the other side before
   * the concussion wave kicks in.
   */
  real implosionForce;

  /**
   * The speed that the shock wave is traveling, this is related
   * to the thickness below in the relationship:
   *
   * thickness >= speed * minimum frame duration
   */
  real shockwaveSpeed;

  /**
   * The shock wave applies its force over a range of distances,
   * this controls how thick. Faster waves require larger
   * thicknesses.
   */
  real shockwaveThickness;

  /**
   * This is the force that is applied at the very centre of the
   * concussion wave on an object that is stationary. Objects
   * that are in front or behind of the wavefront, or that are
   * already moving outwards, get proportionally less
   * force. Objects moving in towards the centre get
   * proportionally more force.
   */
  real peakConcussionForce;

  /**
   * The length of time that the concussion wave is active.
   * As the wave nears this, the forces it applies reduces.
   */
  real concussionDuration;

  /**
   * How far from a rigid body's centre the concussion wave pushes it,
   * at most, so that it spins as it is thrown. Zero pushes every body
   * through its centre.
   */
  real concussionOffset;

  /**
   * This is the peak force for stationary objects in
   * the centre of the convection chimney.
   */
  real peakConvectionForce;

  /**
   * The radius of the chimney cylinder in the xz plane.
   */
  real chimneyRadius;

  /**
   * The maximum height of the chimney.
   */
  real chimneyHeight;

  /**
   * The length of time the convection chimney is active. Typically
   * this is the longest effect to be in operation, as the heat
   * from the explosion outlives the shock wave and implosion
   * itself.
   */
  real convectionDuration;

public:
  /**
   * Creates a new explosion with sensible default values, at the origin,
   * for objects of about a metre and a few kilograms.
   */
  Explosion();

  /**
   * Moves the explosion on by the given time. Call it once per
   * simulation step.
   */
  void advanceTime(real_duration duration);

  /** Returns how long the explosion has been going. */
  real getTimePassed() const;

  /**
   * Starts the explosion again from detonation.
   */
  void restart();

  /**
   * Returns true while any stage of the explosion is still applying
   * forces.
   */
  bool isActive() const;

  /**
   * Calculates and applies the force that the explosion
   * has on the given rigid body.
   */
  void updateForce(RigidBody *body, real duration) override;

  /**
   * Calculates and applies the force that the explosion has
   * on the given particle.
   */
  void updateForce(Particle *particle, real duration) override;

private:
  /** The implosion's pull on an object at the given position. */
  Vector3 implosionAt(const Vector3 &position) const;

  /** The shock wave's push on an object at the given position and velocity. */
  Vector3 concussionAt(const Vector3 &position, const Vector3 &velocity) const;

  /** The convection chimney's push on an object at the given position. */
  Vector3 convectionAt(const Vector3 &position) const;
};

/**
 * A force generator that applies an aerodynamic force.
 */
class Aero : public ForceGenerator {
protected:
  /**
   * Holds the aerodynamic tensor for the surface in body
   * space.
   */
  Matrix3 tensor;

  /**
   * Holds the relative position of the aerodynamic surface in
   * body coordinates.
   */
  Vector3 position;

  /**
   * Holds a pointer to a vector containing the windspeed of the
   * environment. This is easier than managing a separate
   * windspeed vector per generator and having to update it
   * manually as the wind changes.
   */
  const Vector3 *windspeed;

public:
  /**
   * Creates a new aerodynamic force generator with the
   * given properties.
   */
  Aero(const Matrix3 &tensor, const Vector3 &position, const Vector3 *windspeed);

  /**
   * Applies the force to the given rigid body.
   */
  void updateForce(RigidBody *body, real duration) override;

protected:
  /**
   * Uses an explicit tensor matrix to update the force on
   * the given rigid body. This is exactly the same as for updateForce
   * only it takes an explicit tensor.
   */
  void updateForceFromTensor(RigidBody *body, real duration, const Matrix3 &tensor);
};

/**
 * A force generator with a control aerodynamic surface. This
 * requires three inertia tensors, for the two extremes and
 * 'resting' position of the control surface.  The latter tensor is
 * the one inherited from the base class, the two extremes are
 * defined in this class.
 */
class AeroControl : public Aero {
protected:
  /**
   * The aerodynamic tensor for the surface, when the control is at
   * its maximum value.
   */
  Matrix3 maxTensor;

  /**
   * The aerodynamic tensor for the surface, when the control is at
   * its minimum value.
   */
  Matrix3 minTensor;

  /**
   * The current position of the control for this surface. This
   * should range between -1 (in which case the minTensor value
   * is used), through 0 (where the base-class tensor value is
   * used) to +1 (where the maxTensor value is used).
   */
  real controlSetting;

private:
  /**
   * Calculates the final aerodynamic tensor for the current
   * control setting.
   */
  Matrix3 getTensor();

public:
  /**
   * Creates a new aerodynamic control surface with the given
   * properties.
   */
  AeroControl(const Matrix3 &base, const Matrix3 &min, const Matrix3 &max, const Vector3 &position,
              const Vector3 *windspeed);

  /**
   * Sets the control position of this control. This * should
  range between -1 (in which case the minTensor value is *
  used), through 0 (where the base-class tensor value is used) *
  to +1 (where the maxTensor value is used). Values outside that
  * range give undefined results.
  */
  void setControl(real value);

  /**
   * Applies the force to the given rigid body.
   */
  void updateForce(RigidBody *body, real duration) override;
};

/**
 * A force generator to apply a buoyant force to a rigid body.
 */
class Buoyancy : public ForceGenerator {
  /**
   * The maximum submersion depth of the object before
   * it generates its maximum buoyancy force.
   */
  real maxDepth;

  /**
   * The volume of the object.
   */
  real volume;

  /**
   * The height of the water plane above y=0. The plane will be
   * parallel to the XZ plane.
   */
  real waterHeight;

  /**
   * The density of the liquid. Pure water has a density of
   * 1000kg per cubic meter.
   */
  real liquidDensity;

  /**
   * The centre of buoyancy of the rigid body, in body coordinates.
   */
  Vector3 centreOfBuoyancy;

public:
  /** Creates a new buoyancy force with the given parameters. */
  Buoyancy(const Vector3 &cOfB, real maxDepth, real volume, real waterHeight,
           real liquidDensity = 1000.0f);

  /**
   * Applies the force to the given rigid body.
   */
  void updateForce(RigidBody *body, real duration) override;
};

/**
 * Holds all the force generators and the bodies they apply to.
 */
class ForceRegistry {
protected:
  /**
   * Keeps track of one force generator and the body it
   * applies to.
   */
  struct ForceRegistration {
    RigidBody *body;
    ForceGenerator *fg;
  };

  /**
   * Holds the list of registrations.
   */
  typedef std::vector<ForceRegistration> Registry;
  Registry registrations;

public:
  /**
   * Registers the given force generator to apply to the
   * given body.
   */
  void add(RigidBody *body, ForceGenerator *fg);

  /**
   * Removes the given registered pair from the registry.
   * If the pair is not registered, this method will have
   * no effect.
   */
  void remove(RigidBody *body, ForceGenerator *fg);

  /** SAKBox: removes every registration of `body`. */
  void remove(const RigidBody *body);

  /**
   * Clears all registrations from the registry. This will
   * not delete the bodies or the force generators
   * themselves, just the records of their connection.
   */
  void clear();

  /**
   * Calls all the force generators to update the forces of
   * their corresponding bodies.
   */
  void updateForces(real duration);
};
} // namespace cyclone

#endif // CYCLONE_FGEN_H