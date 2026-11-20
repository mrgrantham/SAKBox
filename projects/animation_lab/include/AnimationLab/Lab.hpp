// Lab.hpp - the animation lab's three ways of making a motion with physics
// (Cyclone, on the Mac), and baking what you like into a small BakedMotion
// the consoles play (ScratchPadd's Motion library).
//
// The three ways, from simple to thorough (the user's: all three):
//   solveThrow   - say where it starts and lands and how long it takes: the
//                  throw is worked out, then flown (a projectile, Millington
//                  ch. 3): a natural arc, every time
//   guideAlong   - sketch a rough path (keys: where, when): a spring pulls
//                  the thing after it, so it lags, overshoots and settles
//                  (the follow-through of Lasseter's principles, from
//                  physics)
//   searchThrow  - when there's no formula (air drag, a bounce on the
//                  floor): throws are tried, the best kept and improved on,
//                  until one lands where it should
//   tumbleThrow  - a thing of a real shape (a model's, its hull: the user's
//                  B) thrown spinning onto the floor, bouncing and rolling
//                  to rest as that shape would; its turns recorded too
// Each makes a Recording (where it was at each fixed step); bake() turns
// one into a BakedMotion: its path and its timing apart (the user's A).
// Every way works flat too (2D: z stays 0), for motion on a screen.
//
//   const Recording thrown = solveThrow({.from = {0, 0, 0}, .to = {4, 0, 0}, .seconds = 1});
//   const ScratchPadd::Motion::BakedMotion baked = bake(thrown);
#pragma once

#include <ScratchPadd/Motion/BakedMotion.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace AnimationLab {

using Point = ScratchPadd::Motion::BakedMotion::Point;

// A motion as the physics made it: where it was every `step` seconds
struct Recording {
  double step = 1.0 / 120;
  std::vector<Point> positions;
  // How it was turned at each step, for a thing that tumbles (none: it
  // doesn't turn)
  std::vector<ScratchPadd::Motion::BakedMotion::Turn> turns;
  double seconds() const;
};

// solveThrow: from `from` to `to` in `seconds`, under gravity (down -y)
struct Throw {
  Point from{}, to{};
  double seconds = 1;
  double gravity = 9.81;
  bool flat = false; // (2D: z stays 0)
  double step = 1.0 / 120;
};
Recording solveThrow(const Throw &goal);

// guideAlong: a spring pulling the thing after a point moving through the
// keys (straight from each to the next); stiffer follows closer, less
// damping overshoots more; it carries on `settle` seconds after the last key
struct Key {
  double time;
  Point at;
};
struct Guide {
  std::vector<Key> keys;
  double stiffness = 120;
  double damping = 8;
  double settle = 0.6;
  bool flat = false;
  double step = 1.0 / 120;
};
Recording guideAlong(const Guide &guide);

// searchThrow: as solveThrow, with air drag (each second its speed kept
// `drag` of, 1 none) and a floor at `floor` it bounces off (keeping
// `bounce` of its speed); throws are searched for one landing at `to` at
// `seconds`
struct Search {
  Throw goal;
  double drag = 0.6;
  double floor = -1e9; // (none)
  double bounce = 0.5;
  int rounds = 12, triesARound = 40;
  std::uint64_t seed = 1;
};
struct Found {
  Recording recording;
  double miss = 0; // (how far from `to` it ended)
};
Found searchThrow(const Search &search);

// tumbleThrow: thrown as solveThrow throws (towards `to` in `seconds`, in a
// vacuum), spinning, a thing whose shape is `corners` (its own space, its
// middle at 0); it lands on the floor (y = `floor`), bounces and rolls, and
// is recorded until it's still, or `settle` seconds past the throw's time
struct Tumble {
  Throw goal;
  std::vector<Point> corners;
  Point spin{0, 6, 3}; // (radians a second, about x, y and z)
  double floor = 0;
  double bounce = 0.3, friction = 0.6;
  double settle = 2;
};
Recording tumbleThrow(const Tumble &tumble);
// A shape's corners for tumbleThrow from all its points: those furthest out
// in each of 26 directions (its hull, near enough, and few)
std::vector<Point> hullCorners(const std::vector<Point> &points);

// A recording as a BakedMotion: `pathPoints` evenly spaced along its way,
// and how far along it is at `timingPoints` evenly spaced moments
ScratchPadd::Motion::BakedMotion bake(const Recording &recording, std::size_t pathPoints = 24,
                                      std::size_t timingPoints = 24);
// The furthest a baked motion strays from its recording, at the recording's
// steps
double bakeError(const Recording &recording, const ScratchPadd::Motion::BakedMotion &baked);

// A baked motion as text, and back (the lab's saved motions): "seconds S",
// then a "path x y z" line a point, then "timing" and its numbers
std::string writeText(const ScratchPadd::Motion::BakedMotion &baked);
std::optional<ScratchPadd::Motion::BakedMotion> readText(const std::string &text);

} // namespace AnimationLab
