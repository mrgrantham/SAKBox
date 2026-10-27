/*
 * Interface file for the fine grained collision detection system.
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
 * This file contains the fine grained collision detection system.
 * It is used to return contacts between pairs of primitives.
 *
 * There are two groups of tests in this file. Intersection tests
 * use the fastest separating axis method to check if two objects
 * intersect, and the collision tests generate the contacts. The
 * collision tests typically use the intersection tests as an early
 * out.
 */
#ifndef CYCLONE_COLLISION_FINE_H
#define CYCLONE_COLLISION_FINE_H

#include "contacts.h"

#include <map>
#include <utility>
#include <vector>

namespace cyclone {

// Forward declarations of primitive friends
class IntersectionTests;
class CollisionDetector;
struct BoxContactFeature;

/**
 * Represents a primitive to detect collisions against.
 */
class CollisionPrimitive {
public:
  /**
   * This class exists to help the collision detector
   * and intersection routines, so they should have
   * access to its data.
   */
  friend class IntersectionTests;
  friend class CollisionDetector;

  /**
   * The rigid body that is represented by this primitive.
   */
  RigidBody *body;

  /**
   * The offset of this primitive from the given rigid body.
   */
  Matrix4 offset;

  /**
   * Calculates the internals for the primitive.
   */
  void calculateInternals();

  /**
   * This is a convenience function to allow access to the
   * axis vectors in the transform for this primitive.
   */
  Vector3 getAxis(unsigned index) const { return transform.getAxisVector(index); }

  /**
   * Returns the resultant transform of the primitive, calculated from
   * the combined offset of the primitive and the transform
   * (orientation + position) of the rigid body to which it is
   * attached.
   */
  const Matrix4 &getTransform() const { return transform; }

protected:
  /**
   * The resultant transform of the primitive. This is
   * calculated by combining the offset of the primitive
   * with the transform of the rigid body.
   */
  Matrix4 transform;
};

/**
 * Represents a rigid body that can be treated as a sphere
 * for collision detection.
 */
class CollisionSphere : public CollisionPrimitive {
public:
  /**
   * The radius of the sphere.
   */
  real radius;
};

/**
 * The plane is not a primitive: it doesn't represent another
 * rigid body. It is used for contacts with the immovable
 * world geometry.
 */
class CollisionPlane {
public:
  /**
   * The plane normal
   */
  Vector3 direction;

  /**
   * The distance of the plane from the origin.
   */
  real offset;
};

/**
 * Represents a rigid body that can be treated as an aligned bounding
 * box for collision detection.
 */
class CollisionBox : public CollisionPrimitive {
public:
  /**
   * Holds the half-sizes of the box along each of its local axes.
   */
  Vector3 halfSize;
};

/**
 * A wrapper class that holds fast intersection tests. These
 * can be used to drive the coarse collision detection system or
 * as an early out in the full collision tests below.
 */
/**
 * SAKBox: a capsule, "a cylinder with hemispherical ends", one of the
 * primitives the book builds its contact generation for (section 13.1),
 * though it only works through spheres and boxes ("Capsules are more
 * difficult than spheres, but can be useful in some situations"). It's
 * every point within `radius` of a line segment along the primitive's Y
 * axis, from -halfLength to +halfLength: a sphere swept along the
 * segment. So its contacts are a sphere's, at the points of the segment
 * nearest the other shape, and CollisionDetector's capsule functions make
 * them with its sphere functions.
 */
class CollisionCapsule : public CollisionPrimitive {
public:
  /** The radius of the ends, and of the cylinder between them. */
  real radius;

  /** Half the length of the segment between the ends' centres. */
  real halfLength;

  /**
   * The sphere of the capsule's radius at `along` on its segment (from
   * -halfLength to +halfLength), on its body, ready to collide.
   */
  CollisionSphere sphereAt(real along) const;
};

class IntersectionTests {
public:
  static bool sphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane);

  static bool sphereAndSphere(const CollisionSphere &one, const CollisionSphere &two);

  static bool boxAndBox(const CollisionBox &one, const CollisionBox &two);

