// The board's pieces: hexes and corners where the hex grid puts them, and
// meshes with something in them
#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <gtest/gtest.h>

#include <cmath>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

TEST(BoardPiecesTest, HexesAndCornersSitOnTheTable) {
  const auto o = BoardPieces::centre({0, 0});
  EXPECT_FLOAT_EQ(static_cast<float>(o.x), 0);
  EXPECT_FLOAT_EQ(static_cast<float>(o.y), 0);
  // A neighbour is sqrt(3) away; a corner 1 from each hex it touches
  const auto n = BoardPieces::centre({1, 0});
  EXPECT_NEAR(std::hypot(static_cast<float>(n.x), static_cast<float>(n.z)), std::sqrt(3.0f), 1e-4);
  const Corner corner{{0, 0}, 0};
  for (Hex hex : HexGrid::hexesTouching(corner)) {
    const auto c = BoardPieces::centre(hex), p = BoardPieces::at(corner);
    EXPECT_NEAR(std::hypot(static_cast<float>(c.x - p.x), static_cast<float>(c.z - p.z)), 1, 1e-4);
  }
}

TEST(BoardPiecesTest, ThePiecesHaveShape) {
  const BoardPieces pieces;
  EXPECT_EQ(pieces.hexTile.triangleCount(), 6u);
  EXPECT_GT(pieces.station.triangleCount(), 0u);
  EXPECT_GT(pieces.colony.triangleCount(), 0u);
  EXPECT_EQ(pieces.ship.triangleCount(), 4u);
  EXPECT_EQ(pieces.hexRing.triangleCount(), 12u);
  EXPECT_EQ(pieces.cornerRing.triangleCount(), 12u);
  EXPECT_NE(BoardColours::ofPlayer(0), BoardColours::ofPlayer(1));
}

// Each tile shows the face for what it is: face down until explored, then
// its kind; a home, and an exhausted home by what it now gives
TEST(BoardPiecesTest, TilesShowTheirFaces) {
  Planet planet;
  EXPECT_EQ(faceOf(planet), Art::Face::PlanetFaceDown);
  planet.flipped = true;
  planet.kind = PlanetKind::GasGiant;
  EXPECT_EQ(faceOf(planet), Art::Face::PlanetFuel);
  planet.formerHomeOf = 2;
  EXPECT_EQ(faceOf(planet), Art::Face::HomeExhaustedFuel);
  Planet home;
  home.homeOf = 0;
  EXPECT_EQ(faceOf(home), Art::Face::Home);
  ZoneSite site;
  EXPECT_EQ(faceOf(site), Art::Face::FindFaceDown);
  site.flipped = true;
  site.find = Find::AlienStation;
  EXPECT_EQ(faceOf(site), Art::Face::FindOutpost);
}
