/*
 * Implementation file for the fine grained collision detector.
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
#include <cstdio>
#include <cstdlib>
#include <cyclone/collide_fine.h>
#include <memory.h>

using namespace cyclone;

void CollisionPrimitive::calculateInternals() { transform = body->getTransform() * offset; }

bool IntersectionTests::sphereAndHalfSpace(const CollisionSphere &sphere,
                                           const CollisionPlane &plane) {
  // Find the distance from the origin
  real ballDistance = plane.direction * sphere.getAxis(3) - sphere.radius;

  // Check for the intersection
  return ballDistance <= plane.offset;
}

bool IntersectionTests::sphereAndSphere(const CollisionSphere &one, const CollisionSphere &two) {
  // Find the vector between the objects
  Vector3 midline = one.getAxis(3) - two.getAxis(3);

  // See if it is large enough.
  return midline.squareMagnitude() < (one.radius + two.radius) * (one.radius + two.radius);
}

static inline real transformToAxis(const CollisionBox &box, const Vector3 &axis) {
  return box.halfSize.x * real_abs(axis * box.getAxis(0)) +
         box.halfSize.y * real_abs(axis * box.getAxis(1)) +
         box.halfSize.z * real_abs(axis * box.getAxis(2));
}

/**
 * This function checks if the two boxes overlap
 * along the given axis. The final parameter toCentre
 * is used to pass in the vector between the boxes centre
 * points, to avoid having to recalculate it each time.
 */
static inline bool overlapOnAxis(const CollisionBox &one, const CollisionBox &two,
                                 const Vector3 &axis, const Vector3 &toCentre) {
  // Project the half-size of one onto axis
  real oneProject = transformToAxis(one, axis);
  real twoProject = transformToAxis(two, axis);

  // Project this onto the axis
  real distance = real_abs(toCentre * axis);

  // Check for overlap
  return (distance < oneProject + twoProject);
}

// SAKBox: a cross product of parallel edges is (almost) zero and can't
// separate anything, but compared 0 < 0 and reported the boxes apart; skip it
// as CollisionDetector::boxAndBox's tryAxis does.
static inline bool overlapOnAxisOrDegenerate(const CollisionBox &one, const CollisionBox &two,
                                             const Vector3 &axis, const Vector3 &toCentre) {
  return axis.squareMagnitude() < 0.0001 || overlapOnAxis(one, two, axis, toCentre);
}

// This preprocessor definition is only used as a convenience
// in the boxAndBox intersection  method.
#define TEST_OVERLAP(axis) overlapOnAxisOrDegenerate(one, two, (axis), toCentre)

bool IntersectionTests::boxAndBox(const CollisionBox &one, const CollisionBox &two) {
  // Find the vector between the two centres
  Vector3 toCentre = two.getAxis(3) - one.getAxis(3);

  return (
      // Check on box one's axes first
      TEST_OVERLAP(one.getAxis(0)) && TEST_OVERLAP(one.getAxis(1)) &&
      TEST_OVERLAP(one.getAxis(2)) &&

      // And on two's
      TEST_OVERLAP(two.getAxis(0)) && TEST_OVERLAP(two.getAxis(1)) &&
      TEST_OVERLAP(two.getAxis(2)) &&

      // Now on the cross products
      TEST_OVERLAP(one.getAxis(0) % two.getAxis(0)) &&
      TEST_OVERLAP(one.getAxis(0) % two.getAxis(1)) &&
      TEST_OVERLAP(one.getAxis(0) % two.getAxis(2)) &&
      TEST_OVERLAP(one.getAxis(1) % two.getAxis(0)) &&
      TEST_OVERLAP(one.getAxis(1) % two.getAxis(1)) &&
      TEST_OVERLAP(one.getAxis(1) % two.getAxis(2)) &&
      TEST_OVERLAP(one.getAxis(2) % two.getAxis(0)) &&
      TEST_OVERLAP(one.getAxis(2) % two.getAxis(1)) &&
      TEST_OVERLAP(one.getAxis(2) % two.getAxis(2)));
}
#undef TEST_OVERLAP

bool IntersectionTests::boxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane) {
  // Work out the projected radius of the box onto the plane direction
  real projectedRadius = transformToAxis(box, plane.direction);

  // Work out how far the box is from the origin
  real boxDistance = plane.direction * box.getAxis(3) - projectedRadius;

  // Check for the intersection
  return boxDistance <= plane.offset;
}

unsigned CollisionDetector::sphereAndTruePlane(const CollisionSphere &sphere,
                                               const CollisionPlane &plane, CollisionData *data) {
  // Make sure we have contacts
  if (data->contactsLeft <= 0)
    return 0;

  // Cache the sphere position
  Vector3 position = sphere.getAxis(3);

  // Find the distance from the plane
  real centreDistance = plane.direction * position - plane.offset;

  // Check if we're within radius
  if (centreDistance * centreDistance > sphere.radius * sphere.radius) {
    return 0;
  }

  // Check which side of the plane we're on
  Vector3 normal = plane.direction;
  real penetration = -centreDistance;
  if (centreDistance < 0) {
    normal *= -1;
    penetration = -penetration;
  }
  penetration += sphere.radius;

  // Create the contact - it has a normal in the plane direction.
  Contact *contact = data->contacts;
  contact->contactNormal = normal;
  contact->penetration = penetration;
  contact->contactPoint = position - plane.direction * centreDistance;
  contact->setBodyData(sphere.body, nullptr, data->friction, data->restitution);

  data->addContacts(1);
  return 1;
}

