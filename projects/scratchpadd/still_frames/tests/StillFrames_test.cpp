// Holding the last frame: never while something moves or changed, and not
// for the frame right after something moved
#include <ScratchPadd/StillFrames/StillFrames.hpp>

#include <gtest/gtest.h>

TEST(StillFramesTest, OneMoreFrameAfterAnythingMoves) {
  ScratchPadd::StillFrames still;
  EXPECT_FALSE(still.canHold(false, false)); // (nothing drawn yet)
  still.drawn(true);                         // (a banner fading)
  EXPECT_FALSE(still.canHold(true, false));
  EXPECT_FALSE(still.canHold(false, false)); // (it stopped: one frame more)
  still.drawn(false);
  EXPECT_TRUE(still.canHold(false, false));
  EXPECT_FALSE(still.canHold(false, true)); // (something new)
}
