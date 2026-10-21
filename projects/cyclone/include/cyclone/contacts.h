/*
 * Interface file for the contact resolution system.
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
 * This file contains the contact resolution system for cyclone,
 * although it is called the contact resolution system, it handles
 * collisions, contacts (sliding and resting), and constraints (such
 * as joints).
 *
 * The resolver uses an iterative satisfaction algorithm; it loops
 * through each contact and tries to resolve it. This is a very fast
 * algorithm but can be unstable when the contacts are highly
 * inter-related.
 */
#ifndef CYCLONE_CONTACTS_H
#define CYCLONE_CONTACTS_H

#include "body.h"
#include <map>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cyclone {

/*
 * Forward declaration, see full declaration below for complete
 * documentation.
 */
class ContactResolver;
class Joint;

/**
 * A contact represents two bodies in contact. Resolving a
 * contact removes their interpenetration, and applies sufficient
 * impulse to keep them apart. Colliding bodies may also rebound.
 * Contacts can be used to represent positional joints, by making
 * the contact constraint keep the bodies in their correct
 * orientation.
 *
 * It can be a good idea to create a contact object even when the
 * contact isn't violated. Because resolving one contact can violate
 * another, contacts that are close to being violated should be
 * sent to the resolver; that way if one resolution moves the body,
 * the contact may be violated, and can be resolved. If the contact
 * is not violated, it will not be resolved, so you only loose a
 * small amount of execution time.
 *
 * The contact has no callable functions, it just holds the contact
 * details. To resolve a set of contacts, use the contact resolver
 * class.
 */
class Contact {
  // ... Other data as before ...

  /**
   * The contact resolver object needs access into the contacts to
   * set and effect the contact.
   */
  friend class ContactResolver;

public:
  /**
   * Holds the bodies that are involved in the contact. The
   * second of these can be NULL, for contacts with the scenery.
   */
  RigidBody *body[2];

  /**
   * Holds the lateral friction coefficient at the contact.
   */
  real friction;

  /**
   * Holds the normal restitution coefficient at the contact.
   */
  real restitution;

  /**
   * Holds the position of the contact in world coordinates.
   */
  Vector3 contactPoint;

  /**
   * Holds the direction of the contact in world coordinates.
   */
  Vector3 contactNormal;

  /**
   * Holds the depth of penetration at the contact point. If both
   * bodies are specified then the contact point should be midway
   * between the inter-penetrating points.
   */
  real penetration;

  /**
   * Sets the data that doesn't normally depend on the position
   * of the contact (i.e. the bodies, and their material properties).
   */
  void setBodyData(RigidBody *one, RigidBody *two, real friction, real restitution);

protected:
  /**
   * A transform matrix that converts co-ordinates in the contact's
   * frame of reference to world co-ordinates. The columns of this
   * matrix form an orthonormal set of vectors.
   */
  Matrix3 contactToWorld;

  /**
   * Holds the closing velocity at the point of contact. This is set
   * when the calculateInternals function is run.
   */
  Vector3 contactVelocity;

  /**
   * Holds the required change in velocity for this contact to be
   * resolved.
   */
  real desiredDeltaVelocity;

  /**
   * Holds the world space position of the contact point relative to
   * centre of each body. This is set when the calculateInternals
   * function is run.
   */
  Vector3 relativeContactPosition[2];

protected:
  /**
   * Calculates internal data from state data. This is called before
   * the resolution algorithm tries to do any resolution. It should
   * never need to be called manually.
   */
  void calculateInternals(real duration);

  /**
   * Reverses the contact. This involves swapping the two rigid bodies
   * and reversing the contact normal. The internal values should then
   * be recalculated using calculateInternals (this is not done
   * automatically).
   */
  void swapBodies();

  /**
   * Updates the awake state of rigid bodies that are taking
   * place in the given contact. A body will be made awake if it
   * is in contact with a body that is awake.
   */
  void matchAwakeState();

  /**
   * Calculates and sets the internal value for the desired delta
   * velocity.
   */
  void calculateDesiredDeltaVelocity(real duration);

  /**
   * Calculates and returns the velocity of the contact
   * point on the given body.
   */
  Vector3 calculateLocalVelocity(unsigned bodyIndex, real duration);

  /**
   * Calculates an orthonormal basis for the contact point, based on
   * the primary friction direction (for anisotropic friction) or
   * a random orientation (for isotropic friction).
   */
  void calculateContactBasis();