unsigned CollisionDetector::sphereAndHalfSpace(const CollisionSphere &sphere,
                                               const CollisionPlane &plane, CollisionData *data) {
  // Make sure we have contacts
  if (data->contactsLeft <= 0)
    return 0;

  // Cache the sphere position
  Vector3 position = sphere.getAxis(3);

  // Find the distance from the plane
  real ballDistance = plane.direction * position - sphere.radius - plane.offset;

  if (ballDistance >= 0)
    return 0;

  // Create the contact - it has a normal in the plane direction.
  Contact *contact = data->contacts;
  contact->contactNormal = plane.direction;
  contact->penetration = -ballDistance;
  contact->contactPoint = position - plane.direction * (ballDistance + sphere.radius);
  contact->setBodyData(sphere.body, nullptr, data->friction, data->restitution);

  data->addContacts(1);
  return 1;
}

unsigned CollisionDetector::sphereAndSphere(const CollisionSphere &one, const CollisionSphere &two,
                                            CollisionData *data) {
  // Make sure we have contacts
  if (data->contactsLeft <= 0)
    return 0;

  // Cache the sphere positions
  Vector3 positionOne = one.getAxis(3);
  Vector3 positionTwo = two.getAxis(3);

  // Find the vector between the objects
  Vector3 midline = positionOne - positionTwo;
  real size = midline.magnitude();

  // See if it is large enough.
  if (size <= 0.0f || size >= one.radius + two.radius) {
    return 0;
  }

  // We manually create the normal, because we have the
  // size to hand.
  Vector3 normal = midline * (((real)1.0) / size);

  Contact *contact = data->contacts;
  contact->contactNormal = normal;
  // SAKBox: upstream used positionOne + midline / 2, a point outside sphere
  // one on the far side from sphere two.
  contact->contactPoint = positionTwo + midline * (real)0.5;
  contact->penetration = (one.radius + two.radius - size);
  contact->setBodyData(one.body, two.body, data->friction, data->restitution);

  data->addContacts(1);
  return 1;
}

/*
 * This function checks if the two boxes overlap
 * along the given axis, returning the ammount of overlap.
 * The final parameter toCentre
 * is used to pass in the vector between the boxes centre
 * points, to avoid having to recalculate it each time.
 */
static inline real penetrationOnAxis(const CollisionBox &one, const CollisionBox &two,
                                     const Vector3 &axis, const Vector3 &toCentre) {
  // Project the half-size of one onto axis
  real oneProject = transformToAxis(one, axis);
  real twoProject = transformToAxis(two, axis);

  // Project this onto the axis
  real distance = real_abs(toCentre * axis);

  // Return the overlap (i.e. positive indicates
  // overlap, negative indicates separation).
  return oneProject + twoProject - distance;
}

// SAKBox: how much shallower an axis tested later must be to be chosen over
// the best so far (Box2D-Lite's tolerances, by Erin Catto). Without this,
// boxes resting flat have several axes tied for shallowest, and which one
// wins flips between frames with rounding: the contact jumps from one face
// to another, or to an edge, and the box jitters (Catto, "Contact
// Manifolds", GDC 2007: "feature flip-flop"). Box one's faces are tested
// first, then box two's, then the edge pairs, so faces are preferred over
// edges, and box one's faces over box two's.
static constexpr double relativeAxisTolerance = 0.95;
static constexpr double absoluteAxisTolerance = 0.01;

static inline bool tryAxis(const CollisionBox &one, const CollisionBox &two, Vector3 axis,
                           const Vector3 &toCentre, unsigned index,

                           // These values may be updated
                           real &smallestPenetration, unsigned &smallestCase) {
  // Make sure we have a normalized axis, and don't check almost parallel axes
  if (axis.squareMagnitude() < 0.0001)
    return true;
  axis.normalise();

  real penetration = penetrationOnAxis(one, two, axis, toCentre);

  if (penetration < 0)
    return false;
  // SAKBox: the first axis is taken as it is; later ones must be clearly
  // shallower (see relativeAxisTolerance), by a margin scaled to the
  // smaller box
  const real margin =
      (real)absoluteAxisTolerance * std::min({one.halfSize.x, one.halfSize.y, one.halfSize.z,
                                              two.halfSize.x, two.halfSize.y, two.halfSize.z});
  if (smallestCase == 0xffffff ||
      penetration < (real)relativeAxisTolerance * smallestPenetration - margin) {
    smallestPenetration = penetration;
    smallestCase = index;
  }
  return true;
}

void fillPointFaceBoxBox(const CollisionBox &one, const CollisionBox &two, const Vector3 &toCentre,
                         CollisionData *data, unsigned best, real pen) {
  // This method is called when we know that a vertex from
  // box two is in contact with box one.

  Contact *contact = data->contacts;

  // We know which axis the collision is on (i.e. best),
  // but we need to work out which of the two faces on
  // this axis.
  Vector3 normal = one.getAxis(best);
  if (one.getAxis(best) * toCentre > 0) {
    normal = normal * -1.0f;
  }

  // Work out which vertex of box two we're colliding with.
  // Using toCentre doesn't work!
  Vector3 vertex = two.halfSize;
  if (two.getAxis(0) * normal < 0)
    vertex.x = -vertex.x;
  if (two.getAxis(1) * normal < 0)
    vertex.y = -vertex.y;
  if (two.getAxis(2) * normal < 0)
    vertex.z = -vertex.z;

  // Create the contact data
  contact->contactNormal = normal;
  contact->penetration = pen;
  contact->contactPoint = two.getTransform() * vertex;
  contact->setBodyData(one.body, two.body, data->friction, data->restitution);
}

