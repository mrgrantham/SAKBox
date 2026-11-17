// The board's animations: a moved ship slides straight from its last corner;
// a planet explored turns over; nothing else moves
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <gtest/gtest.h>

#include <numbers>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {
State twoShips() {
  State state;
  state.ships = {Ship{0, Hex{}, false, Corner{{0, 0}, 0}},
                 Ship{1, Hex{}, false, Corner{{1, 0}, 1}}};
  state.planets = {Planet{}, Planet{}};
  return state;
}
float x(const ScratchPadd::Scene::Vector3 &v) { return static_cast<float>(v.x); }
float z(const ScratchPadd::Scene::Vector3 &v) { return static_cast<float>(v.z); }
} // namespace

TEST(BoardMotionTest, AMovedShipSlidesStraight) {
  const State before = twoShips();
  State after = before;
  after.ships[0].corner = Corner{{0, 1}, 0};
  const BoardMotion motion(before, after);
  const auto from = BoardPieces::at(before.ships[0].corner);
  const auto to = BoardPieces::at(after.ships[0].corner);
  const auto half = motion.shipAt(0, 0.5f);
  EXPECT_NEAR(x(half), (x(from) + x(to)) / 2, 1e-5);
  EXPECT_NEAR(z(half), (z(from) + z(to)) / 2, 1e-5);
  EXPECT_NEAR(x(motion.shipAt(0, 1)), x(to), 1e-5);
  // The other ship stays put
  EXPECT_NEAR(x(motion.shipAt(1, 0)), x(BoardPieces::at(before.ships[1].corner)), 1e-5);
}

TEST(BoardMotionTest, AShipThatCameOrWentDoesntSlide) {
  const State before = twoShips();
  State after = before;
  after.ships.erase(after.ships.begin()); // (one settled)
  const BoardMotion motion(before, after);
  EXPECT_NEAR(x(motion.shipAt(0, 0)), x(BoardPieces::at(after.ships[0].corner)), 1e-5);
}

TEST(BoardMotionTest, AnExploredPlanetTurnsOver) {
  const State before = twoShips();
  State after = before;
  after.planets[1].flipped = true;
  const BoardMotion motion(before, after);
  EXPECT_FALSE(motion.planetTurn(0, 0.5f).has_value());
  const auto turn = motion.planetTurn(1, 0.5f);
  ASSERT_TRUE(turn.has_value());
  if (turn) {
    EXPECT_NEAR(*turn, std::numbers::pi_v<float> / 2, 1e-5);
  }
}
