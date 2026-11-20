// The board's animations: a moved ship slides straight from its last corner;
// a planet explored turns over; nothing else moves
#include <CosmicHex/Handheld/BoardMotion.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <gtest/gtest.h>

#include <cmath>

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

TEST(BoardMotionTest, AMovedShipFliesThere) {
  const State before = twoShips();
  State after = before;
  after.ships[0].corner = Corner{{0, 1}, 0};
  const BoardMotion motion(before, after);
  const auto from = BoardPieces::at(before.ships[0].corner);
  const auto to = BoardPieces::at(after.ships[0].corner);
  // Halfway along, bowed a little off the straight line
  const auto half = motion.shipAt(0, 0.5f);
  const float off = std::hypot(x(half) - (x(from) + x(to)) / 2, z(half) - (z(from) + z(to)) / 2);
  EXPECT_NEAR(off, BoardMotion::kBow * (to - from).length(), 1e-3);
  EXPECT_NEAR(x(motion.shipAt(0, 0)), x(from), 1e-5);
  EXPECT_NEAR(x(motion.shipAt(0, 1)), x(to), 1e-5);
  EXPECT_NEAR(z(motion.shipAt(0, 1)), z(to), 1e-5);
  // The other ship stays put
  EXPECT_NEAR(x(motion.shipAt(1, 0)), x(BoardPieces::at(before.ships[1].corner)), 1e-5);
}

// Its engine burns while it speeds up (the flame swivelled, steering it
// round the arc) and is out as it coasts in; its nose follows the flight,
// and it keeps facing that way after, through the next move's motion
TEST(BoardMotionTest, AShipBurnsThenCoastsAndKeepsItsHeading) {
  const State before = twoShips();
  State after = before;
  after.ships[0].corner = Corner{{0, 1}, 0};
  const BoardMotion motion(before, after);
  const ShipPose early = motion.ship(0, 0.2f), late = motion.ship(0, 0.8f);
  EXPECT_GT(early.flame, 0.3f);
  EXPECT_NE(early.gimbal, 0.0f);
  EXPECT_LE(std::abs(early.gimbal), BoardMotion::kMostGimbal);
  EXPECT_FLOAT_EQ(late.flame, 0);
  const ShipPose arrived = motion.ship(0, 1);
  EXPECT_FLOAT_EQ(arrived.flame, 0);
  EXPECT_NEAR(motion.ship(0, 0.999f).heading, arrived.heading, 0.05f); // (no snap at the end)
  // The next move moves the other ship: this one keeps its heading
  State next = after;
  next.ships[1].corner = Corner{{1, 1}, 0};
  const BoardMotion then(after, next, &motion);
  EXPECT_FLOAT_EQ(then.ship(0, 0.5f).heading, arrived.heading);
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

// A station built drops in, lands, squashes and springs back, its dust
// spreading; one already there doesn't move; a station become a colony
// lands again
TEST(BoardMotionTest, ABuildingDropsInWithDust) {
  State before = twoShips();
  before.stations = {Station{Corner{{0, 0}, 0}, 0, false}, Station{Corner{{1, 0}, 0}, 1, false}};
  State after = before;
  after.stations[1].colony = true;
  after.stations.push_back(Station{Corner{{0, 1}, 1}, 0, false});
  const BoardMotion motion(before, after);
  EXPECT_FLOAT_EQ(motion.landing(0, 0.2f).drop, 0); // (already there)
  EXPECT_FALSE(motion.landing(0, 0.8f).dust.has_value());
  for (const std::size_t built : {std::size_t{1}, std::size_t{2}}) {
    EXPECT_FLOAT_EQ(motion.landing(built, 0).drop, BoardMotion::kDropHeight);
    EXPECT_GT(motion.landing(built, 0.2f).drop, motion.landing(built, 0.4f).drop);
    EXPECT_FLOAT_EQ(motion.landing(built, BoardMotion::kLanded).drop, 0);
    EXPECT_LT(motion.landing(built, BoardMotion::kLanded + 0.01f).squash, 0.8f);
    ASSERT_TRUE(motion.landing(built, 1.0f).dust.has_value());
    EXPECT_GT(*motion.landing(built, 1.0f).dust, 0);
    EXPECT_FALSE(motion.landing(built, BoardMotion::kLandingEnds).dust.has_value());
    EXPECT_FLOAT_EQ(motion.landing(built, BoardMotion::kLandingEnds).squash, 1);
  }
}

// Whether anything moves: nothing between the same states
TEST(BoardMotionTest, NothingMovesBetweenTheSameStates) {
  const State state = twoShips();
  EXPECT_FALSE(BoardMotion(state, state).anything());
  State moved = state;
  moved.ships[0].corner = Corner{{0, 1}, 0};
  EXPECT_TRUE(BoardMotion(state, moved).anything());
}
