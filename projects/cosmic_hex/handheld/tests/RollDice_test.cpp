// A roll's dice: thrown, stepped on the clock until they settle, shown, the
// faces handed back once, the glass pulled away; skipping a stage at a time
#include <CosmicHex/Handheld/RollDice.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex::Handheld;

TEST(RollDiceTest, ThrownShownSentAndPulled) {
  RollDice dice(1.0 / 60, 4);
  EXPECT_TRUE(dice.idle());
  dice.throwFrom(42, 0);
  EXPECT_EQ(dice.stage(), RollDice::Stage::Rolling);
  std::optional<std::pair<int, int>> sent;
  int times = 0;
  double now = 0;
  for (; now < 30 && !dice.idle(); now += 1.0 / 60)
    if (const auto faces = dice.update(now)) {
      sent = faces;
      ++times;
    }
  EXPECT_TRUE(dice.idle());
  ASSERT_TRUE(sent.has_value());
  EXPECT_EQ(times, 1); // (once)
  EXPECT_EQ(*sent, dice.rolled());
  EXPECT_GE(sent->first, 1);
  EXPECT_LE(sent->second, 6);
}

TEST(RollDiceTest, SkippingGoesAStageAtATime) {
  RollDice dice(1.0 / 60, 4);
  dice.throwFrom(7, 0);
  dice.skip(0); // (on to where they land, the roll shown)
  EXPECT_EQ(dice.stage(), RollDice::Stage::Showing);
  dice.skip(0); // (shown long enough: sent next frame)
  const auto faces = dice.update(1.0 / 60);
  ASSERT_TRUE(faces.has_value());
  EXPECT_EQ(dice.stage(), RollDice::Stage::Pulling);
  dice.skip(0);
  EXPECT_TRUE(dice.idle());
}
