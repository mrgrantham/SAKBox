// ViewHistory (Rewind): views become steps once still, back and forward
// step through them, nothing's lost going back then moving, and the strip
// scrubs between steps
#include <AnimationLab/ViewHistory.hpp>

#include <gtest/gtest.h>

using AnimationLab::OrbitView;
using AnimationLab::ViewHistory;

namespace {
OrbitView::Pose turnedTo(float yaw) {
  OrbitView::Pose pose;
  pose.yaw = yaw;
  return pose;
}
// The view held still long enough to be a step
void holdStill(ViewHistory &history, const OrbitView::Pose &pose) {
  for (int i = 0; i < 30; ++i)
    history.watch(pose, 1.0f / 60, false);
}
} // namespace

// The first view is a step at once; a new one only after it's been still
// a moment, and not while it's still moving
TEST(ViewHistoryTest, AViewStillAMomentIsAStep) {
  ViewHistory history;
  history.watch(turnedTo(0), 1.0f / 60, false);
  EXPECT_EQ(history.count(), 1u);
  history.watch(turnedTo(0.5f), 1.0f / 60, false);
  history.watch(turnedTo(0.5f), ViewHistory::kStillSeconds / 2, false);
  EXPECT_EQ(history.count(), 1u); // (not still long enough yet)
  for (int i = 0; i < 30; ++i)
    history.watch(turnedTo(1), 1.0f / 60, true); // (held: a drag)
  EXPECT_EQ(history.count(), 1u);
  holdStill(history, turnedTo(1));
  EXPECT_EQ(history.count(), 2u);
  EXPECT_EQ(history.at(), 1u);
}

// Back and forward step through the views, and stop at the ends
TEST(ViewHistoryTest, BackAndForward) {
  ViewHistory history;
  for (float yaw : {0.0f, 1.0f, 2.0f})
    holdStill(history, turnedTo(yaw));
  ASSERT_EQ(history.count(), 3u);
  EXPECT_FLOAT_EQ(history.back(turnedTo(2))->yaw, 1);
  EXPECT_FLOAT_EQ(history.back(turnedTo(1))->yaw, 0);
  EXPECT_FALSE(history.back(turnedTo(0)).has_value());
  EXPECT_FLOAT_EQ(history.forward(turnedTo(0))->yaw, 1);
  EXPECT_FLOAT_EQ(history.forward(turnedTo(1))->yaw, 2);
  EXPECT_FALSE(history.forward(turnedTo(2)).has_value());
}

// A view moved to but not yet still is made a step when going back, so
// forward returns to it
TEST(ViewHistoryTest, BackKeepsAViewNotYetStill) {
  ViewHistory history;
  holdStill(history, turnedTo(0));
  history.watch(turnedTo(1), 1.0f / 60, false); // (just moved)
  EXPECT_FLOAT_EQ(history.back(turnedTo(1))->yaw, 0);
  EXPECT_FLOAT_EQ(history.forward(turnedTo(0))->yaw, 1);
}

// Going back, then moving: the new view is added at the end, and the ones
// gone back past are kept
TEST(ViewHistoryTest, NothingIsLostMovingAfterGoingBack) {
  ViewHistory history;
  for (float yaw : {0.0f, 1.0f, 2.0f})
    holdStill(history, turnedTo(yaw));
  history.back(turnedTo(2));
  history.back(turnedTo(1));
  holdStill(history, turnedTo(0)); // (arrived: already a step, nothing new)
  EXPECT_EQ(history.count(), 3u);
  holdStill(history, turnedTo(-1));
  ASSERT_EQ(history.count(), 4u);
  EXPECT_EQ(history.at(), 3u);
  EXPECT_FLOAT_EQ(history.step(2).yaw, 2);
}

// The strip: a place between two steps is a view between them; letting go
// makes the nearest step the one you're at
TEST(ViewHistoryTest, ScrubbingBetweenSteps) {
  ViewHistory history;
  for (float yaw : {0.0f, 1.0f, 2.0f})
    holdStill(history, turnedTo(yaw));
  EXPECT_NEAR(history.along(0.5f).yaw, 0.5f, 1e-5f);
  EXPECT_NEAR(history.along(1.75f).yaw, 1.75f, 1e-5f);
  EXPECT_FLOAT_EQ(history.along(99).yaw, 2);
  history.stopAt(0.8f);
  EXPECT_EQ(history.at(), 1u);
}

// The oldest steps go once there are too many
TEST(ViewHistoryTest, KeepsTheNewestSteps) {
  ViewHistory history;
  for (std::size_t i = 0; i < ViewHistory::kMostSteps + 10; ++i)
    holdStill(history, turnedTo(static_cast<float>(i) * 0.01f));
  EXPECT_EQ(history.count(), ViewHistory::kMostSteps);
  EXPECT_NEAR(history.step(0).yaw, 0.10f, 1e-5f);
}
