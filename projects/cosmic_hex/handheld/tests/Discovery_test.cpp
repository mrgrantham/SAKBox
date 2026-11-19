// A tile turned over shown big: which tiles, their words, and the show's
// timeline (the bottom screen's tile only after the top's is back in place)
#include <CosmicHex/Handheld/Discovery.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(DiscoveryTest, APlanetTurnedOverWithItsWords) {
  const State before = newGame(4, agreedRules(boardR3()));
  State after = before;
  std::size_t i = 0;
  while (i < after.planets.size() && (after.planets[i].flipped || after.planets[i].homeOf >= 0))
    ++i;
  ASSERT_LT(i, after.planets.size());
  after.planets[i].flipped = true;
  after.planets[i].kind = PlanetKind::GasGiant;
  after.planets[i].number = 6;
  const auto found = Discovery::between(before, after);
  ASSERT_EQ(found.size(), 1u);
  EXPECT_TRUE(found[0].planet);
  EXPECT_EQ(found[0].hex, after.planets[i].hex);
  EXPECT_EQ(found[0].face, Art::Face::PlanetFuel);
  EXPECT_EQ(found[0].number, 6);
  EXPECT_EQ(found[0].name, "Gas giant");
  EXPECT_EQ(found[0].gives, "Fuel on a 6");
  EXPECT_TRUE(Discovery::between(after, after).empty());
}

TEST(DiscoveryTest, AFindTurnedOver) {
  const State before = newGame(4, agreedRules(boardR3()));
  ASSERT_FALSE(before.zone.empty());
  State after = before;
  after.zone[0].flipped = true;
  after.zone[0].find = Find::Ruins;
  const auto found = Discovery::between(before, after);
  ASSERT_EQ(found.size(), 1u);
  EXPECT_FALSE(found[0].planet);
  EXPECT_EQ(found[0].number, 0);
  EXPECT_EQ(found[0].name, "Ancient ruins");
  EXPECT_EQ(found[0].gives, "+" + std::to_string(after.settings.ruinsBadges + 1) + " badges");
}

// The parts in order: lifted, turned, the token dropped, held, back down
// flat; only then does the bottom screen's tile fade in
TEST(DiscoveryTest, TheBottomScreenWaitsForTheTop) {
  using namespace Discovery;
  EXPECT_FLOAT_EQ(at(0).lifted(), 0);
  EXPECT_FLOAT_EQ(at(kSecondsUp).lifted(), 1);
  EXPECT_FLOAT_EQ(at(kSecondsUp).turn, 0); // (face down while it lifts)
  EXPECT_FLOAT_EQ(at(kSecondsUp + kSecondsTurn).turn, 1);
  EXPECT_FLOAT_EQ(at(kSecondsUp + kSecondsTurn).chip, 0);
  const float downEnds = kSeconds - kSecondsFade;
  EXPECT_FLOAT_EQ(at(downEnds - kSecondsDown).lifted(), 1);
  for (float s = 0; s < downEnds; s += 0.05f) {
    EXPECT_FLOAT_EQ(at(s).fade, 0) << s;
  }
  EXPECT_NEAR(at(downEnds).lifted(), 0, 1e-5f); // (flat in its place)
  EXPECT_GT(at(downEnds + kSecondsFade / 2).fade, 0);
  EXPECT_FLOAT_EQ(at(kSeconds).fade, 1);
  EXPECT_FALSE(at(kSeconds - 0.01f).done);
  EXPECT_TRUE(at(kSeconds).done);
}
