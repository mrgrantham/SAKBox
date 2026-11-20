// Names at the table: the person's own (or "You"), the bots' stars, all
// different, the same for the same seed
#include <CosmicHex/Handheld/PlayerNames.hpp>

#include <gtest/gtest.h>

#include <set>

using CosmicHex::Handheld::PlayerNames;

TEST(PlayerNamesTest, YoursAndTheBotsAllDifferent) {
  const PlayerNames names = PlayerNames::forGame(4, 0, "Vega", 7);
  EXPECT_EQ(names[0], "Vega");
  std::set<std::string> all;
  for (int p = 0; p < 4; ++p) {
    EXPECT_FALSE(names[p].empty());
    all.insert(names[p]);
  }
  EXPECT_EQ(all.size(), 4u); // (no bot took "Vega")
  EXPECT_EQ(PlayerNames::forGame(4, 0, "", 7)[0], "You");
  EXPECT_EQ(PlayerNames::forGame(4, -1, "", 7)[0], PlayerNames::forGame(4, -1, "", 7)[0]);
  EXPECT_EQ(names.upperOf(0), "VEGA");
  EXPECT_EQ(PlayerNames::forGame(2, 0, "Bartholomew", 1).shortOf(0), "Barthol");
  EXPECT_EQ(names[9], "");
}
