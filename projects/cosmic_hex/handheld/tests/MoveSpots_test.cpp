// Where moves happen: a ship's move at its corner and where it goes, a
// station's build at its corner, the blockade on its planet; a touch (always
// on a corner) finds the moves there, or the blockade's on its hex
#include <CosmicHex/Handheld/MoveSpots.hpp>

#include <gtest/gtest.h>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;
using Kind = Action::Kind;

TEST(MoveSpotsTest, MovesHappenWhereTheirPiecesAre) {
  State state;
  const Corner from{{0, 0}, 0}, to = boardCorners()[5];
  state.ships = {Ship{0, Hex{}, false, from}};
  state.stations = {Station{to, 0, true}};
  state.planets = {Planet{}};
  state.planets[0].hex = {2, -1};
  const Action move{Kind::Move, 0, cornerNumber(to)};
  const MoveSpot spot = spotOf(state, move);
  ASSERT_EQ(spot.corners.size(), 2u);
  EXPECT_EQ(spot.corners[0], from);
  EXPECT_EQ(spot.corners[1], to);
  // Touched at either end's corner
  EXPECT_TRUE(concerns(state, move, {from.hex, from}));
  EXPECT_TRUE(concerns(state, move, {to.hex, to}));
  // Another corner of the same hex: not there
  EXPECT_FALSE(concerns(state, move, {from.hex, HexGrid::corner(from.hex, 3)}));
  // A colony's ship; the blockade's planet; ending the turn: nowhere
  EXPECT_EQ(spotOf(state, {Kind::BuildShip, 0}).corners.at(0), to);
  EXPECT_TRUE(
      concerns(state, {Kind::MoveBlockade, 0, -1}, {Hex{2, -1}, HexGrid::corner({2, -1}, 0)}));
  EXPECT_FALSE(
      concerns(state, {Kind::MoveBlockade, 0, -1}, {Hex{4, -4}, HexGrid::corner({4, -4}, 0)}));
  EXPECT_TRUE(spotOf(state, {Kind::EndTurn}).corners.empty());
}
