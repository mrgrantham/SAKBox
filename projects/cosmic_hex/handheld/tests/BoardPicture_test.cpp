// The flat board: tiles where the hex grid puts them, each touch finding its
// hex (and a corner, close to one), zooming and panning kept in bounds. The
// picture is also written out (bazel's test outputs) to look at.
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>

using namespace CosmicHex;
using namespace CosmicHex::Handheld;

namespace {
State aGame() { return newGame(4, agreedRules(boardR3())); }

// A PPM of the picture, in the test's outputs (TEST_UNDECLARED_OUTPUTS_DIR)
void writeOut(const BoardPicture &picture, const char *name) {
  const char *folder = std::getenv("TEST_UNDECLARED_OUTPUTS_DIR");
  if (!folder)
    return;
  std::ofstream file(std::string(folder) + "/" + name, std::ios::binary);
  file << "P6\n" << picture.width() << " " << picture.height() << "\n255\n";
  for (const auto &c : picture.pixels()) // (the bytes themselves: PPM's binary form)
    file.put(static_cast<char>(c.r)).put(static_cast<char>(c.g)).put(static_cast<char>(c.b));
}
} // namespace

TEST(BoardPictureTest, TheWholeBoardFitsAndTouchesFindTheirHex) {
  const State state = aGame();
  BoardPicture picture(320, 240);
  picture.draw(state);
  writeOut(picture, "board_zoom1.ppm");
  for (const Planet &planet : state.planets) {
    const auto at = picture.where(planet.hex);
    ASSERT_GE(at.x, 0);
    ASSERT_LT(at.x, 320);
    ASSERT_GE(at.y, 0);
    ASSERT_LT(at.y, 240);
    const auto spot = picture.at(static_cast<int>(at.x), static_cast<int>(at.y));
    ASSERT_TRUE(spot.has_value());
    if (spot) {
      EXPECT_EQ(spot->hex, planet.hex);
    }
  }
  // A touch anywhere picks the corner nearest it (round 93): on a corner,
  // and a little way in from each of a hex's corners towards its middle
  const Hex hex = state.planets[0].hex;
  const HexGrid::Point middle = picture.where(hex);
  for (int i = 0; i < 6; ++i) {
    const Corner corner = HexGrid::corner(hex, i);
    const HexGrid::Point c = picture.where(corner);
    for (const float in : {0.0f, 0.4f}) {
      const auto spot = picture.at(static_cast<int>(std::lround(c.x + (middle.x - c.x) * in)),
                                   static_cast<int>(std::lround(c.y + (middle.y - c.y) * in)));
      ASSERT_TRUE(spot.has_value());
      if (spot) {
        EXPECT_EQ(cornerNumber(spot->corner), cornerNumber(corner)) << "corner " << i << ", " << in;
      }
    }
  }
  // Off the board: nothing
  EXPECT_FALSE(picture.at(2, 2).has_value());
}

TEST(BoardPictureTest, ZoomingEnlargesAndPanningStaysInBounds) {
  const State state = aGame();
  BoardPicture picture(320, 240);
  const auto near = picture.where(Hex{1, 0}), centre = picture.where(Hex{0, 0});
  picture.setZoom(2);
  const auto near2 = picture.where(Hex{1, 0}), centre2 = picture.where(Hex{0, 0});
  EXPECT_NEAR(near2.x - centre2.x, 2 * (near.x - centre.x), 1);
  picture.pan(100000, 0); // as far as it goes, and no further
  const float limit = picture.where(Hex{0, 0}).x;
  picture.pan(1, 0);
  EXPECT_FLOAT_EQ(picture.where(Hex{0, 0}).x, limit);
  picture.draw(state);
  writeOut(picture, "board_zoom2_panned.ppm");
  picture.setZoom(1); // back to the whole board, centred
  EXPECT_FLOAT_EQ(picture.where(Hex{0, 0}).x, 160);
}

// Round 87: a turned-over planet shows its number chip over its face's
// lower part (cream at the chip's top); a face-down one doesn't
TEST(BoardPictureTest, TurnedOverPlanetsShowTheirNumberChips) {
  State state = aGame();
  int number = 2;
  for (Planet &planet : state.planets) {
    if (planet.homeOf >= 0) // (the homes: no number)
      continue;
    planet.flipped = true;
    planet.kind = PlanetKind::AsteroidBelt;
    planet.number = number;
    number = number == 6 ? 8 : number == 12 ? 2 : number + 1;
  }
  BoardPicture picture(320, 240);
  picture.setZoom(3);
  picture.draw(state);
  writeOut(picture, "board_chips_zoom3.ppm");
  BoardPicture whole(320, 240);
  whole.draw(state);
  writeOut(whole, "board_chips_zoom1.ppm");
  // A hex's size (centre to corner) from two neighbours' centres
  const auto a = whole.where(Hex{0, 0}), b = whole.where(Hex{1, 0});
  const float size = std::hypot(b.x - a.x, b.y - a.y) / std::sqrt(3.0f);
  // Every turned-over planet: just inside its chip's top, cream
  int checked = 0;
  for (const Planet &planet : state.planets) {
    if (chipOf(planet) == 0)
      continue;
    const auto at = whole.where(planet.hex);
    const int x = static_cast<int>(std::lround(at.x));
    const int y = static_cast<int>(std::lround(at.y + (kChipDown - 0.75f * kChipRadius) * size));
    const auto colour =
        whole.pixels()[(static_cast<std::size_t>(y) * 320) + static_cast<std::size_t>(x)];
    EXPECT_GT(colour.r, 200) << "planet at " << planet.hex.q << "," << planet.hex.r;
    ++checked;
  }
  EXPECT_GT(checked, 10);
  // Face down again: no chips
  for (Planet &planet : state.planets)
    planet.flipped = planet.homeOf >= 0;
  for (const Planet &planet : state.planets)
    EXPECT_EQ(chipOf(planet), 0);
}

// Preparing the faces a step at a time (round 92: in spare frames, so the
// DS's first map doesn't stall) draws the same picture as drawing at once
TEST(BoardPictureTest, PreparingAheadDrawsTheSamePicture) {
  const State state = aGame();
  BoardPicture atOnce(256, 192), ahead(256, 192);
  int steps = 0;
  while (!ahead.prepare())
    ++steps;
  EXPECT_GT(steps, 10);         // (one face or chip a step)
  EXPECT_TRUE(ahead.prepare()); // (and done)
  atOnce.draw(state);
  ahead.draw(state);
  EXPECT_EQ(atOnce.pixels(), ahead.pixels());
}
