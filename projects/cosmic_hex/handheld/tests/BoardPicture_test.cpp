// The flat board: tiles where the hex grid puts them, each touch finding its
// hex (and a corner, close to one), zooming and panning kept in bounds. The
// picture is also written out (bazel's test outputs) to look at.
#include <CosmicHex/Handheld/BoardPicture.hpp>
#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/PlatformProfile.hpp>
#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

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
  // A touch anywhere picks the corner nearest it: on a corner,
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

// A turned-over planet shows its number chip over its face's
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

// Preparing the faces a step at a time (in spare frames, so the
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

// Zooming shows at once: just after setZoom, before any face is
// drawn at the new size, the map shows the whole board's faces scaled up
// (a planet's middle isn't the empty space's colour); it's ready, sharp,
// once prepare() has drawn this zoom's
TEST(BoardPictureTest, ZoomingShowsAtOnceThenSharpens) {
  const State state = aGame();
  BoardPicture picture(320, 240);
  picture.draw(state);
  EXPECT_TRUE(picture.ready());
  picture.setZoom(2);
  EXPECT_FALSE(picture.ready());
  picture.draw(state);
  // The stand-in: nearly the same as the sharp picture drawn after (most
  // pixels within a little of it), only blockier
  const std::vector<ScratchPadd::Scene::Colour> standIn = picture.pixels();
  int frames = 0;
  while (!picture.prepare())
    ++frames;
  EXPECT_TRUE(picture.ready());
  EXPECT_GT(frames, 10); // (a face or chip a frame)
  picture.draw(state);
  int close = 0;
  for (std::size_t p = 0; p < standIn.size(); ++p) {
    const auto &a = standIn[p], &b = picture.pixels()[p];
    close += std::abs(a.r - b.r) < 48 && std::abs(a.g - b.g) < 48 && std::abs(a.b - b.b) < 48;
  }
  EXPECT_GT(close, static_cast<int>(standIn.size() * 9 / 10));
}

// Drawing only what changed gives the same picture as drawing
// it all, pixel for pixel: through two whole games of bots' moves, with
// marks coming and going, the zoom changing (and its faces sharpening),
// panning, and areas drawn over (as the buttons are) asked for again
TEST(BoardPictureTest, DrawingWhatChangedMatchesDrawingItAll) {
  for (const std::uint64_t seed : {3u, 17u}) {
    State state = newGame(4, agreedRules(boardR3(), 4));
    SeededRandom::Generator random(seed);
    BoardPicture kept(256, 192), fresh(256, 192);
    int compared = 0, small = 0;
    for (int step = 0; step < 3000 && state.winner < 0; ++step) {
      const auto legal = Game::legalActions(state);
      Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                             ? Game::dealerAction(state, random)
                             : legal[random.below(static_cast<std::uint32_t>(legal.size()))]);
      if (step % 4 != 0)
        continue;
      BoardPicture::Marks marks;
      if (step % 3 == 0 && !state.stations.empty())
        marks.corners.push_back(state.stations.back().at);
      if (step % 5 == 0 && !state.planets.empty())
        marks.hex = state.planets[static_cast<std::size_t>(step / 5) % state.planets.size()].hex;
      if (step % 140 == 0) {
        kept.setZoom(1 + (step / 140) % 3);
        kept.pan(static_cast<float>(step % 30) - 15, 0);
      }
      if (step % 11 == 0)
        for (int i = 0; i < 6; ++i)
          kept.prepare(); // (a zoomed face or two sharpening now and then)
      if (step % 13 == 0) {
        // Something drawn over the picture, then its area asked for again
        auto pixels = kept.pixelsToDrawOver();
        for (int y = 170; y < 190; ++y)
          for (int x = 4; x < 26; ++x)
            pixels[static_cast<std::size_t>(y * 256 + x)] = {255, 0, 255, 255};
        kept.redrawNext({4, 170, 22, 20});
      }
      kept.draw(state, marks);
      // The same from scratch: a new picture, at the same zoom and pan
      fresh = BoardPicture(256, 192);
      fresh.setZoom(kept.zoom());
      fresh.pan(kept.where(Hex{0, 0}).x - fresh.where(Hex{0, 0}).x,
                kept.where(Hex{0, 0}).y - fresh.where(Hex{0, 0}).y);
      // (compared once the zoom's faces are all sharp: part way, the fresh
      // picture couldn't stand in the same ones)
      if (!kept.ready())
        continue;
      while (!fresh.prepare()) {
      }
      fresh.draw(state, marks);
      ++compared;
      int area = 0;
      for (const auto &a : kept.changed())
        area += a.width * a.height;
      small += area < 256 * 192 / 2;
      ASSERT_EQ(kept.pixels(), fresh.pixels()) << "seed " << seed << ", step " << step;
    }
    EXPECT_GT(compared, 50);
    EXPECT_GT(small, compared / 2); // (most draws changed less than half the picture)
  }
}

// Zooming stops at the closest zoom whose sharp tiles fit the budget: a
// DS's bottom screen has zoom 2 (about 240 KB) and not 3 (about 530 KB)
TEST(BoardPictureTest, ZoomingStopsWithinTheBudget) {
  BoardPicture picture(256, 192);
  EXPECT_EQ(picture.mostZoom(), BoardPicture::kMostZoom);
  picture.setZoomBudget(kDsProfile.boardZoomBytes);
  EXPECT_EQ(picture.mostZoom(), 2);
  picture.setZoom(3);
  EXPECT_EQ(picture.zoom(), 2);
  picture.setZoomBudget(kThreeDsProfile.boardZoomBytes);
  BoardPicture threeDs(320, 240);
  threeDs.setZoomBudget(kThreeDsProfile.boardZoomBytes);
  EXPECT_EQ(threeDs.mostZoom(), BoardPicture::kMostZoom);
}