static inline Vector3 contactPoint(const Vector3 &pOne, const Vector3 &dOne, real oneSize,
                                   const Vector3 &pTwo, const Vector3 &dTwo, real twoSize,

                                   // If this is true, and the contact point is outside
                                   // the edge (in the case of an edge-face contact) then
                                   // we use one's midpoint, otherwise we use two's.
                                   bool useOne) {
  Vector3 toSt, cOne, cTwo;
  real dpStaOne, dpStaTwo, dpOneTwo, smOne, smTwo;
  real denom, mua, mub;

  smOne = dOne.squareMagnitude();
  smTwo = dTwo.squareMagnitude();
  dpOneTwo = dTwo * dOne;

  toSt = pOne - pTwo;
  dpStaOne = dOne * toSt;
  dpStaTwo = dTwo * toSt;

  denom = smOne * smTwo - dpOneTwo * dpOneTwo;

  // Zero denominator indicates parrallel lines
  if (real_abs(denom) < 0.0001f) {
    return useOne ? pOne : pTwo;
  }

  mua = (dpOneTwo * dpStaTwo - smTwo * dpStaOne) / denom;
  mub = (smOne * dpStaTwo - dpOneTwo * dpStaOne) / denom;

  // If either of the edges has the nearest point out
  // of bounds, then the edges aren't crossed, we have
  // an edge-face contact. Our point is on the edge, which
  // we know from the useOne parameter.
  if (mua > oneSize || mua < -oneSize || mub > twoSize || mub < -twoSize) {
    return useOne ? pOne : pTwo;
  } else {
    cOne = pOne + dOne * mua;
    cTwo = pTwo + dTwo * mub;

    return cOne * 0.5 + cTwo * 0.5;
  }
}

// This preprocessor definition is only used as a convenience
// in the boxAndBox contact generation method.
#define CHECK_OVERLAP(axis, index)                                                                 \
  if (!tryAxis(one, two, (axis), toCentre, (index), pen, best))                                    \
    return 0;

unsigned CollisionDetector::boxAndBox(const CollisionBox &one, const CollisionBox &two,
                                      CollisionData *data) {
  return boxAndBox(one, two, data, nullptr);
}

// The corner bits (see BoxContactFeature) of the corner of `box` furthest
// along `direction`
static unsigned char cornerBits(const CollisionBox &box, const Vector3 &direction) {
  unsigned char bits = 0;
  for (unsigned i = 0; i < 3; i++)
    if (box.getAxis(i) * direction >= 0)
      bits |= (unsigned char)(1 << i);
  return bits;
}

unsigned CollisionDetector::boxAndBox(const CollisionBox &one, const CollisionBox &two,
                                      CollisionData *data, BoxContactFeature *touching) {
  // if (!IntersectionTests::boxAndBox(one, two)) return 0;

  // Find the vector between the two centres
  Vector3 toCentre = two.getAxis(3) - one.getAxis(3);

  // We start assuming there is no contact
  real pen = REAL_MAX;
  unsigned best = 0xffffff;

  // Now we check each axes, returning if it gives us
  // a separating axis, and keeping track of the axis with
  // the smallest penetration otherwise.
  CHECK_OVERLAP(one.getAxis(0), 0);
  CHECK_OVERLAP(one.getAxis(1), 1);
  CHECK_OVERLAP(one.getAxis(2), 2);

  CHECK_OVERLAP(two.getAxis(0), 3);
  CHECK_OVERLAP(two.getAxis(1), 4);
  CHECK_OVERLAP(two.getAxis(2), 5);

  // Store the best axis-major, in case we run into almost
  // parallel edge collisions later
  unsigned bestSingleAxis = best;

  CHECK_OVERLAP(one.getAxis(0) % two.getAxis(0), 6);
  CHECK_OVERLAP(one.getAxis(0) % two.getAxis(1), 7);
  CHECK_OVERLAP(one.getAxis(0) % two.getAxis(2), 8);
  CHECK_OVERLAP(one.getAxis(1) % two.getAxis(0), 9);
  CHECK_OVERLAP(one.getAxis(1) % two.getAxis(1), 10);
  CHECK_OVERLAP(one.getAxis(1) % two.getAxis(2), 11);
  CHECK_OVERLAP(one.getAxis(2) % two.getAxis(0), 12);
  CHECK_OVERLAP(one.getAxis(2) % two.getAxis(1), 13);
  CHECK_OVERLAP(one.getAxis(2) % two.getAxis(2), 14);

  // Make sure we've got a result.
  assert(best != 0xffffff);

  // We now know there's a collision, and we know which
  // of the axes gave the smallest penetration. We now
  // can deal with it in different ways depending on
  // the case.
  if (best < 3) {
    // We've got a vertex of box two on a face of box one.
    fillPointFaceBoxBox(one, two, toCentre, data, best, pen);
    if (touching) {
      // The face of box one towards box two; the vertex of box two furthest
      // along the contact normal (as fillPointFaceBoxBox picks it)
      touching->kind = BoxContactFeature::VertexOfTwoOnFaceOfOne;
      touching->oneAxis = (unsigned char)best;
      touching->oneCorner = (unsigned char)((one.getAxis(best) * toCentre > 0) << best);
      touching->twoAxis = 0;
      touching->twoCorner = cornerBits(two, data->contacts->contactNormal);
    }
    data->addContacts(1);
    return 1;
  } else if (best < 6) {
    // We've got a vertex of box one on a face of box two.
    // We use the same algorithm as above, but swap around
    // one and two (and therefore also the vector between their
    // centres).
    fillPointFaceBoxBox(two, one, toCentre * -1.0f, data, best - 3, pen);
    if (touching) {
      touching->kind = BoxContactFeature::VertexOfOneOnFaceOfTwo;
      touching->twoAxis = (unsigned char)(best - 3);
      touching->twoCorner = (unsigned char)((two.getAxis(best - 3) * toCentre < 0) << (best - 3));
      touching->oneAxis = 0;
      touching->oneCorner = cornerBits(one, data->contacts->contactNormal);
    }
    data->addContacts(1);
    return 1;
  } else {
    // We've got an edge-edge contact. Find out which axes
    best -= 6;
    unsigned oneAxisIndex = best / 3;
    unsigned twoAxisIndex = best % 3;
    Vector3 oneAxis = one.getAxis(oneAxisIndex);
    Vector3 twoAxis = two.getAxis(twoAxisIndex);
    Vector3 axis = oneAxis % twoAxis;
    axis.normalise();

    // The axis should point from box one to box two.
    if (axis * toCentre > 0)
      axis = axis * -1.0f;

    // We have the axes, but not the edges: each axis has 4 edges parallel
    // to it, we need to find which of the 4 for each object. We do
    // that by finding the point in the centre of the edge. We know
    // its component in the direction of the box's collision axis is zero
    // (its a mid-point) and we determine which of the extremes in each
    // of the other axes is closest.
    Vector3 ptOnOneEdge = one.halfSize;
    Vector3 ptOnTwoEdge = two.halfSize;
    for (unsigned i = 0; i < 3; i++) {
      if (i == oneAxisIndex)
        ptOnOneEdge[i] = 0;
      else if (one.getAxis(i) * axis > 0)
        ptOnOneEdge[i] = -ptOnOneEdge[i];

      if (i == twoAxisIndex)
        ptOnTwoEdge[i] = 0;
      else if (two.getAxis(i) * axis < 0)
        ptOnTwoEdge[i] = -ptOnTwoEdge[i];
    }

    // Move them into world coordinates (they are already oriented
    // correctly, since they have been derived from the axes).
    ptOnOneEdge = one.transform * ptOnOneEdge;
    ptOnTwoEdge = two.transform * ptOnTwoEdge;

    // So we have a point and a direction for the colliding edges.
    // We need to find out point of closest approach of the two
    // line-segments.
    Vector3 vertex = contactPoint(ptOnOneEdge, oneAxis, one.halfSize[oneAxisIndex], ptOnTwoEdge,
                                  twoAxis, two.halfSize[twoAxisIndex], bestSingleAxis > 2);

    // We can fill the contact.
    Contact *contact = data->contacts;

    contact->penetration = pen;
    contact->contactNormal = axis;
    contact->contactPoint = vertex;
    contact->setBodyData(one.body, two.body, data->friction, data->restitution);
    if (touching) {
      // The edges chosen above, with their own axis's bit cleared
      touching->kind = BoxContactFeature::EdgeOnEdge;
      touching->oneAxis = (unsigned char)oneAxisIndex;
      touching->oneCorner = (unsigned char)(cornerBits(one, axis * -1) & ~(1u << oneAxisIndex));
      touching->twoAxis = (unsigned char)twoAxisIndex;
      touching->twoCorner = (unsigned char)(cornerBits(two, axis) & ~(1u << twoAxisIndex));
    }
    data->addContacts(1);
    return 1;
  }
  return 0;
}
#undef CHECK_OVERLAP

