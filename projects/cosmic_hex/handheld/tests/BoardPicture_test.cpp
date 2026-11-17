// The flat board: tiles where the hex grid puts them, each touch finding its
// hex (and a corner, close to one), zooming and panning kept in bounds. The
// picture is also written out (bazel's test outputs) to look at.
#include <CosmicHex/Handheld/BoardPicture.hpp>

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
      EXPECT_FALSE(spot->corner.has_value()); // (the middle, far from corners)
    }
  }
  // A touch on a corner finds it
  const Corner corner = HexGrid::corner(state.planets[0].hex, 2);
  const auto there = picture.where(corner);
  const auto spot =
      picture.at(static_cast<int>(std::lround(there.x)), static_cast<int>(std::lround(there.y)));
  ASSERT_TRUE(spot.has_value());
  if (spot && spot->corner) {
    EXPECT_EQ(cornerNumber(*spot->corner), cornerNumber(corner));
  } else {
    ADD_FAILURE() << "no corner found";
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
