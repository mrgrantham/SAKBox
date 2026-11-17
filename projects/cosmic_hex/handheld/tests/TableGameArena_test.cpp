// The bots' search in a frame arena (round 90), with the counting
// allocator linked (as on the consoles): the same seed plays the same moves
// with the arena as without (the arena changes where memory comes from,
// not what the search does), the arena is used, and far fewer allocations
// reach the heap
#include <CosmicHex/Handheld/TableGame.hpp>
#include <ScratchPadd/MemoryStats.hpp>

#include <gtest/gtest.h>

#include <cstdio>
#include <vector>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
namespace MS = ScratchPadd::MemoryStats;

namespace {
// The first `count` moves of a four-bot game, and the heap allocations it made
std::pair<std::vector<std::string>, std::uint64_t> play(std::size_t arenaBytes, int count,
                                                        std::size_t *peak = nullptr) {
  TableGame game(TableSettings{.botIterations = 60, .frameArenaBytes = arenaBytes, .seed = 11});
  MS::Reader reader;
  std::vector<std::string> moves;
  for (int i = 0; i < count && !game.over(); ++i) {
    const auto move = game.step();
    if (!move)
      break;
    moves.push_back(Game::writeAction(*move));
    if (peak)
      *peak = std::max(*peak, game.lastArenaPeak());
  }
  return {moves, reader.take().all.allocations};
}
} // namespace

TEST(TableGameArenaTest, TheSameMovesWithFewerAllocations) {
  ASSERT_TRUE(MS::linked());
  std::size_t peak = 0;
  const auto [plain, plainAllocations] = play(0, 120);
  const auto [framed, framedAllocations] = play(256 * 1024, 120, &peak);
  std::printf(
      "heap allocations: %llu without the arena, %llu with; the most one game held: %zu bytes\n",
      static_cast<unsigned long long>(plainAllocations),
      static_cast<unsigned long long>(framedAllocations), peak);
  EXPECT_EQ(plain, framed);
  EXPECT_GT(peak, 0u);                                // (the arena was used)
  EXPECT_LT(framedAllocations, plainAllocations / 4); // (most of the search's allocations gone)
}