  /**
   * Does an intersection test on an arbitrarily aligned box and a
   * half-space.
   *
   * The box is given as a transform matrix, including
   * position, and a vector of half-sizes for the extend of the
   * box along each local axis.
   *
   * The half-space is given as a direction (i.e. unit) vector and the
   * offset of the limiting plane from the origin, along the given
   * direction.
   */
  static bool boxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane);
};

/**
 * A helper structure that contains information for the detector to use
 * in building its contact data.
 */
struct CollisionData {
  /**
   * Holds the base of the collision data: the first contact
   * in the array. This is used so that the contact pointer (below)
   * can be incremented each time a contact is detected, while
   * this pointer points to the first contact found.
   */
  Contact *contactArray;

  /** Holds the contact array to write into. */
  Contact *contacts;

  /** Holds the maximum number of contacts the array can take. */
  int contactsLeft;

  /** Holds the number of contacts found so far. */
  unsigned contactCount;

  /** Holds the friction value to write into any collisions. */
  real friction;

  /** Holds the restitution value to write into any collisions. */
  real restitution;

  /**
   * Holds the collision tolerance, even uncolliding objects this
   * close should have collisions generated.
   */
  real tolerance;

  /**
   * Checks if there are more contacts available in the contact
   * data.
   */
  bool hasMoreContacts() { return contactsLeft > 0; }

  /**
   * Resets the data so that it has no used contacts recorded.
   */
  void reset(unsigned maxContacts) {
    contactsLeft = maxContacts;
    contactCount = 0;
    contacts = contactArray;
  }

  /**
   * Notifies the data that the given number of contacts have
   * been added.
   */
  void addContacts(unsigned count) {
    // Reduce the number of contacts remaining, add number used
    contactsLeft -= count;
    contactCount += count;

    // Move the array forward
    contacts += count;
  }
};

/**
 * A wrapper class that holds the fine grained collision detection
 * routines.
 *
 * Each of the functions has the same format: it takes the details
 * of two objects, and a pointer to a contact array to fill. It
 * returns the number of contacts it wrote into the array.
 */
class CollisionDetector {
public:
  static unsigned sphereAndHalfSpace(const CollisionSphere &sphere, const CollisionPlane &plane,
                                     CollisionData *data);

  static unsigned sphereAndTruePlane(const CollisionSphere &sphere, const CollisionPlane &plane,
                                     CollisionData *data);

  static unsigned sphereAndSphere(const CollisionSphere &one, const CollisionSphere &two,
                                  CollisionData *data);

  /**
   * Does a collision test on a collision box and a plane representing
   * a half-space (i.e. the normal of the plane
   * points out of the half-space).
   */
  static unsigned boxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane,
                                  CollisionData *data);

  static unsigned boxAndBox(const CollisionBox &one, const CollisionBox &two, CollisionData *data);

  /**
   * SAKBox: as boxAndBox, also reporting which parts of the boxes the
   * contact is between (left unchanged if there's no contact). Used by
   * BoxContactCache.
   */
  static unsigned boxAndBox(const CollisionBox &one, const CollisionBox &two, CollisionData *data,
                            BoxContactFeature *touching);

  static unsigned boxAndPoint(const CollisionBox &box, const Vector3 &point, CollisionData *data);

  static unsigned boxAndSphere(const CollisionBox &box, const CollisionSphere &sphere,
                               CollisionData *data);

  /**
   * SAKBox: a capsule against a half-space, such as the ground: the
   * capsule's lowest point is always on one of its ends, so each end
   * touches as a sphere would. Lying flat, it touches at both, so it rests
   * level, as a box does on its corners.
   */
  static unsigned capsuleAndHalfSpace(const CollisionCapsule &capsule, const CollisionPlane &plane,
                                      CollisionData *data);

  /**
   * SAKBox: a capsule against a sphere: the sphere against the capsule's
   * sphere at the point of its segment nearest the sphere's centre.
   */
  static unsigned capsuleAndSphere(const CollisionCapsule &capsule, const CollisionSphere &sphere,
                                   CollisionData *data);

  /**
   * SAKBox: two capsules: the spheres at the nearest points of their
   * segments (Ericson, "Real-Time Collision Detection", section 5.1.9).
   * Side by side (their segments parallel), they touch along a line, so
   * there's a contact at each end of where the segments overlap, and they
   * rest level on each other.
   */
  static unsigned capsuleAndCapsule(const CollisionCapsule &one, const CollisionCapsule &two,
                                    CollisionData *data);

  /**
   * SAKBox: a capsule against a box: each end that touches the box, as a
   * sphere would (two, lying on a face), or if neither does, the point of
   * the segment nearest the box (lying across an edge).
   */
  static unsigned capsuleAndBox(const CollisionCapsule &capsule, const CollisionBox &box,
                                CollisionData *data);
};