  /**
   * Applies an impulse to the given body, returning the
   * change in velocities.
   */
  void applyImpulse(const Vector3 &impulse, RigidBody *body, Vector3 *velocityChange,
                    Vector3 *rotationChange);

  /**
   * Performs an inertia-weighted impulse based resolution of this
   * contact alone.
   */
  void applyVelocityChange(Vector3 velocityChange[2], Vector3 rotationChange[2]);

  /**
   * Performs an inertia weighted penetration resolution of this
   * contact alone.
   */
  void applyPositionChange(Vector3 linearChange[2], Vector3 angularChange[2], real penetration);

  /**
   * Calculates the impulse needed to resolve this contact,
   * given that the contact has no friction. A pair of inertia
   * tensors - one for each contact object - is specified to
   * save calculation time: the calling function has access to
   * these anyway.
   */
  Vector3 calculateFrictionlessImpulse(Matrix3 *inverseInertiaTensor);

  /**
   * Calculates the impulse needed to resolve this contact,
   * given that the contact has a non-zero coefficient of
   * friction. A pair of inertia tensors - one for each contact
   * object - is specified to save calculation time: the calling
   * function has access to these anyway.
   */
  Vector3 calculateFrictionImpulse(Matrix3 *inverseInertiaTensor);
};

/**
 * The contact resolution routine. One resolver instance
 * can be shared for the whole simulation, as long as you need
 * roughly the same parameters each time (which is normal).
 *
 * @section algorithm Resolution Algorithm
 *
 * The resolver uses an iterative satisfaction algorithm; it loops
 * through each contact and tries to resolve it. Each contact is
 * resolved locally, which may in turn put other contacts in a worse
 * position. The algorithm then revisits other contacts and repeats
 * the process up to a specified iteration limit. It can be proved
 * that given enough iterations, the simulation will get to the
 * correct result. As with all approaches, numerical stability can
 * cause problems that make a correct resolution impossible.
 *
 * @subsection strengths Strengths
 *
 * This algorithm is very fast, much faster than other physics
 * approaches. Even using many more iterations than there are
 * contacts, it will be faster than global approaches.
 *
 * Many global algorithms are unstable under high friction, this
 * approach is very robust indeed for high friction and low
 * restitution values.
 *
 * The algorithm produces visually believable behaviour. Tradeoffs
 * have been made to err on the side of visual realism rather than
 * computational expense or numerical accuracy.
 *
 * @subsection weaknesses Weaknesses
 *
 * The algorithm does not cope well with situations with many
 * inter-related contacts: stacked boxes, for example. In this
 * case the simulation may appear to jiggle slightly, which often
 * dislodges a box from the stack, allowing it to collapse.
 *
 * Another issue with the resolution mechanism is that resolving
 * one contact may make another contact move sideways against
 * friction, because each contact is handled independently, this
 * friction is not taken into account. If one object is pushing
 * against another, the pushed object may move across its support
 * without friction, even though friction is set between those bodies.
 *
 * In general this resolver is not suitable for stacks of bodies,
 * but is perfect for handling impact, explosive, and flat resting
 * situations.
 */
class ContactResolver {
protected:
  /**
   * Holds the number of iterations to perform when resolving
   * velocity.
   */
  unsigned velocityIterations;

  /**
   * Holds the number of iterations to perform when resolving
   * position.
   */
  unsigned positionIterations;

  /**
   * To avoid instability velocities smaller
   * than this value are considered to be zero. Too small and the
   * simulation may be unstable, too large and the bodies may
   * interpenetrate visually. A good starting point is the default
   * of 0.01.
   */
  real velocityEpsilon;

  /**
   * To avoid instability penetrations
   * smaller than this value are considered to be not interpenetrating.
   * Too small and the simulation may be unstable, too large and the
   * bodies may interpenetrate visually. A good starting point is
   * the default of0.01.
   */
  real positionEpsilon;

