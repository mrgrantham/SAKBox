// Settings.hpp - everything that makes one of the lab's motions, as one
// value: the way (and each way's settings, so switching back keeps them),
// flat or 3D, the shape it's seen as (and tumbles as), and the bake. The
// window's history is a list of these, saved as text and read back.
//
//   Settings s;  s.way = Way::Tumble;  s.shape = "rocket";
//   const Made made = make(s, cornersOf(s.shape));
//   const std::string text = writeSettings(s);  // (two the same: the same text)
#pragma once

#include <AnimationLab/Lab.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AnimationLab {

enum class Way : std::uint8_t { Throw, Guide, Search, Tumble };
std::string_view nameOf(Way way);

struct Settings {
  Way way = Way::Throw;
  bool flat = false;
  Throw throwing{.from = {0, 0, 0}, .to = {4, 0, 1}, .seconds = 1.2};
  Guide guide{.keys = {{0, {0, 0, 0}}, {0.35, {1.5, 1.2f, 0}}, {0.7, {3, 0, 0}}}};
  Search search{.goal = {.from = {0, 1, 0}, .to = {5, 0.3f, 0}, .seconds = 1.5},
                .drag = 0.8,
                .floor = 0,
                .bounce = 0.6};
  // (Tumble's corners come from the shape)
  Tumble tumble{.goal = {.from = {0, 2, 0}, .to = {4, 0.5f, 1}, .seconds = 0.9}};
  std::string shape = "die"; // (the lab's shapes: LabShapes)
  float size = 1;            // (its longest side)
  int pathPoints = 24, timingPoints = 24;
};

// A motion made from settings, and baked
struct Made {
  Recording recording;
  ScratchPadd::Motion::BakedMotion baked;
  double miss = 0;  // (Search: how far from where it should land)
  double error = 0; // (how far the bake strays from the physics)
};
// `corners`: the shape's, for Tumble (its own space, at its size)
Made make(const Settings &settings, const std::vector<Point> &corners);

// A line or so naming what's different about these settings: the way, the
// shape and the numbers that matter for it
std::string summary(const Settings &settings);

// As text (a line a setting, "name value..."), and read back; nothing if
// it isn't settings. Settings left out keep their defaults
std::string writeSettings(const Settings &settings);
std::optional<Settings> readSettings(std::string_view text);

} // namespace AnimationLab
