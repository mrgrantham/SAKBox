// The window's blending of the physics padd's snapshots, without a window
#include "SnapshotTimeline.hpp"

#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace CyclonePadds;

namespace {

// A one-bone snapshot at physics time `time`, the bone at x = `x` turned
// `degrees` about the vertical
RagdollSnapshot at(double time, float x, float degrees = 0, std::uint32_t generation = 1) {
  RagdollSnapshot s;
  s.time = time;
  s.generation = generation;
  s.bones.push_back({.position = glm::vec3(x, 0, 0),
                     .orientation = glm::angleAxis(glm::radians(degrees), glm::vec3(0, 1, 0))});
  return s;
}

float degreesOf(const glm::quat &q) { return glm::degrees(glm::angle(q)); }

} // namespace

// Halfway between two snapshots: halfway along, and turned halfway
TEST(SnapshotTimelineTest, BlendsPositionAndTurn) {
  const auto bones = SnapshotTimeline::blended(at(0, 0, 0).bones, at(1, 2, 90).bones, 0.5f);
  ASSERT_EQ(bones.size(), 1u);
  EXPECT_FLOAT_EQ(bones[0].position.x, 1.0f);
  EXPECT_NEAR(degreesOf(bones[0].orientation), 45.0f, 0.01f);
}

// Drawn `delay` behind the physics, blended between the snapshots either
// side of that time
TEST(SnapshotTimelineTest, DrawsADelayBehindBlended) {
  SnapshotTimeline timeline;
  timeline.setDelay(0.05);
  for (int i = 0; i <= 2; ++i) {
    timeline.add(at(0.1 * i, static_cast<float>(i)), 0.1 * i); // arriving as they're made
  }
  EXPECT_NEAR(timeline.drawTime(0.2), 0.15, 1e-9);
  EXPECT_NEAR(timeline.bonesAt(0.2, true)[0].position.x, 1.5f, 1e-4);
  EXPECT_NEAR(timeline.bonesAt(0.17, true)[0].position.x, 1.2f, 1e-4);
}

// Outside the snapshots it holds the oldest or newest: it never guesses
// ahead of the physics
TEST(SnapshotTimelineTest, HoldsOutsideTheSnapshots) {
  SnapshotTimeline timeline;
  timeline.setDelay(0.05);
  timeline.add(at(0.0, 0), 0.0);
  timeline.add(at(0.1, 1), 0.1);
  EXPECT_FLOAT_EQ(timeline.bonesAt(0.5, true)[0].position.x, 1.0f);  // long after
  EXPECT_FLOAT_EQ(timeline.bonesAt(-0.5, true)[0].position.x, 0.0f); // before
}

// Not blending draws the newest as it is
TEST(SnapshotTimelineTest, NotBlendingDrawsTheNewest) {
  SnapshotTimeline timeline;
  timeline.add(at(0.0, 0), 0.0);
  timeline.add(at(0.1, 1), 0.1);
  EXPECT_FLOAT_EQ(timeline.bonesAt(0.1, false)[0].position.x, 1.0f);
}

// A reset (a new generation) isn't blended with the ragdoll before it
TEST(SnapshotTimelineTest, AResetIsntBlended) {
  SnapshotTimeline timeline;
  timeline.setDelay(0.05);
  timeline.add(at(0.0, 0, 0, 1), 0.0);
  timeline.add(at(0.1, 5, 0, 2), 0.1);
  EXPECT_FLOAT_EQ(timeline.bonesAt(0.1, true)[0].position.x, 5.0f);
}

// Snapshots arriving a little early or late move the drawing time only a
// little; after a pause (the physics' time far behind) it catches up at once
TEST(SnapshotTimelineTest, EasesJitterAndCatchesUpAfterAPause) {
  SnapshotTimeline timeline;
  timeline.setDelay(0);
  timeline.add(at(0.0, 0), 0.0);
  timeline.add(at(0.1, 1), 0.11); // 10 ms late
  EXPECT_NEAR(timeline.drawTime(0.11), 0.11 - SnapshotTimeline::kOffsetEasing * 0.01, 1e-9);

  timeline.add(at(0.2, 2), 3.0); // paused for about 3 s
  EXPECT_NEAR(timeline.drawTime(3.0), 0.2, 1e-9);
}
