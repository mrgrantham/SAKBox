// The simulator's defaults play the rules as agreed: the batch's settings
// for R3 with no options changed are the rules library's agreedRules()
#include "../../simulate/Batch.hpp"

#include <CosmicHex/Rules.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;

TEST(AgreedRulesTest, TheSimulatorsDefaultsAreTheAgreedRules) {
  Batch::Options options;
  options.layout = "R3";
  options.layoutsFile = "projects/cosmic_hex/layouts/layouts.txt";
  ASSERT_FALSE(Batch::readLayout(options).has_value());
  const Settings simulated = Batch::settingsFor(options);
  const Settings agreed = agreedRules(boardR3());
  EXPECT_TRUE(simulated == agreed);
  // (a readable difference, if not: the settings' line of a new game's state)
  const auto settingsLine = [](const Settings &settings) {
    const std::string text = Game::writeState(newGame(4, settings));
    const std::size_t at = text.find("settings ");
    return text.substr(at, text.find('\n', at) - at);
  };
  EXPECT_EQ(settingsLine(simulated), settingsLine(agreed));
}
