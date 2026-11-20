// Dice thrown in turn: each lands showing the number chosen for it, rests
// where the rehearsal said, and stays put while the next is thrown
#include <CosmicHex/Dice/DiceInTurn.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex::Dice;

namespace {
// The number on top as drawn: the die's turn (numbers included) applied to
// each face's direction, whichever points most nearly up
int shownOnTop(const cyclone::Quaternion &q) {
  cyclone::Matrix4 m;
  m.setOrientationAndPos(q, cyclone::Vector3());
  int face = 0;
  double best = -2;
  for (int axis = 0; axis < 3; ++axis) {
    const double up = static_cast<double>(m.getAxisVector(axis).y);
    if (up > best) {
      best = up;
      face = axis + 1;
    }
    if (-up > best) {
      best = -up;
      face = 6 - axis;
    }
  }
  return face;
}
} // namespace

TEST(DiceInTurnTest, TheTurnPutsAFaceUp) {
  for (int face = 1; face <= 6; ++face)
    for (int up = 1; up <= 6; ++up) {
      // (a die resting with `up` on top; the numbers turned so `face` shows)
      cyclone::Quaternion resting; // (its +y up: 2 on top)
      const cyclone::Quaternion toUp = turnFaceTo(up, 2);
      cyclone::Quaternion body = toUp; // (a body turned so its `up` side is on top)
      body.normalise();
      cyclone::Quaternion drawn = body;
      drawn *= turnFaceTo(face, up);
      EXPECT_EQ(shownOnTop(drawn), face) << face << " " << up;
      (void)resting;
    }
}

TEST(DiceInTurnTest, EachLandsOnItsNumberAndStays) {
  DiceInTurn dice(1.0 / 30);
  const std::array<int, 4> wanted{5, 2, 6, 3};
  for (int i = 0; i < 4; ++i) {
    dice.throwDie(i, 1000 + static_cast<std::uint64_t>(i), wanted[static_cast<std::size_t>(i)]);
    int steps = 0;
    while (!dice.settled(i) && steps++ < 400)
      dice.step();
    ASSERT_TRUE(dice.settled(i));
    EXPECT_EQ(shownOnTop(dice.orientation(i)), wanted[static_cast<std::size_t>(i)]) << i;
  }
  // (the first still where it settled, after the others were thrown)
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(shownOnTop(dice.orientation(i)), wanted[static_cast<std::size_t>(i)]) << i;
}

TEST(DiceInTurnTest, ImpatientSetsItDownAtOnce) {
  DiceInTurn dice(1.0 / 30);
  dice.throwDie(0, 7, 4);
  dice.step();
  EXPECT_TRUE(dice.moving());
  dice.throwDie(1, 8, 1); // (the first set down where it would land)
  EXPECT_TRUE(dice.settled(0));
  EXPECT_EQ(shownOnTop(dice.orientation(0)), 4);
}