/**
 * SAKBox: which parts of two boxes a contact is between. Unlike the contact
 * itself, this stays the same while the boxes move a little, so it can be
 * kept from frame to frame (see BoxContactCache).
 *
 * - A vertex of one box on a face of the other.
 * - An edge of each, crossing (the edge-on-edge contact boxAndBox finds).
 * - Where faces rest on each other, an edge of one box's face crossing the
 *   edge of the other box's face (the reference face): the incident edge
 *   crosses the plane through the reference edge, square to the face, and
 *   the contact is that point's depth below the face, along its normal.
 *   These are the points that clipping one face against the other finds;
 *   `faceAxis` is the reference face's axis (its side is that axis's bit of
 *   the reference edge).
 *
 * Corners are numbered by bits: 1 for +x, 2 for +y, 4 for +z (clear for
 * the negative side). A face is its axis and the side of the box it's on
 * (that axis's bit of `corner`); an edge is its axis and which of the four
 * edges along it (the other two axes' bits).
 */
struct BoxContactFeature {
  enum Kind : unsigned char {
    VertexOfTwoOnFaceOfOne,
    VertexOfOneOnFaceOfTwo,
    EdgeOnEdge,
    EdgeCrossingFaceOfOne, // one's edge is the reference edge, two's crosses it
    EdgeCrossingFaceOfTwo,
  };
  Kind kind = EdgeOnEdge;
  unsigned char oneAxis = 0, oneCorner = 0;
  unsigned char twoAxis = 0, twoCorner = 0;
  unsigned char faceAxis = 0;

  bool operator==(const BoxContactFeature &) const = default;
};

/**
 * SAKBox: contacts between pairs of boxes, kept from frame to frame, as the
 * book describes (section 13.3.5). CollisionDetector::boxAndBox finds one
 * contact, at the deepest point, so a box resting on another is held up at
 * a single corner and rocks. Here each frame's deepest contact is added to
 * those found for the same pair in earlier frames, and all of them are
 * recalculated for where the boxes are now, so within a few frames a
 * resting box has a contact at each of its corners on the face below. A
 * kept contact is dropped once its two parts have moved more than the
 * collision data's tolerance apart, or a vertex has slid off its face, or
 * two edges no longer cross.
 *
 * Keep one cache for a whole scene and call boxAndBox in place of
 * CollisionDetector::boxAndBox, with each pair in the same order every
 * frame. Call forget() when a box is removed, clear() when the scene
 * resets.
 */
class BoxContactCache {
public:
  /**
   * At most this many contacts are kept for one pair of boxes, those that
   * best cover the area where they touch: as many as the corners of the
   * area where two box faces overlap. (The book's resolver stacks worse
   * with fewer: cut to four, as Gregorius suggests for faster solvers,
   * eight awake boxes fell within a second.)
   */
  static constexpr unsigned maxContactsPerPair = 8;

  /**
   * Adds the contacts between the two boxes to the collision data: the
   * deepest one now and those kept from earlier frames. Returns how many
   * were added.
   */
  unsigned boxAndBox(const CollisionBox &one, const CollisionBox &two, CollisionData *data);

  /** Forgets the contacts kept for every pair including this box. */
  void forget(const CollisionBox &box);

  /** Forgets every contact kept. */
  void clear();

private:
  std::map<std::pair<const CollisionBox *, const CollisionBox *>, std::vector<BoxContactFeature>>
      pairs;
};

} // namespace cyclone

#endif // CYCLONE_COLLISION_FINE_H
