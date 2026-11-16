// The balance lab's layouts: read from the repo's file, what's on each hex,
// cycling a hex (empty, planet, find, empty; homes left alone), the usual
// finds' places, empty-space corners, and a layout written and read back.
#include "../src/LabLayouts.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>

using namespace CosmicHex;
using namespace CosmicHex::Lab;

namespace {
const char *kLayouts = "projects/cosmic_hex/layouts/layouts.txt";

LabLayout layoutNamed(const std::string &name) {
  for (const LabLayout &layout : readLayouts(kLayouts))
    if (layout.name == name)
      return layout;
  ADD_FAILURE() << "no layout " << name;
  return {};
}
} // namespace

TEST(LabLayoutsTest, ReadsTheRepositorysLayouts) {
  const std::vector<LabLayout> all = readLayouts(kLayouts);
  EXPECT_GE(all.size(), 49u); // rounds 44 and 45's, and later ones
  const LabLayout r2 = layoutNamed("R2");
  EXPECT_EQ(r2.homes.size(), 4u);
  EXPECT_EQ(r2.planets.size(), 15u); // (the core included)
  EXPECT_EQ(r2.reach, "2,2,3");
  EXPECT_EQ(kindAt(r2, Hex{0, 0}), HexKind::Planet);
  EXPECT_EQ(kindAt(r2, r2.homes[0]), HexKind::Home);
}

TEST(LabLayoutsTest, CyclesAHexAndLeavesHomesAlone) {
  LabLayout layout = layoutNamed("R2");
  Hex empty{};
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius))
    if (kindAt(layout, hex) == HexKind::Empty) {
      empty = hex;
      break;
    }
  cycle(layout, empty);
  EXPECT_EQ(kindAt(layout, empty), HexKind::Planet);
  cycle(layout, empty);
  EXPECT_EQ(kindAt(layout, empty), HexKind::Find);
  cycle(layout, empty);
  EXPECT_EQ(kindAt(layout, empty), HexKind::Empty);
  const Hex home = layout.homes[0];
  cycle(layout, home);
  EXPECT_EQ(kindAt(layout, home), HexKind::Home);
  EXPECT_TRUE(layout.reach.empty()); // (edited: no longer known)
}

// Without its own finds, a layout uses the usual places (2 from the centre
// and the six board corners) less those planets and homes take
TEST(LabLayoutsTest, TheUsualFindsPlaces) {
  LabLayout bare;
  EXPECT_EQ(usualZone(bare).size(), 18u); // 12 two from the centre, 6 corners
  bare.planets = {{2, 0}};
  EXPECT_EQ(usualZone(bare).size(), 17u);
}

// Empty-space corners: those touching no planet or home
TEST(LabLayoutsTest, CountsEmptySpaceCorners) {
  LabLayout bare;
  EXPECT_EQ(emptySpaceCorners(bare), 150); // every corner of a radius-4 board
  bare.planets = {{0, 0}};
  EXPECT_EQ(emptySpaceCorners(bare), 144); // less the centre's six
}

// A layout written as a line reads back, and the batch reads it too
TEST(LabLayoutsTest, WrittenAndReadBack) {
  LabLayout layout = layoutNamed("R2");
  cycle(layout, Hex{1, 0});
  layout.name = "Mine";
  const std::string file = testing::TempDir() + "/saved_layouts.txt";
  std::ofstream(file) << layoutLine(layout) << "\n";
  const std::vector<LabLayout> read = readLayouts(file);
  ASSERT_EQ(read.size(), 1u);
  EXPECT_EQ(read[0].planets, layout.planets);
  EXPECT_EQ(read[0].zone, layout.zone);
  Batch::Options options;
  options.layout = "Mine";
  options.layoutsFile = file;
  EXPECT_FALSE(Batch::readLayout(options).has_value());
  EXPECT_EQ(options.zone, layout.zone);
  std::remove(file.c_str());
}