// SAKBox: BoxContactCache (see collide_fine.h)

// The point of `box`, in its own coordinates, at the given corner bits: a
// vertex, or with the coordinate along `alongAxis` (if below 3) at 0, the
// middle of an edge
static Vector3 cornerPoint(const CollisionBox &box, unsigned char corner, unsigned alongAxis = 3) {
  Vector3 point;
  for (unsigned i = 0; i < 3; i++)
    point[i] = i == alongAxis ? (real)0 : (corner & (1 << i) ? box.halfSize[i] : -box.halfSize[i]);
  return point;
}

// Works out a kept contact for where the boxes are now, writing it to
// `contact`. Returns false if its parts are no longer touching: more than
// the tolerance apart, a vertex off its face, or edges that don't cross.
static bool fillKeptContact(const CollisionBox &one, const CollisionBox &two,
                            const BoxContactFeature &feature, const CollisionData &data,
                            Contact *contact) {
  const real tolerance = data.tolerance;

  if (feature.kind == BoxContactFeature::EdgeCrossingFaceOfOne ||
      feature.kind == BoxContactFeature::EdgeCrossingFaceOfTwo) {
    // An edge of the incident face crossing the reference face's side
    // plane through the reference edge, worked out in the reference box's
    // coordinates (Sutherland-Hodgman clipping, one plane at a time)
    const bool faceOnOne = feature.kind == BoxContactFeature::EdgeCrossingFaceOfOne;
    const CollisionBox &reference = faceOnOne ? one : two;
    const CollisionBox &incident = faceOnOne ? two : one;
    const unsigned referenceAxis = faceOnOne ? feature.oneAxis : feature.twoAxis;
    const unsigned char referenceEdge = faceOnOne ? feature.oneCorner : feature.twoCorner;
    const unsigned incidentAxis = faceOnOne ? feature.twoAxis : feature.oneAxis;
    const unsigned char incidentEdge = faceOnOne ? feature.twoCorner : feature.oneCorner;
    const unsigned faceAxis = feature.faceAxis;
    const unsigned sideAxis = 3 - faceAxis - referenceAxis;
    const real faceSide = (referenceEdge & (1 << faceAxis)) ? 1 : -1;
    const real side = (referenceEdge & (1 << sideAxis)) ? 1 : -1;

    const Matrix4 &toReference = reference.getTransform();
    const Vector3 middle = toReference.transformInverse(
        incident.getTransform() * cornerPoint(incident, incidentEdge, incidentAxis));
    const Vector3 along = toReference.transformInverseDirection(incident.getAxis(incidentAxis));
    if (real_abs(along[sideAxis]) < (real)1e-6)
      return false; // the edges are parallel, so never cross
    const real slack = (real)1e-6;
    const real t = (side * reference.halfSize[sideAxis] - middle[sideAxis]) / along[sideAxis];
    if (real_abs(t) > incident.halfSize[incidentAxis] + slack)
      return false; // the incident edge ends before the plane
    const Vector3 crossing = middle + along * t;
    if (real_abs(crossing[referenceAxis]) > reference.halfSize[referenceAxis] + slack)
      return false; // beyond the end of the reference edge, off the face
    const real depth = reference.halfSize[faceAxis] - faceSide * crossing[faceAxis];
    if (depth < -tolerance || depth > 2 * reference.halfSize[faceAxis])
      return false;

    contact->contactNormal = reference.getAxis(faceAxis) * -faceSide;
    contact->penetration = depth;
    contact->contactPoint = toReference * crossing;
    contact->setBodyData(reference.body, incident.body, data.friction, data.restitution);
    return true;
  }

  if (feature.kind != BoxContactFeature::EdgeOnEdge) {
    // A vertex on a face: as fillPointFaceBoxBox, with the normal out of
    // the face's box, and the depth that of this vertex
    const bool faceOnOne = feature.kind == BoxContactFeature::VertexOfTwoOnFaceOfOne;
    const CollisionBox &face = faceOnOne ? one : two;
    const CollisionBox &vertexBox = faceOnOne ? two : one;
    const unsigned axis = faceOnOne ? feature.oneAxis : feature.twoAxis;
    const unsigned char side = faceOnOne ? feature.oneCorner : feature.twoCorner;
    const unsigned char corner = faceOnOne ? feature.twoCorner : feature.oneCorner;

    const Vector3 vertex = vertexBox.getTransform() * cornerPoint(vertexBox, corner);
    const Vector3 relative = face.getTransform().transformInverse(vertex);
    const real outwards = (side & (1 << axis)) ? 1 : -1;
    const real depth = face.halfSize[axis] - outwards * relative[axis];
    if (depth < -tolerance || depth > 2 * face.halfSize[axis])
      return false;
    for (unsigned i = 0; i < 3; i++)
      if (i != axis && real_abs(relative[i]) > face.halfSize[i] + tolerance)
        return false;

    contact->contactNormal = face.getAxis(axis) * -outwards;
    contact->penetration = depth;
    contact->contactPoint = vertex;
    contact->setBodyData(face.body, vertexBox.body, data.friction, data.restitution);
    return true;
  }

  // An edge on an edge: the normal across both, pointing from box two to
  // box one as in boxAndBox
  const Vector3 oneAxis = one.getAxis(feature.oneAxis), twoAxis = two.getAxis(feature.twoAxis);
  Vector3 normal = oneAxis % twoAxis;
  if (normal.squareMagnitude() < 0.0001)
    return false; // the edges have turned parallel
  normal.normalise();
  if (normal * (two.getAxis(3) - one.getAxis(3)) > 0)
    normal = normal * -1.0f;

  // The closest points of the two edges (as contactPoint() finds them; the
  // axes are unit vectors)
  const Vector3 middleOne =
      one.getTransform() * cornerPoint(one, feature.oneCorner, feature.oneAxis);
  const Vector3 middleTwo =
      two.getTransform() * cornerPoint(two, feature.twoCorner, feature.twoAxis);
  const Vector3 apart = middleOne - middleTwo;
  const real alongOne = oneAxis * apart, alongTwo = twoAxis * apart, cosine = oneAxis * twoAxis;
  const real denominator = 1 - cosine * cosine;
  const real mua = (cosine * alongTwo - alongOne) / denominator;
  const real mub = (alongTwo - cosine * alongOne) / denominator;
  if (real_abs(mua) > one.halfSize[feature.oneAxis] + tolerance ||
      real_abs(mub) > two.halfSize[feature.twoAxis] + tolerance)
    return false; // the edges no longer cross
  const Vector3 nearOne = middleOne + oneAxis * mua, nearTwo = middleTwo + twoAxis * mub;

  // Box two's edge has gone this far into box one
  const real depth = (nearTwo - nearOne) * normal;
  if (depth < -tolerance)
    return false;

  contact->contactNormal = normal;
  contact->penetration = depth;
  contact->contactPoint = nearOne * 0.5 + nearTwo * 0.5;
  contact->setBodyData(one.body, two.body, data.friction, data.restitution);
  return true;
}

