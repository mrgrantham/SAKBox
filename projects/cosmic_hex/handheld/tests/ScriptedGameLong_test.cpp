// Full-length whole games under the agreed rules (the win at 12), played
// through the app's own shows and touch panel (ScriptedGame): the seeds
// that first found the shows cut short. Slow, so run by hand before
// merging (tagged manual; the README's "Scripted play"):
//   bazel test --jobs=3 //projects/cosmic_hex/handheld:ScriptedGameLong_test
#include "ScriptedRuns.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace CosmicHex::Handheld;
using namespace CosmicHex::Handheld::Testing;

// Every show seen whole unless the player skipped it, each game ended by
// the rules' own win
TEST(ScriptedGameLongTest, FullLengthGamesShowEverythingWhole) {
  const std::vector<PlayedGame> runs = playAll({{.seed = 1}, {.seed = 3}, {.seed = 9}});
  for (const PlayedGame &run : runs) {
    EXPECT_EQ(endedByTheWin(*run.game), "") << nameOf(run.settings);
    EXPECT_EQ(run.game->table().state().settings.winPoints, 12);
    EXPECT_EQ(unasked(*run.game), "") << nameOf(run.settings);
  }
}
