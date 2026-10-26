// SAKBox: the rigid-body demos' knocks (src/demos/impact_sounds.h), heard
// through an OfflineOutput, so the tests hear exactly what plays.
#include "demos/impact_sounds.h"

#include <gtest/gtest.h>

#include <ScratchPadd/Audio/Output.hpp>

#include <cmath>
#include <deque>
#include <vector>

namespace {

// Bodies dropping onto the ground (or resting on it)
class Scene {
public:
  // A 1 kg body at `x, z`, moving down at `speed`, touching the ground
  cyclone::Contact &body(float speed, float x = 0, float z = 0, float mass = 1) {
    cyclone::RigidBody &body = bodies_.emplace_back();
    body.setMass(mass);
    body.setPosition(x, 0.5f, z);
    body.setVelocity(0, -speed, 0);
    body.setRotation(0, 0, 0);
    cyclone::Contact &contact = contacts_.emplace_back();
    contact.setBodyData(&body, nullptr, 0.9f, 0.1f);
    contact.contactPoint = cyclone::Vector3(x, 0, z);
    contact.contactNormal = cyclone::Vector3(0, 1, 0);
    return contact;
  }
  void hear(ImpactSounds &sounds, float seconds = 1.0f / 60) {
    std::vector<cyclone::Contact> all(contacts_.begin(), contacts_.end());
    sounds.hear(all.data(), static_cast<unsigned>(all.size()), seconds);
  }

private:
  std::deque<cyclone::RigidBody> bodies_; // (a deque: they don't move as it grows)
  std::deque<cyclone::Contact> contacts_;
};

// The loudest the left and the right are over the next `frames` frames
std::pair<float, float> loudest(ScratchPadd::Audio::OfflineOutput &output, std::uint32_t frames) {
  std::vector<float> out(2 * frames);
  output.render(out.data(), frames);
  float left = 0, right = 0;
  for (std::uint32_t i = 0; i < frames; ++i) {
    left = std::max(left, std::abs(out[2 * i]));
    right = std::max(right, std::abs(out[2 * i + 1]));
  }
  return {left, right};
}

} // namespace

// A body hitting the ground knocks; one resting on it, or touching it
// slowly, doesn't
TEST(ImpactSoundsTest, HitsKnockAndRestingDoesnt) {
  ScratchPadd::Audio::OfflineOutput output;
  ImpactSounds sounds(output);
  ASSERT_TRUE(sounds.playing()) << sounds.error();
  Scene resting;
  resting.body(0);
  resting.body(ImpactSounds::kQuietestHit * 0.5f);
  resting.hear(sounds);
  EXPECT_EQ(sounds.knocks(), 0u);
  Scene hit;
  hit.body(5);
  hit.hear(sounds);
  EXPECT_EQ(sounds.knocks(), 1u);
  EXPECT_GT(loudest(output, 4800).first, 0.05f);
}

// A pair knocks at most once every kRepeatSeconds, however many frames
// (and contacts) its hit lasts
TEST(ImpactSoundsTest, APairKnocksOnceAWhile) {
  ScratchPadd::Audio::OfflineOutput output;
  ImpactSounds sounds(output);
  Scene scene;
  scene.body(5);
  scene.hear(sounds);
  scene.hear(sounds); // the same hit, the next frame
  EXPECT_EQ(sounds.knocks(), 1u);
  scene.hear(sounds, ImpactSounds::kRepeatSeconds);
  EXPECT_EQ(sounds.knocks(), 2u);
}

// A crowd of hits in one frame: only the hardest few knock
TEST(ImpactSoundsTest, AFewHitsAFrame) {
  ScratchPadd::Audio::OfflineOutput output;
  ImpactSounds sounds(output);
  Scene scene;
  for (int i = 0; i < 20; ++i)
    scene.body(2.0f + i * 0.2f, static_cast<float>(i));
  scene.hear(sounds);
  EXPECT_EQ(sounds.knocks(), static_cast<std::uint32_t>(ImpactSounds::kHitsPerFrame));
}

// Turned off (V in the demos), nothing knocks
TEST(ImpactSoundsTest, TurnedOff) {
  ScratchPadd::Audio::OfflineOutput output;
  ImpactSounds sounds(output);
  sounds.setEnabled(false);
  Scene scene;
  scene.body(8);
  scene.hear(sounds);
  sounds.blast(cyclone::Vector3(0, 0, 0));
  EXPECT_EQ(sounds.knocks(), 0u);
  EXPECT_EQ(loudest(output, 4800).first, 0.0f);
}

// A hit on the right of the screen sounds on the right: with the camera's
// angle 0 the demos look along -x, so the right is -z; turned 180 degrees
// around, it's +z
TEST(ImpactSoundsTest, PannedToWhereItHappened) {
  for (const float angle : {0.0f, 180.0f}) {
    ScratchPadd::Audio::OfflineOutput output;
    ImpactSounds sounds(output);
    sounds.setCameraAngle(angle);
    Scene scene;
    scene.body(8, 0, angle == 0 ? -8.0f : 8.0f);
    scene.hear(sounds);
    const auto [left, right] = loudest(output, 4800);
    EXPECT_GT(right, 2 * left) << "camera at " << angle;
  }
}