// The features (see BoxContactFeature) where a face of one box rests on a
// face of the other, given the feature boxAndBox found for the deepest
// point of that contact: each corner of either face against the other
// face, and each edge of the incident face where it crosses the side of
// the reference face (see BoxContactFeature). Together they are the corners of the area where the
// faces overlap, the points that clipping one face against the other finds (Catto, "Contact
// Manifolds", GDC 2007; Gregorius, "Robust Contact Creation for Physics Simulations", GDC 2015);
// fillKeptContact keeps the ones that are really there.
static std::vector<BoxContactFeature>
facesTouching(const CollisionBox &one, const CollisionBox &two, const BoxContactFeature &deepest) {
  // The reference face, which the deepest corner is on, and the incident
  // face: the face of the other box that most nearly faces it
  const bool referenceIsOne = deepest.kind == BoxContactFeature::VertexOfTwoOnFaceOfOne;
  const CollisionBox &reference = referenceIsOne ? one : two;
  const CollisionBox &incident = referenceIsOne ? two : one;
  const unsigned referenceAxis = referenceIsOne ? deepest.oneAxis : deepest.twoAxis;
  const unsigned char referenceSide =
      (referenceIsOne ? deepest.oneCorner : deepest.twoCorner) & (1 << referenceAxis);
  const Vector3 outOfReference =
      reference.getAxis(referenceAxis) * (referenceSide ? (real)1 : (real)-1);
  unsigned incidentAxis = 0;
  for (unsigned i = 1; i < 3; i++)
    if (real_abs(incident.getAxis(i) * outOfReference) >
        real_abs(incident.getAxis(incidentAxis) * outOfReference))
      incidentAxis = i;
  const unsigned char incidentSide =
      incident.getAxis(incidentAxis) * outOfReference < 0 ? (unsigned char)(1 << incidentAxis) : 0;

  // A feature with the boxes' parts given as reference and incident
  auto vertexOnFace = [&](bool vertexOnIncident, unsigned char corner) {
    BoxContactFeature feature;
    const bool vertexOnTwo = vertexOnIncident == referenceIsOne;
    feature.kind = vertexOnTwo ? BoxContactFeature::VertexOfTwoOnFaceOfOne
                               : BoxContactFeature::VertexOfOneOnFaceOfTwo;
    const unsigned faceAxis = vertexOnIncident ? referenceAxis : incidentAxis;
    const unsigned char faceSide = vertexOnIncident ? referenceSide : incidentSide;
    if (vertexOnTwo) {
      feature.oneAxis = (unsigned char)faceAxis;
      feature.oneCorner = faceSide;
      feature.twoCorner = corner;
    } else {
      feature.twoAxis = (unsigned char)faceAxis;
      feature.twoCorner = faceSide;
      feature.oneCorner = corner;
    }
    return feature;
  };
  auto edgeCrossing = [&](unsigned referenceEdgeAxis, unsigned char referenceEdge,
                          unsigned incidentEdgeAxis, unsigned char incidentEdge) {
    BoxContactFeature feature;
    feature.kind = referenceIsOne ? BoxContactFeature::EdgeCrossingFaceOfOne
                                  : BoxContactFeature::EdgeCrossingFaceOfTwo;
    feature.faceAxis = (unsigned char)referenceAxis;
    feature.oneAxis = (unsigned char)(referenceIsOne ? referenceEdgeAxis : incidentEdgeAxis);
    feature.oneCorner = referenceIsOne ? referenceEdge : incidentEdge;
    feature.twoAxis = (unsigned char)(referenceIsOne ? incidentEdgeAxis : referenceEdgeAxis);
    feature.twoCorner = referenceIsOne ? incidentEdge : referenceEdge;
    return feature;
  };

  // A face's four corners have its side's bit and any of the other two;
  // its four edges run along each of the other two axes, on either side of
  // the third (corner bits with their own axis's bit clear)
  auto corners = [](unsigned axis, unsigned char side) {
    std::vector<unsigned char> found;
    for (unsigned char corner = 0; corner < 8; corner++)
      if ((corner & (1 << axis)) == side)
        found.push_back(corner);
    return found;
  };
  auto edges = [](unsigned axis, unsigned char side) {
    std::vector<std::pair<unsigned, unsigned char>> found;
    for (unsigned along = 0; along < 3; along++) {
      if (along == axis)
        continue;
      const unsigned across = 3 - axis - along;
      for (unsigned char bit : {(unsigned char)0, (unsigned char)(1 << across)})
        found.emplace_back(along, (unsigned char)(side | bit));
    }
    return found;
  };

  std::vector<BoxContactFeature> features;
  for (unsigned char corner : corners(incidentAxis, incidentSide))
    features.push_back(vertexOnFace(true, corner));
  for (unsigned char corner : corners(referenceAxis, referenceSide))
    features.push_back(vertexOnFace(false, corner));
  for (auto [referenceAlong, referenceEdge] : edges(referenceAxis, referenceSide))
    for (auto [incidentAlong, incidentEdge] : edges(incidentAxis, incidentSide))
      features.push_back(edgeCrossing(referenceAlong, referenceEdge, incidentAlong, incidentEdge));
  return features;
}

