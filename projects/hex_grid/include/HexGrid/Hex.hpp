// Hex.hpp - a position on a hex grid, and the maths of moving around one:
// neighbours, distance, rings, ranges, lines, rotation and reflection.
//
// Positions are axial coordinates (q, r), with the third cube coordinate
// s = -q - r worked out when needed. Everything is integer arithmetic, so
// the results are the same on every platform (the DS included), and it all
// works at compile time.
//
//   HexGrid::Hex home{3, -1};
//   int far = HexGrid::distance(home, HexGrid::Hex{});      // 3
//   for (HexGrid::Hex next : HexGrid::neighbours(home)) ...
//   for (HexGrid::Hex h : HexGrid::ring(HexGrid::Hex{}, 2)) ...
//
// The directions, and so the order of neighbours and of the hexes on a
// ring, are fixed (see kDirections), so anything built on them (a board,
// a search) comes out the same every time.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <vector>

namespace HexGrid {

struct Hex {
  int q = 0;
  int r = 0;

  constexpr int s() const { return -q - r; }

  constexpr Hex operator+(Hex other) const { return {q + other.q, r + other.r}; }
  constexpr Hex operator-(Hex other) const { return {q - other.q, r - other.r}; }
  constexpr Hex operator*(int times) const { return {q * times, r * times}; }
  constexpr Hex &operator+=(Hex other) { return *this = *this + other; }
  constexpr bool operator==(const Hex &) const = default;
  // An order (by q, then r), for sorted containers and stable output
  constexpr auto operator<=>(const Hex &) const = default;
};

// The six directions, counter-clockwise from +q (east, with pointy-topped
// hexes): direction i + 1 is direction i turned 60 degrees.
inline constexpr std::array<Hex, 6> kDirections = {
    {{1, 0}, {1, -1}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}}};

// The direction `turns` steps counter-clockwise from +q (any integer).
constexpr Hex direction(int turns) { return kDirections[((turns % 6) + 6) % 6]; }

constexpr Hex neighbour(Hex hex, int turns) { return hex + direction(turns); }

constexpr std::array<Hex, 6> neighbours(Hex hex) {
  std::array<Hex, 6> out{};
  for (int i = 0; i < 6; ++i)
    out[i] = hex + kDirections[i];
  return out;
}

// How many steps from `hex` to the centre (0, 0).
constexpr int length(Hex hex) {
  const int q = hex.q < 0 ? -hex.q : hex.q, r = hex.r < 0 ? -hex.r : hex.r,
            s = hex.s() < 0 ? -hex.s() : hex.s();
  return (q + r + s) / 2;
}

// How many steps from one hex to another.
constexpr int distance(Hex a, Hex b) { return length(a - b); }

// Turned `turns` x 60 degrees counter-clockwise about `centre`.
constexpr Hex rotated(Hex hex, int turns, Hex centre = {}) {
  Hex v = hex - centre;
  for (int i = 0; i < ((turns % 6) + 6) % 6; ++i)
    v = {-v.s(), -v.q}; // (q, r, s) -> (-s, -q, -r)
  return centre + v;
}

// Mirrored across the line through `centre` along the q axis (q and s
// swap places; r stays).
constexpr Hex reflected(Hex hex, Hex centre = {}) {
  const Hex v = hex - centre;
  return centre + Hex{v.s(), v.r};
}

// The hexes exactly `radius` steps from `centre`, starting from the one in
// direction 4 and going counter-clockwise (6 x radius of them; just the
// centre for radius 0).
inline std::vector<Hex> ring(Hex centre, int radius) {
  if (radius <= 0)
    return {centre};
  std::vector<Hex> out;
  out.reserve(std::size_t{6} * static_cast<std::size_t>(radius));
  Hex hex = centre + direction(4) * radius;
  for (int side = 0; side < 6; ++side) {
    for (int step = 0; step < radius; ++step) {
      out.push_back(hex);
      hex = neighbour(hex, side);
    }
  }
  return out;
}

// The hexes within `radius` steps of `centre`, ring by ring from the centre
// outwards (1 + 3 x radius x (radius + 1) of them).
inline std::vector<Hex> spiral(Hex centre, int radius) {
  std::vector<Hex> out{centre};
  for (int k = 1; k <= radius; ++k) {
    const std::vector<Hex> next = ring(centre, k);
    out.insert(out.end(), next.begin(), next.end());
  }
  return out;
}

// The hexes on the straight line from `a` to `b`, both included
// (distance + 1 of them), each one a step from the last. Where the line
// runs exactly along an edge it always takes the same side, so the result
// is the same every time (and on every platform: it's exact integer
// arithmetic, nudged the way Red Blob Games' guide nudges its floats).
inline std::vector<Hex> line(Hex a, Hex b) {
  const int steps = distance(a, b);
  if (steps == 0)
    return {a};
  // Each point along the line, scaled by kScale x steps so it's an integer,
  // and nudged off the edges between hexes
  constexpr std::int64_t kScale = 1000000;
  const std::int64_t whole = kScale * steps;
  const auto roundedDiv = [](std::int64_t value, std::int64_t by) {
    const std::int64_t half = by / 2;
    return value >= 0 ? (value + half) / by : -((-value + half) / by);
  };
  std::vector<Hex> out;
  out.reserve(static_cast<std::size_t>(steps) + 1);
  for (int i = 0; i <= steps; ++i) {
    const std::int64_t q = std::int64_t{a.q} * whole + std::int64_t{b.q - a.q} * i * kScale + 1;
    const std::int64_t r = std::int64_t{a.r} * whole + std::int64_t{b.r - a.r} * i * kScale + 2;
    const std::int64_t s = -q - r; // nudged by -3
    std::int64_t rq = roundedDiv(q, whole), rr = roundedDiv(r, whole), rs = roundedDiv(s, whole);
    const std::int64_t dq = std::llabs(rq * whole - q), dr = std::llabs(rr * whole - r),
                       ds = std::llabs(rs * whole - s);
    // The cube coordinates must sum to 0: fix the one that rounded furthest
    if (dq > dr && dq > ds)
      rq = -rr - rs;
    else if (dr > ds)
      rr = -rq - rs;
    out.push_back({static_cast<int>(rq), static_cast<int>(rr)});
  }
  return out;
}

} // namespace HexGrid

template <> struct std::hash<HexGrid::Hex> {
  std::size_t operator()(const HexGrid::Hex &hex) const noexcept {
    return std::hash<std::uint64_t>{}(
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(hex.q)) << 32) |
        static_cast<std::uint32_t>(hex.r));
  }
};
