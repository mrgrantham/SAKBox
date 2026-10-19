#define CATCH_CONFIG_MAIN
#include <FlexController.hpp>
#include <FlexGroup.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <stdexcept>

using namespace Flex::Math;

TEST_CASE("Test FlexGroup", "[FlexGroup]") {

  SECTION("Precision comes from the base until overridden") {
    FlexGroup group("flight", Precision{.fracBits = 1});
    REQUIRE(group.precision().fracBits == 1);
    {
      FlexOverride focus = group.push(Precision{.fracBits = 5});
      REQUIRE(group.precision().fracBits == 5);
    }
    REQUIRE(group.precision().fracBits == 1);
  }

  SECTION("Overrides can be released in any order") {
    FlexGroup group("hull", Precision{.fracBits = 2});
    FlexOverride first = group.push(Precision{.fracBits = -1});
    FlexOverride second = group.push(Precision{.fracBits = 8});
    REQUIRE(group.precision().fracBits == 8);
    first.release();
    REQUIRE(group.precision().fracBits == 8);
    second.release();
    REQUIRE(group.precision().fracBits == 2);
    REQUIRE(group.overrideCount() == 0);
  }

  SECTION("Moving an override keeps it applied") {
    FlexGroup group("render", Precision{.fracBits = 9});
    FlexOverride held;
    REQUIRE_FALSE(held.active());
    held = group.push(Precision{.fracBits = 3});
    REQUIRE(held.active());
    REQUIRE(group.precision().fracBits == 3);
    held = FlexOverride();
    REQUIRE(group.precision().fracBits == 9);
  }

  SECTION("View never changes stored values; commit depends on policy") {
    FlexGroup view("render", Precision{.fracBits = 1}, Policy::View);
    FlexGroup lossy("physics", Precision{.fracBits = 1}, Policy::Lossy);
    REQUIRE(view.view(1.3) == 1.0);
    REQUIRE(view.commit(1.3) == 1.3);
    REQUIRE(lossy.commit(1.3) == 1.0);
    REQUIRE(lossy.commit(FlexPoint(1.3)) == FlexPoint(1.0));
  }

  SECTION("activate() makes FlexReal arithmetic follow the group") {
    FlexGroup group("engine", Precision{.fracBits = 2}, Policy::PerOperation);
    {
      auto active = group.activate();
      REQUIRE((FlexReal(0.75) * FlexReal(0.75)).value() == 0.5);
    }
    REQUIRE((FlexReal(0.75) * FlexReal(0.75)).value() == 0.5625);
  }
}

TEST_CASE("Test FlexController", "[FlexController]") {
  FlexController controller;
  controller.addGroup("flight", Precision{.fracBits = 1});
  controller.addGroup("hull", Precision{.fracBits = 2}, Policy::Lossy);

  REQUIRE(controller.groupCount() == 2);
  REQUIRE(controller.hasGroup("hull"));
  REQUIRE(controller.group("hull").policy() == Policy::Lossy);
  REQUIRE_THROWS_AS(controller.addGroup("hull"), std::invalid_argument);
  REQUIRE_THROWS_AS(controller.group("render"), std::out_of_range);

  FlexOverride phase = controller.group("hull").push(Precision{.fracBits = -1});
  REQUIRE(controller.group("hull").precision().fracBits == -1);

  int visited = 0;
  controller.forEachGroup([&](FlexGroup &) { visited++; });
  REQUIRE(visited == 2);
}

// The two Precision Flight mechanics, as properties of the library

// PHASE: walls centred on odd z with a +-0.4 collision window are always
// skipped when the hull samples every 2 units, and always hit at 1 or finer.
static bool hullHitsWall(int bits, Rounding rounding, double wallZ) {
  FlexGroup hull("hull", Precision{.fracBits = bits, .rounding = rounding});
  const double step = 25.0 / 60.0;
  for (double z = wallZ + 5.0; z > wallZ - 5.0; z -= step) {
    if (std::abs(hull.view(FlexPoint(z)).value() - wallZ) <= 0.4) {
      return true;
    }
  }
  return false;
}

TEST_CASE("PHASE skips thin walls only at coarse hull precision", "[mechanics]") {
  for (Rounding rounding : {Rounding::TowardZero, Rounding::Nearest}) {
    for (int k = 0; k < 200; ++k) {
      double wallZ = -(2.0 * k + 1.0);
      REQUIRE_FALSE(hullHitsWall(-1, rounding, wallZ));
      REQUIRE(hullHitsWall(0, rounding, wallZ));
      REQUIRE(hullHitsWall(2, rounding, wallZ));
    }
  }
}

// FOCUS: an opening a quarter unit off the 0.5 grid, with 0.2 of slack, can
// only be lined up with once the grid is finer.
TEST_CASE("FOCUS is needed to line up with gold gates", "[mechanics]") {
  auto reachable = [](int bits) {
    FlexGroup flight("flight", Precision{.fracBits = bits});
    for (double x = -3.0; x < 3.0; x += 0.001) {
      if (std::abs(flight.view(FlexPoint(x)).value() - 1.25) <= 0.2) {
        return true;
      }
    }
    return false;
  };
  REQUIRE_FALSE(reachable(1));
  REQUIRE(reachable(5));
}