// A contact found between two boxes, with the parts of the boxes it's
// between
struct FoundContact {
  BoxContactFeature feature;
  Contact contact;
};

// Cuts `found` down to `limit` contacts that best cover the area where the
// boxes touch: the deepest, the one farthest from it, the one making the
// largest triangle with those two, and the one adding the most area outside
// that triangle (Gregorius, "Robust Contact Creation for Physics
// Simulations", GDC 2015), then (if the limit is above four) those farthest
// from the ones already kept.
static void keepCoveringMost(std::vector<FoundContact> &found, unsigned limit) {
  if (found.size() <= limit)
    return;
  auto point = [&](size_t i) { return found[i].contact.contactPoint; };
  auto deepest = std::max_element(found.begin(), found.end(), [](const auto &x, const auto &y) {
    return x.contact.penetration < y.contact.penetration;
  });
  const size_t a = (size_t)(deepest - found.begin());
  const Vector3 normal = found[a].contact.contactNormal;
  auto best = [&](auto score) {
    size_t chosen = a;
    real most = 0;
    for (size_t i = 0; i < found.size(); i++)
      if (score(i) > most) {
        most = score(i);
        chosen = i;
      }
    return chosen;
  };
  // Twice the area of triangle (x, y, z) seen along the normal, signed by
  // its winding
  auto area = [&](const Vector3 &x, const Vector3 &y, const Vector3 &z) {
    return ((y - x) % (z - x)) * normal;
  };
  const size_t b = best([&](size_t i) { return (point(i) - point(a)).squareMagnitude(); });
  const size_t c = best([&](size_t i) { return real_abs(area(point(a), point(b), point(i))); });
  const real winding = area(point(a), point(b), point(c)) < 0 ? (real)-1 : (real)1;
  const size_t d = best([&](size_t i) {
    // How far outside the triangle the point is, as the area it adds
    // beyond the edge it's farthest outside
    const Vector3 corners[3] = {point(a), point(b), point(c)};
    real outside = 0;
    for (int edge = 0; edge < 3; edge++)
      outside =
          std::max(outside, -winding * area(corners[edge], corners[(edge + 1) % 3], point(i)));
    return outside;
  });
  std::vector<size_t> chosen;
  for (size_t i : {a, b, c, d})
    if (std::find(chosen.begin(), chosen.end(), i) == chosen.end())
      chosen.push_back(i);
  while (chosen.size() < limit) {
    const size_t next = best([&](size_t i) {
      real nearest = REAL_MAX;
      for (size_t k : chosen)
        nearest = std::min(nearest, (point(i) - point(k)).squareMagnitude());
      return nearest;
    });
    if (std::find(chosen.begin(), chosen.end(), next) != chosen.end())
      break;
    chosen.push_back(next);
  }
  std::vector<FoundContact> kept;
  kept.reserve(chosen.size());
  for (size_t i : chosen)
    kept.push_back(found[i]);
  found = std::move(kept);
}

