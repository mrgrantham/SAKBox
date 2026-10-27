// The ragdoll demo's physics, run without a window, stepped the way the
// demo framework steps it (integrate, generate contacts, resolve), with
// each of the contact resolver's methods.
#include <gtest/gtest.h>

#include <ragdoll_physics.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

using Method = cyclone::ContactResolver::Method;

struct Outcome {
  cyclone::Vector3 centre;    // mean bone position
  double fastest = 0;         // fastest bone speed at the end, m/s
  double fastestSideways = 0; // fastest horizontal bone speed at any point
  double restingSpeed = 0;    // the fastest bone's speed, averaged over the last second
  double meanStretch = 0;     // how far joints are beyond their slack, on average
  double worstStretch = 0;    // the most any joint was beyond its slack, after any step
};

// Runs a freshly reset ragdoll for `seconds` with fixed steps of `dt`, each
// split into `subSteps` (as the demo framework does; see FixedSteps).
Outcome simulate(Method method, double dt, unsigned seed, bool push = true, double seconds = 4.0,
                 unsigned subSteps = 1) {
  static constexpr unsigned kMaxContacts = 256;
  auto contacts = std::make_unique<cyclone::Contact[]>(kMaxContacts);
  cyclone::CollisionData cData;
  cData.contactArray = contacts.get();
  cyclone::ContactResolver resolver(kMaxContacts * 8);
  resolver.setMethod(method);

  auto model = std::make_unique<ragdoll::Ragdoll>();
  cyclone::Random random(seed);
  model->reset(random);
  if (!push) {
    for (ragdoll::Bone &bone : model->bones) {
      bone.body->setVelocity(cyclone::Vector3());
      bone.body->setRotation(cyclone::Vector3());
    }
  }

  Outcome outcome;
  const int steps = static_cast<int>(seconds / dt);
  const int lastSecond = static_cast<int>(1.0 / dt);
  double stretchTotal = 0;
  for (int i = 0; i < steps; ++i) {
    const double subStep = dt / subSteps;
    for (unsigned j = 0; j < subSteps; ++j) {
      model->integrate(subStep);
      model->generateContacts(cData, kMaxContacts);
      resolver.resolveContacts(cData.contactArray, cData.contactCount, subStep, model->jointList());
    }
    outcome.worstStretch = std::max(outcome.worstStretch, model->worstStretch());
    double fastestNow = 0;
    for (const ragdoll::Bone &bone : model->bones) {
      const cyclone::Vector3 v = bone.body->getVelocity();
      outcome.fastestSideways = std::max<double>(outcome.fastestSideways, std::hypot(v.x, v.z));
      fastestNow = std::max<double>(fastestNow, v.magnitude());
    }
    if (i >= steps - lastSecond)
      outcome.restingSpeed += fastestNow / lastSecond;
    for (const cyclone::Joint &joint : model->joints) {
      const double length = (joint.body[1]->getPointInWorldSpace(joint.position[1]) -
                             joint.body[0]->getPointInWorldSpace(joint.position[0]))
                                .magnitude();
      stretchTotal += std::max<double>(0.0, length - joint.error);
    }
  }
  outcome.meanStretch = stretchTotal / (steps * ragdoll::kJoints);
  for (const ragdoll::Bone &bone : model->bones) {
    outcome.centre += bone.body->getPosition() * (1.0 / ragdoll::kBones);
    outcome.fastest = std::max<double>(outcome.fastest, bone.body->getVelocity().magnitude());
  }
  return outcome;
}

} // namespace

// Without a push the ragdoll collapses where it stands and comes to rest.
// With the book's resolver it's down within 4 s, but never quite still: it
// keeps jittering at a few cm/s. Resolved together, friction holds it up a
// little longer (it's still finishing its fall at 4 s), and then it's
// still.
TEST(RagdollTest, CollapsesInPlaceWithoutAPush) {
  for (double dt : {1.0 / 60, 0.05}) {
    SCOPED_TRACE(testing::Message() << "dt " << dt);
    const Outcome book = simulate(Method::oneContactAtATime, dt, 1, /*push=*/false);
    EXPECT_LT(std::hypot(book.centre.x, book.centre.z), 1.0);
    EXPECT_LT(book.centre.y, 1.0); // lying down
    EXPECT_LT(book.fastest, 0.5);  // at rest, near enough

    const Outcome together = simulate(Method::sequentialImpulses, dt, 1, /*push=*/false, 10.0);
    EXPECT_LT(std::hypot(together.centre.x, together.centre.z), 1.0);
    EXPECT_LT(together.centre.y, 1.0);
    EXPECT_LT(together.restingSpeed, 0.2);
  }
}