  /**
   * SAKBox: scratch space for sleepInGroups. Their contents are only used
   * within one call; they are members so their memory is reused each step
   * rather than allocated afresh.
   */
  struct ContactGroup {
    unsigned size = 0;
    bool settled = true;
  };
  std::unordered_map<RigidBody *, RigidBody *> groupParent;
  std::unordered_map<RigidBody *, ContactGroup> groups;

public:
  /**
   * SAKBox: how a set of contacts is resolved.
   */
  enum class Method {
    /**
     * The book's resolver: positions, then velocities, one contact at a
     * time, the worst first. Fast and robust for impacts and flat resting,
     * but friction at one contact is ignored while another is resolved, so
     * awake stacks creep sideways and tall ones fall (book sections 15.5
     * and 17.3.1).
     */
    oneContactAtATime,
    /**
     * Sequential impulses (Catto, "Iterative Dynamics with Temporal
     * Coherence", GDC 2005), which the book outlines in chapter 18. Every
     * contact is swept many times, keeping a running total of its push and
     * friction (the friction clamped to the cone allowed by the push), so
     * all the contacts on a body are satisfied together. Each step starts
     * from the last step's totals (warm starting). Bodies are then moved as
     * if they had had the resolved velocities for the whole step, and any
     * overlap left is removed by moving them along the contact normals,
     * without changing their velocities (split impulses). Awake stacks of
     * ten boxes stand still. See resolveTogether().
     */
    sequentialImpulses,
  };

  /** SAKBox: chooses how contacts are resolved (the book's way by default). */
  void setMethod(Method method) { this->method = method; }

  /** SAKBox: how contacts are resolved. */
  Method getMethod() const { return method; }

  /**
   * SAKBox: for Method::sequentialImpulses, how many times each kind of
   * sweep visits every contact (see resolveTogether): velocity sweeps for
   * pushes and friction, settle sweeps after the bodies have moved, and
   * position sweeps for overlap. Unlike setIterations' counts these don't
   * grow with the number of contacts; each sweep costs one visit per
   * contact. The defaults, 40, 10 and 10, keep awake stacks of ten boxes
   * still.
   */
  void setSweeps(unsigned velocitySweeps, unsigned settleSweeps, unsigned positionSweeps);

  /**
   * SAKBox: forgets the pushes and friction kept from the last step for
   * warm starting (Method::sequentialImpulses). Call it when bodies are
   * teleported or deleted, so a new body at a freed address doesn't inherit
   * a deleted one's contacts.
   */
  void forgetWarmStart() { lastImpulses.clear(); }

protected:
  /** SAKBox: see setMethod. */
  Method method = Method::oneContactAtATime;

  /** SAKBox: see setSweeps. */
  unsigned velocitySweeps = 40;
  unsigned settleSweeps = 10;
  unsigned positionSweeps = 10;

  /**
   * SAKBox: the share of the overlap beyond positionEpsilon that each step
   * removes, for Method::sequentialImpulses: Box2D's b2_baumgarte and
   * Bullet's split-impulse m_erp2. Joints are put back together in full
   * (limited by maxCorrection), as Box2D's are.
   */
  static inline const real overlapCorrection = (real)0.2;

  /**
   * SAKBox: the most a contact point may have moved on its body since the
   * last step and still be taken for the same contact when warm starting.
   */
  static inline const real warmStartMatchDistance = (real)0.05;

  /**
   * SAKBox: the most the position sweeps move bodies apart, or joints
   * together, in one step (Box2D's b2_maxLinearCorrection), so a deep
   * overlap or a badly stretched joint is fixed over a few steps rather
   * than with a jump.
   */
  static inline const real maxCorrection = (real)0.2;

  /**
   * SAKBox: one contact as Method::sequentialImpulses works on it.
   */
  struct SolverContact {
    Contact *contact;
    // The bodies that move in response: null for the ground, immovable
    // bodies and sleeping ones
    RigidBody *movable[2];
    Vector3 normal, tangent[2];
    // How much the bodies' relative speed at the contact changes for a
    // unit impulse along the normal and along each tangent
    real normalResponse, tangentResponse[2];
    // The relative velocity along the normal when integrate() moved the
    // bodies this step (separating is positive)
    real startNormalVelocity;
    // The separating speed to leave with (zero, or the bounce)
    real bounceVelocity;
    // The normal velocity the velocity sweeps aim for at least
    real targetNormalVelocity;
    // The separating speed the position sweeps aim for, to remove overlap
    real targetSeparatingSpeed = 0;
    // Running totals of the impulses applied: velocity sweeps' push and
    // friction, and position sweeps' push
    real normalImpulse = 0, tangentImpulse[2] = {0, 0};
    real positionImpulse = 0;

    // The total velocity sweep impulse, in world coordinates
    Vector3 impulse() const {
      return normal * normalImpulse + tangent[0] * tangentImpulse[0] +
             tangent[1] * tangentImpulse[1];
    }
  };
  std::vector<SolverContact> solverContacts;