unsigned BoxContactCache::boxAndBox(const CollisionBox &one, const CollisionBox &two,
                                    CollisionData *data) {
  if (!data->hasMoreContacts())
    return 0;
  const auto key = std::make_pair(&one, &two);
  std::vector<BoxContactFeature> &kept = pairs[key];
  std::vector<FoundContact> found;

  // The deepest contact now. Where corners tie for deepest, boxAndBox may
  // pick one that isn't over the other box's face (a box resting half off
  // another), so it's only used, and kept, if it is.
  FoundContact deepest;
  CollisionData scratch = *data;
  scratch.contactArray = &deepest.contact;
  scratch.reset(1);
  const bool touching = CollisionDetector::boxAndBox(one, two, &scratch, &deepest.feature) > 0;
  if (touching && (deepest.feature.kind == BoxContactFeature::EdgeOnEdge ||
                   fillKeptContact(one, two, deepest.feature, *data, &deepest.contact)))
    found.push_back(deepest);

  // Those kept from earlier frames, for where the boxes are now
  auto alreadyFound = [&](const BoxContactFeature &feature) {
    return std::any_of(found.begin(), found.end(),
                       [&](const FoundContact &f) { return f.feature == feature; });
  };
  for (const BoxContactFeature &feature : kept) {
    FoundContact now{.feature = feature};
    if (!alreadyFound(feature) && fillKeptContact(one, two, feature, *data, &now.contact))
      found.push_back(now);
  }

  // SAKBox: a corner on a face means the faces may rest on each other: add
  // the rest of the area where they touch now (see facesTouching), rather
  // than finding a corner a frame as the box rocks. A point already found
  // isn't added twice: where the faces' corners meet, each box's corner is
  // on the other's face, and edges cross there too.
  if (touching && deepest.feature.kind != BoxContactFeature::EdgeOnEdge) {
    const real samePoint = (real)0.01 * std::min({one.halfSize.x, one.halfSize.y, one.halfSize.z,
                                                  two.halfSize.x, two.halfSize.y, two.halfSize.z});
    for (const BoxContactFeature &feature : facesTouching(one, two, deepest.feature)) {
      FoundContact now{.feature = feature};
      if (alreadyFound(feature) || !fillKeptContact(one, two, feature, *data, &now.contact))
        continue;
      const bool samePlace = std::any_of(found.begin(), found.end(), [&](const FoundContact &f) {
        // Compared across the face, leaving out the overlap's depth
        Vector3 apart = f.contact.contactPoint - now.contact.contactPoint;
        apart -= now.contact.contactNormal * (apart * now.contact.contactNormal);
        return apart.squareMagnitude() < samePoint * samePoint;
      });
      if (!samePlace)
        found.push_back(now);
    }
  }

  keepCoveringMost(found, maxContactsPerPair);

  // Keep them all for the next frame, and report as many as there's room
  // for now
  kept.clear();
  unsigned added = 0;
  for (const FoundContact &f : found) {
    kept.push_back(f.feature);
    if (data->hasMoreContacts()) {
      *data->contacts = f.contact;
      data->addContacts(1);
      added++;
    }
  }
  if (kept.empty())
    pairs.erase(key);
  return added;
}

void BoxContactCache::forget(const CollisionBox &box) {
  std::erase_if(pairs, [&](const auto &pair) {
    return pair.first.first == &box || pair.first.second == &box;
  });
}

void BoxContactCache::clear() { pairs.clear(); }

unsigned CollisionDetector::boxAndPoint(const CollisionBox &box, const Vector3 &point,
                                        CollisionData *data) {
  // Transform the point into box coordinates
  Vector3 relPt = box.transform.transformInverse(point);

  Vector3 normal;

  // Check each axis, looking for the axis on which the
  // penetration is least deep.
  real min_depth = box.halfSize.x - real_abs(relPt.x);
  if (min_depth < 0)
    return 0;
  normal = box.getAxis(0) * ((relPt.x < 0) ? -1 : 1);

  real depth = box.halfSize.y - real_abs(relPt.y);
  if (depth < 0)
    return 0;
  else if (depth < min_depth) {
    min_depth = depth;
    normal = box.getAxis(1) * ((relPt.y < 0) ? -1 : 1);
  }

  depth = box.halfSize.z - real_abs(relPt.z);
  if (depth < 0)
    return 0;
  else if (depth < min_depth) {
    min_depth = depth;
    normal = box.getAxis(2) * ((relPt.z < 0) ? -1 : 1);
  }

  // Compile the contact
  Contact *contact = data->contacts;
  contact->contactNormal = normal;
  contact->contactPoint = point;
  contact->penetration = min_depth;

  // Note that we don't know what rigid body the point
  // belongs to, so we just use NULL. Where this is called
  // this value can be left, or filled in.
  contact->setBodyData(box.body, nullptr, data->friction, data->restitution);

  data->addContacts(1);
  return 1;
}

