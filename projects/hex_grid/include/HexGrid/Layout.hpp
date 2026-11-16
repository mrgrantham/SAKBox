// Layout.hpp - where hexes are on screen: a hex's centre and corners in
// pixels (or world units), and which hex a point (a touch) falls in.
//
//   HexGrid::Layout layout{HexGrid::Orientation::PointyTop, {12, 12}, {128, 96}};
//   HexGrid::Point centre = layout.centre({2, -1});
//   HexGrid::Hex touched = layout.hexAt({touch.px, touch.py});
//   HexGrid::Point spot = layout.point(HexGrid::corner({2, -1}, 0));
//
// This part uses floating point: it's for drawing and input, which needn't
// be the same to the last bit everywhere. The game's rules stay on Hex.
#pragma once

#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Hex.hpp>

#include <array>
#include <cmath>

namespace HexGrid {

struct Point {
  float x = 0;
  float y = 0;
};

enum class Orientation {
  PointyTop, // a corner at the top; rows of hexes run left to right
  FlatTop,   // a flat edge at the top; columns run top to bottom
};

struct Layout {
  Orientation orientation = Orientation::PointyTop;
  Point size{1, 1}; // from a hex's centre to a corner, across and down
  Point origin{};   // where the hex (0, 0) is centred

  Point centre(Hex hex) const {
    const float q = static_cast<float>(hex.q), r = static_cast<float>(hex.r);
    if (orientation == Orientation::PointyTop)
      return {origin.x + size.x * (kSqrt3 * q + kSqrt3 / 2 * r), origin.y + size.y * (1.5f * r)};
    return {origin.x + size.x * (1.5f * q), origin.y + size.y * (kSqrt3 / 2 * q + kSqrt3 * r)};
  }

  // The six corners, going round from the first (on the right with flat
  // tops, up and right with pointy tops), each 60 degrees on.
  std::array<Point, 6> corners(Hex hex) const {
    const Point c = centre(hex);
    std::array<Point, 6> out{};
    const float start = orientation == Orientation::PointyTop ? 0.5f : 0.0f;
    for (int i = 0; i < 6; ++i) {
      const float angle = kPi / 3 * (static_cast<float>(i) + start);
      out[i] = {c.x + size.x * std::cos(angle), c.y + size.y * std::sin(angle)};
    }
    return out;
  }

  // Where a corner is (from CornersAndEdges.hpp): the middle of the three
  // hexes' centres, which is the point they share whatever the orientation.
  Point point(Corner corner) const {
    Point sum{};
    for (Hex hex : hexesTouching(corner)) {
      const Point c = centre(hex);
      sum.x += c.x;
      sum.y += c.y;
    }
    return {sum.x / 3, sum.y / 3};
  }

  // The hex a point falls in.
  Hex hexAt(Point point) const {
    const float x = (point.x - origin.x) / size.x, y = (point.y - origin.y) / size.y;
    float q, r;
    if (orientation == Orientation::PointyTop) {
      q = kSqrt3 / 3 * x - y / 3;
      r = 2.0f / 3 * y;
    } else {
      q = 2.0f / 3 * x;
      r = -x / 3 + kSqrt3 / 3 * y;
    }
    return roundedHex(q, r);
  }

  // The nearest hex to fractional axial coordinates.
  static Hex roundedHex(float q, float r) {
    const float s = -q - r;
    float rq = std::round(q), rr = std::round(r), rs = std::round(s);
    const float dq = std::abs(rq - q), dr = std::abs(rr - r), ds = std::abs(rs - s);
    if (dq > dr && dq > ds)
      rq = -rr - rs;
    else if (dr > ds)
      rr = -rq - rs;
    return {static_cast<int>(rq), static_cast<int>(rr)};
  }

private:
  static constexpr float kSqrt3 = 1.7320508075688772f;
  static constexpr float kPi = 3.14159265358979f;
};

} // namespace HexGrid
