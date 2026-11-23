// Short whole games (ScriptedGame_test's) at the other paces: Quick
// animations, a DS's slower frames, four bots watched, and an impatient
// player pressing B while things animate. Every show is seen whole unless
// the player skipped it, and B never skips a show still to come (the
// README's "Scripted play")
#include "ScriptedRuns.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace CosmicHex::Handheld;
using namespace CosmicHex::Handheld::Testing;

namespace {

constexpr int kSeedsAPace = 5;

std::vector<PlayedGame> playedAt(std::uint64_t firstSeed, const ScriptedGameSettings &pace) {
  return playAll(shortGames(firstSeed, kSeedsAPace, pace));
}

void expectSeenWhole(const std::vector<PlayedGame> &runs) {
  for (const PlayedGame &run : runs) {
    EXPECT_EQ(endedByTheWin(*run.game), "") << nameOf(run.settings);
    EXPECT_EQ(unasked(*run.game), "") << nameOf(run.settings);
  }
}

} // namespace

// Twice as fast: the shows' own clock at Quick's pace
TEST(ScriptedGamePacesTest, QuickAnimations) {
  expectSeenWhole(playedAt(1, {.animations = Animations::Quick}));
}

// A DS's frames, often 20 a second rather than 60
TEST(ScriptedGamePacesTest, ADSsSlowerFrames) {
  expectSeenWhole(playedAt(6, {.frameSeconds = 1.0 / 20}));
}

// Four bots, watched: no player's moves to wait for
TEST(ScriptedGamePacesTest, Watched) { expectSeenWhole(playedAt(11, {.person = false})); }

// B jumped the shows' clock past every show's end, so a home's pay
// waiting for the banner B skipped was skipped too, its die never seen.
// Shows still to come now start afresh after a skip
TEST(ScriptedGamePacesTest, SkippingWhatAnimatesLeavesWhatsToCome) {
  const std::vector<PlayedGame> runs = playedAt(16, {.impatience = 10});
  for (const PlayedGame &run : runs)
    EXPECT_EQ(run.game->log().count(ShowLog::Cut::SkippedQueued), 0u) << nameOf(run.settings);
  expectSeenWhole(runs);
}