unsigned CollisionDetector::boxAndSphere(const CollisionBox &box, const CollisionSphere &sphere,
                                         CollisionData *data) {
  // Transform the centre of the sphere into box coordinates
  Vector3 centre = sphere.getAxis(3);
  Vector3 relCentre = box.transform.transformInverse(centre);

  // Early out check to see if we can exclude the contact
  if (real_abs(relCentre.x) - sphere.radius > box.halfSize.x ||
      real_abs(relCentre.y) - sphere.radius > box.halfSize.y ||
      real_abs(relCentre.z) - sphere.radius > box.halfSize.z) {
    return 0;
  }

  Vector3 closestPt(0, 0, 0);
  real dist;

  // Clamp each coordinate to the box.
  dist = relCentre.x;
  if (dist > box.halfSize.x)
    dist = box.halfSize.x;
  if (dist < -box.halfSize.x)
    dist = -box.halfSize.x;
  closestPt.x = dist;

  dist = relCentre.y;
  if (dist > box.halfSize.y)
    dist = box.halfSize.y;
  if (dist < -box.halfSize.y)
    dist = -box.halfSize.y;
  closestPt.y = dist;

  dist = relCentre.z;
  if (dist > box.halfSize.z)
    dist = box.halfSize.z;
  if (dist < -box.halfSize.z)
    dist = -box.halfSize.z;
  closestPt.z = dist;

  // Check we're in contact
  dist = (closestPt - relCentre).squareMagnitude();
  if (dist > sphere.radius * sphere.radius)
    return 0;

  Contact *contact = data->contacts;
  if (dist > 0) {
    // Compile the contact
    Vector3 closestPtWorld = box.transform.transform(closestPt);
    contact->contactNormal = (closestPtWorld - centre);
    contact->contactNormal.normalise();
    contact->contactPoint = closestPtWorld;
    contact->penetration = sphere.radius - real_sqrt(dist);
  } else {
    // SAKBox: the sphere's centre is inside the box (a fast sphere can get
    // there in one step), so the closest point is the centre itself and
    // gives no normal: it was a zero vector, and resolving the contact
    // turned both bodies' state to NaN. Push the sphere out through the
    // nearest face instead, as Box2D and Bullet do.
    unsigned axis = 0;
    real depth = box.halfSize.x - real_abs(relCentre.x);
    for (unsigned i = 1; i < 3; i++) {
      const real faceDepth = box.halfSize[i] - real_abs(relCentre[i]);
      if (faceDepth < depth) {
        depth = faceDepth;
        axis = i;
      }
    }
    Vector3 outward(0, 0, 0); // the face's outward direction, in box coordinates
    outward[axis] = relCentre[axis] < 0 ? -1 : 1;
    closestPt[axis] = outward[axis] * box.halfSize[axis];
    // (Pointing from the sphere into the box, as for a sphere outside)
    contact->contactNormal = box.transform.transformDirection(outward * -1);
    contact->contactPoint = box.transform.transform(closestPt);
    contact->penetration = sphere.radius + depth;
  }
  contact->setBodyData(box.body, sphere.body, data->friction, data->restitution);

  data->addContacts(1);
  return 1;
}

unsigned CollisionDetector::boxAndHalfSpace(const CollisionBox &box, const CollisionPlane &plane,
                                            CollisionData *data) {
  // Make sure we have contacts
  if (data->contactsLeft <= 0)
    return 0;

  // Check for intersection
  if (!IntersectionTests::boxAndHalfSpace(box, plane)) {
    return 0;
  }

  // We have an intersection, so find the intersection points. We can make
  // do with only checking vertices. If the box is resting on a plane
  // or on an edge, it will be reported as four or two contact points.

  // Go through each combination of + and - for each half-size
  static real mults[8][3] = {{1, 1, 1},  {-1, 1, 1},  {1, -1, 1},  {-1, -1, 1},
                             {1, 1, -1}, {-1, 1, -1}, {1, -1, -1}, {-1, -1, -1}};

  Contact *contact = data->contacts;
  unsigned contactsUsed = 0;
  for (unsigned i = 0; i < 8; i++) {

    // Calculate the position of each vertex
    Vector3 vertexPos(mults[i][0], mults[i][1], mults[i][2]);
    vertexPos.componentProductUpdate(box.halfSize);
    vertexPos = box.transform.transform(vertexPos);

    // Calculate the distance from the plane
    real vertexDistance = vertexPos * plane.direction;

    // Compare this to the plane's distance
    if (vertexDistance <= plane.offset) {
      // Create the contact data.

      // The contact point is halfway between the vertex and the
      // plane - we multiply the direction by half the separation
      // distance and add the vertex location.
      // SAKBox: this added the whole (negative) separation, putting the
      // point as far again below the vertex.
      contact->contactPoint = plane.direction;
      contact->contactPoint *= (plane.offset - vertexDistance) * (real)0.5;
      contact->contactPoint += vertexPos;
      contact->contactNormal = plane.direction;
      contact->penetration = plane.offset - vertexDistance;

      // Write the appropriate data
      contact->setBodyData(box.body, nullptr, data->friction, data->restitution);

      // Move onto the next contact
      contact++;
      contactsUsed++;
      // SAKBox: stop when the array is full, but still record the contacts
      // (this returned early, so they were never counted).
      if (contactsUsed == (unsigned)data->contactsLeft)
        break;
    }
  }

  data->addContacts(contactsUsed);
  return contactsUsed;
}
