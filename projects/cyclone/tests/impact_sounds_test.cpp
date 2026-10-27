// SAKBox: the rigid-body demos' knocks (src/demos/impact_sounds.h), and the
// demos' sounds (src/demos/demo_sounds.h), heard through an OfflineOutput,
// so the tests hear exactly what plays.
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
  // One step's contacts
  void hearStep(ImpactSounds &sounds, float seconds) {
    std::vector<cyclone::Contact> all(contacts_.begin(), contacts_.end());
    sounds.hear(all, seconds);
  }
  // A frame of one step
  void hear(ImpactSounds &sounds, float seconds = 1.0f / 60) {
    hearStep(sounds, seconds);
    sounds.playHits();
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
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  ASSERT_TRUE(demoSounds.playing()) << demoSounds.error();
  Scene resting;
  resting.body(0);
  resting.body(ImpactKnocks::kQuietestHit * 0.5f);
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
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  Scene scene;
  scene.body(5);
  scene.hear(sounds);
  scene.hear(sounds); // the same hit, the next frame
  EXPECT_EQ(sounds.knocks(), 1u);
  scene.hear(sounds, ImpactKnocks::kRepeatSeconds);
  EXPECT_EQ(sounds.knocks(), 2u);
}

// A crowd of hits in one frame: only the hardest few knock
TEST(ImpactSoundsTest, AFewHitsAFrame) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  Scene scene;
  for (int i = 0; i < 20; ++i)
    scene.body(2.0f + i * 0.2f, static_cast<float>(i));
  scene.hear(sounds);
  EXPECT_EQ(sounds.knocks(), static_cast<std::uint32_t>(ImpactKnocks::kHitsPerFrame));
}

// A frame of several steps (sub-steps, or catching up): a pair hit in more
// than one knocks once, and only the hardest few of all their hits knock
TEST(ImpactSoundsTest, AFrameOfSeveralSteps) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  Scene one;
  one.body(5);
  for (int step = 0; step < 4; ++step)
    one.hearStep(sounds, 1.0f / 240);
  EXPECT_EQ(sounds.knocks(), 0u); // not until the frame's done
  sounds.playHits();
  EXPECT_EQ(sounds.knocks(), 1u);

  ScratchPadd::Audio::OfflineOutput crowdedOutput;
  DemoSounds crowdedSounds(crowdedOutput);
  ImpactSounds crowded(crowdedSounds);
  Scene crowd;
  for (int i = 0; i < 20; ++i)
    crowd.body(2.0f + i * 0.2f, static_cast<float>(i));
  for (int step = 0; step < 4; ++step)
    crowd.hearStep(crowded, 1.0f / 240);
  crowded.playHits();
  EXPECT_EQ(crowded.knocks(), static_cast<std::uint32_t>(ImpactKnocks::kHitsPerFrame));
}

// Turned off (V in the demos), nothing knocks
TEST(ImpactSoundsTest, TurnedOff) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  demoSounds.setEnabled(false);
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
    DemoSounds demoSounds(output);
    ImpactSounds sounds(demoSounds);
    sounds.setCameraAngle(angle);
    Scene scene;
    scene.body(8, 0, angle == 0 ? -8.0f : 8.0f);
    scene.hear(sounds);
    const auto [left, right] = loudest(output, 4800);
    EXPECT_GT(right, 2 * left) << "camera at " << angle;
  }
}

// A loop plays until stopped; off, it's silent, and on again it comes back
TEST(DemoSoundsTest, LoopsPlayUntilStoppedAndQuietWhileOff) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds sounds(output);
  ASSERT_TRUE(sounds.playing()) << sounds.error();
  sounds.setLoop("engine", {.volume = 0.5f});
  sounds.update();
  // Two seconds in, still playing (the loop is 1 s long)
  loudest(output, 96000);
  EXPECT_GT(loudest(output, 4800).first, 0.05f);
  sounds.setEnabled(false);
  sounds.update();
  loudest(output, 4800); // (the engine ramps the volume down)
  EXPECT_LT(loudest(output, 4800).first, 0.001f);
  sounds.setEnabled(true);
  sounds.update();
  loudest(output, 4800);
  EXPECT_GT(loudest(output, 4800).first, 0.05f);
  sounds.stopLoop("engine");
  sounds.update();
  loudest(output, 4800);
  EXPECT_EQ(loudest(output, 4800).first, 0.0f);
  EXPECT_EQ(sounds.played(), 1u); // started once
}

// Off, nothing plays once either
TEST(DemoSoundsTest, NothingPlaysWhileOff) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds sounds(output);
  sounds.setEnabled(false);
  EXPECT_FALSE(sounds.play("shot"));
  EXPECT_EQ(sounds.played(), 0u);
  sounds.setEnabled(true);
  EXPECT_TRUE(sounds.play("shot"));
  EXPECT_GT(loudest(output, 4800).first, 0.05f);
}

// Particles knock too: one hitting the ground, or two closing along a
// cable snapping taut; one resting on the ground doesn't
TEST(ImpactSoundsTest, ParticlesKnock) {
  ScratchPadd::Audio::OfflineOutput output;
  DemoSounds demoSounds(output);
  ImpactSounds sounds(demoSounds);
  std::deque<cyclone::Particle> particles(4);
  for (cyclone::Particle &particle : particles)
    particle.setMass(1);
  particles[0].setVelocity(0, -5, 0); // hitting the ground
  particles[1].setVelocity(0, 0, 0);  // resting on it
  particles[2].setVelocity(-3, 0, 0); // pulling away from particles[3]
  particles[3].setVelocity(0, 0, 0);
  std::vector<cyclone::ParticleContact> contacts(3);
  contacts[0].particle[0] = &particles[0];
  contacts[0].particle[1] = nullptr;
  contacts[0].contactNormal = cyclone::Vector3(0, 1, 0);
  contacts[1].particle[0] = &particles[1];
  contacts[1].particle[1] = nullptr;
  contacts[1].contactNormal = cyclone::Vector3(0, 1, 0);
  // A cable pulls particle 2 back towards particle 3 (+x)
  contacts[2].particle[0] = &particles[2];
  contacts[2].particle[1] = &particles[3];
  contacts[2].contactNormal = cyclone::Vector3(1, 0, 0);
  sounds.hear(std::span<const cyclone::ParticleContact>(contacts), 1.0f / 60);
  sounds.playHits();
  EXPECT_EQ(sounds.knocks(), 2u); // the hit and the cable, not the resting one
}
