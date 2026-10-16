// The ragdoll demo's physics, run without a window, stepped the way the
// demo framework steps it (integrate, generate contacts, resolve).
#include <gtest/gtest.h>

#include <ragdoll_physics.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

struct Outcome {
  cyclone::Vector3 centre;    // mean bone position
  double fastest = 0;         // fastest bone speed, m/s
  double fastestSideways = 0; // fastest horizontal bone speed at any point
};

// Runs a freshly reset ragdoll for `seconds` with fixed steps of `dt`.
Outcome simulate(double dt, unsigned seed, bool push = true,
                 double seconds = 4.0) {
  static constexpr unsigned kMaxContacts = 256;
  auto contacts = std::make_unique<cyclone::Contact[]>(kMaxContacts);
  cyclone::CollisionData cData;
  cData.contactArray = contacts.get();
  cyclone::ContactResolver resolver(kMaxContacts * 8);

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
  for (int i = 0; i < steps; ++i) {
    model->integrate(dt);
    model->generateContacts(cData, kMaxContacts);
    resolver.resolveContacts(cData.contactArray, cData.contactCount, dt);
    for (const ragdoll::Bone &bone : model->bones) {
      const cyclone::Vector3 v = bone.body->getVelocity();
      outcome.fastestSideways =
          std::max<double>(outcome.fastestSideways, std::hypot(v.x, v.z));
    }
  }
  for (const ragdoll::Bone &bone : model->bones) {
    outcome.centre += bone.body->getPosition() * (1.0 / ragdoll::kBones);
    outcome.fastest =
        std::max<double>(outcome.fastest, bone.body->getVelocity().magnitude());
  }
  return outcome;
}

} // namespace

// Without a push the ragdoll collapses where it stands and comes to rest.
TEST(RagdollTest, CollapsesInPlaceWithoutAPush) {
  for (double dt : {1.0 / 60, 0.05}) {
    const Outcome o = simulate(dt, 1, /*push=*/false);
    EXPECT_LT(std::hypot(o.centre.x, o.centre.z), 1.0) << "dt " << dt;
    EXPECT_LT(o.centre.y, 1.0) << "dt " << dt; // lying down
    EXPECT_LT(o.fastest, 0.5) << "dt " << dt;  // at rest
  }
}

// The push knocks it over nearby, the same way at any frame rate. (It used to
// be a force applied for one frame: 0.05 s frames launched the ragdoll at
// 65 m/s and it was still flying 200 m away after 4 s.)
TEST(RagdollTest, PushKnocksItOverAtAnyFrameRate) {
  for (unsigned seed : {1u, 2u, 3u, 4u, 5u}) {
    for (double dt : {1.0 / 60, 0.05}) {
      const Outcome o = simulate(dt, seed);
      EXPECT_LT(o.fastestSideways, 8.0) << "seed " << seed << " dt " << dt;
      EXPECT_LT(std::hypot(o.centre.x, o.centre.z), 10.0)
          << "seed " << seed << " dt " << dt;
      EXPECT_LT(o.centre.y, 1.2) << "seed " << seed << " dt " << dt;
      EXPECT_LT(o.fastest, 1.0) << "seed " << seed << " dt " << dt;
    }
  }
}