  /**
   * SAKBox: one joint as Method::sequentialImpulses works on it. With no
   * slack it holds its two anchor points together in every direction (a
   * ball and socket); with slack, it only pulls, along the line between
   * them, once they are as far apart as the slack (a cable).
   */
  struct SolverJoint {
    Joint *joint;
    // As for SolverContact: the bodies that move in response, and each
    // anchor's offset from its body's centre
    RigidBody *movable[2];
    Vector3 offset[2];
    bool cable;

    // Anchor two's position less anchor one's, and its velocity relative to
    // anchor one when integrate() moved the bodies
    Vector3 separation, startRelativeVelocity;

    // A ball and socket: the inverse of how much the anchors' relative
    // velocity changes for a unit impulse (the effective mass, a 3x3
    // matrix since the anchors may move in any direction)
    Matrix3 effectiveMass;

    // A cable: the direction from anchor one to anchor two, how much their
    // separating speed along it changes for a unit impulse, the distance
    // between them now, their separating speed when integrate() moved the
    // bodies, and the separating speed the sweeps allow (closing is always
    // allowed)
    Vector3 direction;
    real response;
    real length, startSpeed;
    real allowedSpeed;
    real cableImpulse = 0;

    // The position sweeps' running totals and targets
    Vector3 positionImpulse;
    real positionImpulseAlong = 0;
    Vector3 targetVelocity;
    real targetSpeed = 0;
  };
  std::vector<SolverJoint> solverJoints;

  /** SAKBox: scratch space for the book's resolver to add joints' contacts. */
  std::vector<Contact> contactsWithJoints;

  /** SAKBox: a body's linear and angular velocity. */
  struct Motion {
    Vector3 linear, angular;
  };

  /**
   * SAKBox: each movable body's velocities when integrate() moved it this
   * step, and the velocities the position sweeps use (they only move the
   * bodies, then are dropped).
   */
  std::unordered_map<RigidBody *, Motion> startVelocities;
  std::unordered_map<RigidBody *, Motion> separatingVelocities;

  /**
   * SAKBox: the total impulse each contact applied last step (in world
   * coordinates), and where it was on the first body (in that body's
   * coordinates), by pair of bodies, for warm starting.
   */
  struct KeptImpulse {
    Vector3 localPoint;
    Vector3 impulse;
    // Whether a contact this step has already taken it
    bool taken = false;
  };
  std::map<std::pair<const RigidBody *, const RigidBody *>, std::vector<KeptImpulse>> lastImpulses;

public:
  /**
   * Stores the number of velocity iterations used in the
   * last call to resolve contacts.
   */
  unsigned velocityIterationsUsed;

  /**
   * Stores the number of position iterations used in the
   * last call to resolve contacts.
   */
  unsigned positionIterationsUsed;

public:
  /**
   * Creates a new contact resolver with the given number of iterations
   * per resolution call, and optional epsilon values.
   */
  ContactResolver(unsigned iterations, real velocityEpsilon = (real)0.01,
                  real positionEpsilon = (real)0.01);

  /**
   * Creates a new contact resolver with the given number of iterations
   * for each kind of resolution, and optional epsilon values.
   */
  ContactResolver(unsigned velocityIterations, unsigned positionIterations,
                  real velocityEpsilon = (real)0.01, real positionEpsilon = (real)0.01);

  /**
   * Returns true if the resolver has valid settings and is ready to go.
   */
  bool isValid() {
    return (velocityIterations > 0) && (positionIterations > 0) && (positionEpsilon >= 0.0f) &&
           (positionEpsilon >= 0.0f);
  }

  /**
   * Sets the number of iterations for each resolution stage.
   */
  void setIterations(unsigned velocityIterations, unsigned positionIterations);

  /**
   * Sets the number of iterations for both resolution stages.
   */
  void setIterations(unsigned iterations);

  /**
   * Sets the tolerance value for both velocity and position.
   */
  void setEpsilon(real velocityEpsilon, real positionEpsilon);

  /**
   * Resolves a set of contacts for both penetration and velocity.
   *
   * Contacts that cannot interact with
   * each other should be passed to separate calls to resolveContacts,
   * as the resolution algorithm takes much longer for lots of
   * contacts than it does for the same number of contacts in small
   * sets.
   *
   * @param contactArray Pointer to an array of contact objects.
   *
   * @param numContacts The number of contacts in the array to resolve.
   *
   * The number of iterations through the resolution algorithm is
   * set with setIterations (SAKBox: it was once a parameter here,
   * and the comment still named it). It should be at least the number of
   * contacts (otherwise some constraints will not be resolved -
   * although sometimes this is not noticable). If the iterations are
   * not needed they will not be used, so adding more iterations may
   * not make any difference. In some cases you would need millions
   * of iterations. Think about the number of iterations as a bound:
   * if you specify a large number, sometimes the algorithm WILL use
   * it, and you may drop lots of frames.
   *
   * @param duration The duration of the previous integration step.
   * This is used to compensate for forces applied.
   *
   * SAKBox: afterwards, bodies touching each other fall asleep as a group
   * (see sleepInGroups).
   */
  void resolveContacts(Contact *contactArray, unsigned numContacts, real duration);

