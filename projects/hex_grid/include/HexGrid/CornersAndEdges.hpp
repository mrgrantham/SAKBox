// CornersAndEdges.hpp - the corners where three hexes meet and the edges
// where two meet, for games that build on corners and move along edges
// rather than (or as well as) on the hexes themselves.
//
//   HexGrid::Corner spot = HexGrid::corner(planet, 2);        // one of its six
//   for (HexGrid::Hex h : HexGrid::hexesTouching(spot)) ...  // the 3 hexes there
//   for (HexGrid::Corner next : HexGrid::neighbours(spot)) ...  // 3 corners an edge away
//   HexGrid::Edge lane = HexGrid::edge(planet, 0);           // towards +q
//   auto [from, to] = HexGrid::endpoints(lane);              // its two corners
//
// Corners and edges are worked out from hexes, so they're integer
// arithmetic like the rest, and come out the same on every platform.
// Paths.hpp searches corners as it does hexes (a corner's neighbours are
// the corners an edge away), and Layout places them on screen.
//
// How they're stored. Each corner is shared by three hexes and each edge
// by two, so to give each one a single name, every hex "owns" two of its
// corners (corners 0 and 1) and three of its edges (edges 0, 1 and 2); the
// rest belong to its neighbours. `corner(hex, i)` and `edge(hex, i)` take
// any of the six and give back the owner's name for it.
#pragma once

#include <HexGrid/Hex.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace HexGrid {

// A point where three hexes meet: corner `index` (0 or 1) of `hex`.
// Corner i of a hex is the one between its neighbours in directions i and
// i + 1, so corner 0 touches the hex and its neighbours 0 and 1, and corner
// 1 touches it and its neighbours 1 and 2.
struct Corner {
  Hex hex;
  int index = 0;

  constexpr bool operator==(const Corner &) const = default;
  constexpr auto operator<=>(const Corner &) const = default;
};

// The side two neighbouring hexes share: edge `index` (0, 1 or 2) of `hex`,
// the one towards its neighbour in direction `index`.
struct Edge {
  Hex hex;
  int index = 0;

  constexpr bool operator==(const Edge &) const = default;
  constexpr auto operator<=>(const Edge &) const = default;
};

// Corner `i` of `hex` (any integer, counted counter-clockwise like the
// directions): the one between its neighbours in directions i and i + 1.
constexpr Corner corner(Hex hex, int i) {
  // Corners 0 and 1 are the hex's own; each of the other four is corner 0
  // or 1 of the neighbour that owns it.
  switch (((i % 6) + 6) % 6) {
  case 0:
    return {hex, 0};
  case 1:
    return {hex, 1};
  case 2:
    return {hex + direction(3), 0};
  case 3:
    return {hex + direction(4), 1};
  case 4:
    return {hex + direction(4), 0};
  default:
    return {hex + direction(5), 1};
  }
}

// The hex's six corners, in order (corner 0 first).
constexpr std::array<Corner, 6> corners(Hex hex) {
  std::array<Corner, 6> out{};
  for (int i = 0; i < 6; ++i)
    out[i] = corner(hex, i);
  return out;
}

// The three hexes that meet at a corner.
constexpr std::array<Hex, 3> hexesTouching(Corner c) {
  return {c.hex, c.hex + direction(c.index), c.hex + direction(c.index + 1)};
}

// Edge `i` of `hex` (any integer): the side it shares with its neighbour in
// direction i.
constexpr Edge edge(Hex hex, int i) {
  const int side = ((i % 6) + 6) % 6;
  if (side < 3)
    return {hex, side};
  // Edges 3, 4 and 5 belong to the neighbour on that side, as its 0, 1, 2
  return {hex + direction(side), side - 3};
}

// The hex's six edges, in order (edge 0 first).
constexpr std::array<Edge, 6> edges(Hex hex) {
  std::array<Edge, 6> out{};
  for (int i = 0; i < 6; ++i)
    out[i] = edge(hex, i);
  return out;
}

// The two hexes either side of an edge.
constexpr std::array<Hex, 2> hexesTouching(Edge e) { return {e.hex, e.hex + direction(e.index)}; }

// The two corners at the ends of an edge. Edge i of a hex runs between its
// corners i - 1 and i (the corners either side of direction i).
constexpr std::array<Corner, 2> endpoints(Edge e) {
  return {corner(e.hex, e.index - 1), corner(e.hex, e.index)};
}

// The three edges that meet at a corner: one between each pair of the
// hexes there.
constexpr std::array<Edge, 3> edgesMeeting(Corner c) {
  const Hex here = c.hex;
  return {edge(here, c.index), edge(here, c.index + 1),
          // between the two neighbours: from neighbour `index`, the edge
          // towards neighbour `index + 1` is in direction index + 2
          edge(here + direction(c.index), c.index + 2)};
}

// The three corners an edge away from a corner (where a piece moving along
// edges can go in one step).
constexpr std::array<Corner, 3> neighbours(Corner c) {
  std::array<Corner, 3> out{};
  const std::array<Edge, 3> meeting = edgesMeeting(c);
  for (int i = 0; i < 3; ++i) {
    const std::array<Corner, 2> ends = endpoints(meeting[i]);
    out[i] = ends[0] == c ? ends[1] : ends[0];
  }
  return out;
}

// The edge joining two corners, if they're neighbours.
constexpr std::optional<Edge> edgeBetween(Corner a, Corner b) {
  for (Edge e : edgesMeeting(a)) {
    const std::array<Corner, 2> ends = endpoints(e);
    if (ends[0] == b || ends[1] == b)
      return e;
  }
  return std::nullopt;
}

// Every corner of the given hexes (a board's), each once, sorted.
inline std::vector<Corner> everyCorner(const std::vector<Hex> &hexes) {
  std::vector<Corner> out;
  out.reserve(hexes.size() * 2 + 6);
  for (Hex hex : hexes)
    for (Corner c : corners(hex))
      out.push_back(c);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

// Every edge of the given hexes (a board's), each once, sorted.
inline std::vector<Edge> everyEdge(const std::vector<Hex> &hexes) {
  std::vector<Edge> out;
  out.reserve(hexes.size() * 3 + 6);
  for (Hex hex : hexes)
    for (Edge e : edges(hex))
      out.push_back(e);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

} // namespace HexGrid

template <> struct std::hash<HexGrid::Corner> {
  std::size_t operator()(const HexGrid::Corner &c) const noexcept {
    return std::hash<HexGrid::Hex>{}(c.hex) * 3 + static_cast<std::size_t>(c.index);
  }
};

template <> struct std::hash<HexGrid::Edge> {
  std::size_t operator()(const HexGrid::Edge &e) const noexcept {
    return std::hash<HexGrid::Hex>{}(e.hex) * 3 + static_cast<std::size_t>(e.index);
  }
};
