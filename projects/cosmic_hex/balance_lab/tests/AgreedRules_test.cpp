// The simulator's defaults play the rules as agreed: the batch's settings
// for R3 with no options changed are the rules library's agreedRules()
#include "../../simulate/Batch.hpp"

#include <CosmicHex/Rules.hpp>

#include <gtest/gtest.h>

#include <fstream>
#include <string>

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

// Three players: 10 points to win (round 85), the same in both
TEST(AgreedRulesTest, ThreePlayersPlayToTen) {
  Batch::Options options;
  options.players = 3;
  EXPECT_EQ(Batch::settingsFor(options).winPoints, kThreePlayerWinPoints);
  EXPECT_EQ(agreedRules(boardR3(), 3).winPoints, kThreePlayerWinPoints);
  EXPECT_EQ(agreedRules(boardR3()).winPoints, 12);
}

// The boards a game offers are the layouts file's, R3 first and equal to
// boardR3()
TEST(AgreedRulesTest, TheBoardChoicesAreTheLayoutsFiles) {
  const auto choices = boardChoices();
  ASSERT_FALSE(choices.empty());
  EXPECT_EQ(choices.front().name, "R3");
  EXPECT_TRUE(choices.front().board == boardR3());
  // Three players: T4 first (the default), then T5; three homes each
  const auto three = boardChoices(3);
  ASSERT_EQ(three.size(), 2u);
  EXPECT_EQ(three.front().name, "T4");
  for (const NamedBoard &choice : three) {
    EXPECT_EQ(choice.board.homes.size(), 3u) << choice.name;
  }
  std::vector<NamedBoard> all = choices;
  all.insert(all.end(), three.begin(), three.end());
  for (const NamedBoard &choice : all) {
    std::ifstream file("projects/cosmic_hex/layouts/layouts.txt");
    std::optional<Board> fromFile;
    for (std::string line; std::getline(file, line);) {
      if (line.starts_with(choice.name + " "))
        fromFile = boardFromLine(line);
    }
    // (equal to the choice's board, and found: an empty optional never is)
    EXPECT_TRUE(fromFile == choice.board) << choice.name;
  }
}
