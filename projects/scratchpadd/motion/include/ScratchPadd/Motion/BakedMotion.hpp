// BakedMotion.hpp - a motion made in the animation lab (physics on a Mac),
// played back cheaply anywhere: its path and its timing kept apart (the
// user's choice).
//
// The path is a list of points evenly spaced along the way it goes, joined
// by a smooth curve through them (Catmull-Rom); the timing is how far along
// the path it is (0 at the start, 1 at the end) at evenly spaced moments.
// Playing it back looks up how far along it is at a time, then where that
// is on the path. Keeping them apart means the path can be reused with
// another timing (slower, snappier), or the timing with another path.
//
//   BakedMotion throwIt = ...;                 // from the lab (or a file)
//   const auto where = throwIt.at(seconds);    // a point, x y z
//
// Small and plain: a few dozen floats, no physics, no allocation to play.
// A thing that tumbles adds its turns: four floats at each timing moment.
#pragma once

#include <array>
#include <vector>

namespace ScratchPadd::Motion {

struct BakedMotion {
  using Point = std::array<float, 3>;

  using Turn = std::array<float, 4>; // (a rotation: w, x, y, z)

  std::vector<Point> path;   // (evenly spaced along the way, start to end)
  std::vector<float> timing; // (how far along, 0 to 1, at evenly spaced moments)
  float seconds = 0;         // (how long it takes)
  // How it's turned at the same moments as the timing, for a thing that
  // tumbles (none: it doesn't turn)
  std::vector<Turn> turns;

  // How far along the path it is at `t` seconds (0 to 1; held at the ends)
  float along(float t) const;
  // The point `s` of the way along the path (0 to 1)
  Point onPath(float s) const;
  // Where it is at `t` seconds
  Point at(float t) const { return onPath(along(t)); }
  // How it's turned at `t` seconds (between two moments, the shorter way
  // round, normalised); unturned with no turns
  Turn turnAt(float t) const;
};

} // namespace ScratchPadd::Motion