  /**
   * SAKBox: resolves the contacts and holds the joints together. Resolving
   * contacts together (Method::sequentialImpulses), each joint is a
   * constraint of its own between its two anchor points (see
   * resolveTogether); with the book's resolver, each joint adds the contact
   * Joint::addContact makes, as if it were a contact generator. Don't also
   * pass a joint's contact in `contactArray`.
   */
  void resolveContacts(Contact *contactArray, unsigned numContacts, real duration,
                       std::span<Joint *const> joints);

protected:
  /**
   * Sets up contacts ready for processing. This makes sure their
   * internal data is configured correctly and the correct set of bodies
   * is made alive.
   */
  void prepareContacts(Contact *contactArray, unsigned numContacts, real duration);

  /**
   * Resolves the velocity issues with the given array of constraints,
   * using the given number of iterations.
   */
  void adjustVelocities(Contact *contactArray, unsigned numContacts, real duration);

  /**
   * Resolves the positional issues with the given array of constraints,
   * using the given number of iterations.
   */
  void adjustPositions(Contact *contacts, unsigned numContacts, real duration);

  /**
   * SAKBox: puts each group of bodies the contacts join to sleep at once
   * (the ground and other immovable bodies don't join groups), when every
   * body in it has settled and can sleep. Bodies sleeping one by one kept
   * waking each other (Contact::matchAwakeState), so a stack never slept.
   * Each body is marked with whether it's in a group of two or more;
   * the next integrate() leaves those to this method. Waking is unchanged: a body
   * is woken through a contact with an awake body that needs resolving.
   */
  void sleepInGroups(Contact *contacts, unsigned numContacts, std::span<Joint *const> joints = {});

  /**
   * SAKBox: resolves the contacts by Method::sequentialImpulses: wakes
   * sleeping bodies that need to move, sets up each contact and warm starts
   * it, sweeps the velocities, moves the bodies by the change in velocity,
   * removes overlap, and keeps this step's impulses for the next.
   */
  void resolveTogether(Contact *contacts, unsigned numContacts, real duration,
                       std::span<Joint *const> joints);

  /** SAKBox: sets up and warm starts each joint for resolveTogether. */
  void setUpJoints(std::span<Joint *const> joints, real duration);

  /** SAKBox: one velocity sweep over the joints. */
  void sweepJoints();

  /**
   * SAKBox: the cable speeds the settle sweeps allow, once the bodies have
   * moved.
   */
  void settleJoints(real duration);

  /** SAKBox: the joints' part of removeOverlap: targets, then one sweep. */
  void setJointPositionTargets(real duration);
  void sweepJointPositions();

  /**
   * SAKBox: the velocity sweeps of resolveTogether, `sweeps` times, aiming
   * for each contact's `target` normal velocity.
   */
  void sweepVelocities(real SolverContact::*target, unsigned sweeps);

  /**
   * SAKBox: the overlap at a contact once the bodies have moved with their
   * current velocities for the step; negative while apart.
   */
  real overlapNow(const SolverContact &solverContact, real duration) const;

  /** SAKBox: the position sweeps of resolveTogether, removing overlap. */
  void removeOverlap(real duration);

  /**
   * SAKBox: the impulse `contact` applied last step, taking it so no other
   * contact this step does; zero if it's new.
   */
  Vector3 takeKeptImpulse(const Contact &contact);
};

/**
 * This is the basic polymorphic interface for contact generators
 * applying to rigid bodies.
 */
class ContactGenerator {
public:
  /**
   * SAKBox: a virtual destructor, so a contact generator deleted through this
   * interface is destroyed in full.
   */
  virtual ~ContactGenerator() = default;

  /**
   * Fills the given contact structure with the generated
   * contact. The contact pointer should point to the first
   * available contact in a contact array, where limit is the
   * maximum number of contacts in the array that can be written
   * to. The method returns the number of contacts that have
   * been written.
   */
  virtual unsigned addContact(Contact *contact, unsigned limit) const = 0;
};

} // namespace cyclone

#endif // CYCLONE_CONTACTS_H