// The push knocks it over nearby, the same way at any frame rate. (It used to
// be a force applied for one frame: 0.05 s frames launched the ragdoll at
// 65 m/s and it was still flying 200 m away after 4 s.)
TEST(RagdollTest, PushKnocksItOverAtAnyFrameRate) {
  for (Method method : {Method::oneContactAtATime, Method::sequentialImpulses}) {
    for (unsigned seed : {1u, 2u, 3u, 4u, 5u}) {
      for (double dt : {1.0 / 60, 0.05}) {
        SCOPED_TRACE(testing::Message()
                     << "method " << (int)method << " seed " << seed << " dt " << dt);
        const Outcome o = simulate(method, dt, seed);
        EXPECT_LT(o.fastestSideways, 8.0);
        EXPECT_LT(std::hypot(o.centre.x, o.centre.z), 10.0);
        EXPECT_LT(o.centre.y, 1.2);
        EXPECT_LT(o.fastest, 1.0);
      }
    }
  }
}

// Resolving contacts together, with the joints as constraints between their
// anchor points, a knocked-over ragdoll comes to rest more fully than with
// the book's resolver: over 20 pushes, the fastest bone's speed in the
// tenth second averages about 2 cm/s at 60 steps a second and 3 cm/s at
// 20, against about 3 and 9 cm/s. Its joints stretch beyond their slack by
// 1 mm (1 cm at 20 steps a second) on average.
TEST(RagdollTest, ComesToRestWhenResolvedTogether) {
  for (double dt : {1.0 / 60, 0.05}) {
    SCOPED_TRACE(testing::Message() << "dt " << dt);
    double together = 0, book = 0, togetherStretch = 0;
    for (unsigned seed = 1; seed <= 20; ++seed) {
      const Outcome t = simulate(Method::sequentialImpulses, dt, seed, true, 10.0);
      EXPECT_LT(t.restingSpeed, 0.2) << "seed " << seed;
      together += t.restingSpeed / 20;
      togetherStretch += t.meanStretch / 20;
      book += simulate(Method::oneContactAtATime, dt, seed, true, 10.0).restingSpeed / 20;
    }
    EXPECT_LT(together, 0.04);
    EXPECT_GT(book, 1.5 * together);
    EXPECT_LT(togetherStretch, 0.015); // m
  }
}

// Sub-steps: smaller steps within each step stretch the joints far less
// when the ragdoll hits the ground. Over 20 pushes, the worst any joint
// stretched beyond its slack:
//
//   steps a second   1 sub-step   2       4       8
//   60               6.7 cm       0.9 cm  0.03 cm 0.003 cm
//   20               62 cm        21 cm   2.6 cm  0.3 cm
//
// Two, the demos' default (FixedSteps::kDefaultSubSteps), already cut it
// several times. And it still comes to rest.
TEST(RagdollTest, SubStepsStretchJointsLess) {
  for (double dt : {1.0 / 60, 0.05}) {
    SCOPED_TRACE(testing::Message() << "dt " << dt);
    double worst[3] = {0, 0, 0}; // with 1, 4 and 2 sub-steps
    for (unsigned seed = 1; seed <= 20; ++seed) {
      worst[0] = std::max(worst[0], simulate(Method::sequentialImpulses, dt, seed).worstStretch);
      worst[2] = std::max(
          worst[2], simulate(Method::sequentialImpulses, dt, seed, true, 4.0, 2).worstStretch);
      const Outcome o = simulate(Method::sequentialImpulses, dt, seed, true, 10.0, 4);
      worst[1] = std::max(worst[1], o.worstStretch);
      EXPECT_LT(o.restingSpeed, 0.2) << "seed " << seed;
    }
    EXPECT_LT(worst[1], worst[0] / 10);
    EXPECT_LT(worst[2], worst[0] / 2.5);
  }
}
