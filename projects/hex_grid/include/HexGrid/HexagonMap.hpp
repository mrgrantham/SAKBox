// HexagonMap.hpp - a value for every hex of a hexagon-shaped board, stored
// in one array (no hashing), so boards are cheap to copy (a bot copies the
// board for every move it tries) and the same in memory everywhere.
//
//   HexGrid::HexagonMap<Tile> board(5);   // 91 hexes, five from centre to edge
//   board[{2, -1}] = Tile::Planet;
//   if (board.contains(next)) ...
//   for (HexGrid::Hex hex : board.hexes()) ...   // ring by ring from the centre
#pragma once

#include <HexGrid/Hex.hpp>

#include <cassert>
#include <cstddef>
#include <vector>

namespace HexGrid {

template <typename T> class HexagonMap {
public:
  // Every hex within `radius` steps of the centre, each holding `fill`.
  explicit HexagonMap(int radius, const T &fill = T{})
      : radius_(radius), values_(static_cast<std::size_t>(count(radius)), fill),
        hexes_(spiral(Hex{}, radius)) {
    assert(radius >= 0);
    // Where each row of r starts in the array: row r holds the q values
    // from max(-radius, -radius - r) to min(radius, radius - r)
    int start = 0;
    for (int r = -radius; r <= radius; ++r) {
      rowStarts_.push_back(start - firstQ(r));
      start += 2 * radius + 1 - (r < 0 ? -r : r);
    }
  }

  // How many hexes a hexagon of this radius has: 1 + 3 x radius x (radius + 1).
  static constexpr int count(int radius) { return 1 + 3 * radius * (radius + 1); }

  int radius() const { return radius_; }
  std::size_t size() const { return values_.size(); }
  bool contains(Hex hex) const { return length(hex) <= radius_; }

  T &operator[](Hex hex) { return values_[index(hex)]; }
  const T &operator[](Hex hex) const { return values_[index(hex)]; }

  // Every hex on the board, ring by ring from the centre (spiral order).
  const std::vector<Hex> &hexes() const { return hexes_; }

  // The board's neighbours of `hex` (those of its six that are on it), in
  // direction order.
  std::vector<Hex> neighboursOn(Hex hex) const {
    std::vector<Hex> out;
    for (Hex next : neighbours(hex)) {
      if (contains(next))
        out.push_back(next);
    }
    return out;
  }

  bool operator==(const HexagonMap &) const = default;

private:
  int firstQ(int r) const { return r < 0 ? -radius_ - r : -radius_; }

  // The rows of r one after another, each holding its q values in order
  std::size_t index(Hex hex) const {
    assert(contains(hex));
    const int row = hex.r + radius_;
    const int at = rowStarts_[static_cast<std::size_t>(row)] + hex.q;
    return static_cast<std::size_t>(at);
  }

  int radius_;
  std::vector<T> values_;
  std::vector<Hex> hexes_;
  std::vector<int> rowStarts_; // for each row, its first index minus its first q
};

} // namespace HexGrid
