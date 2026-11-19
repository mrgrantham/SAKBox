// BoardMotion.hpp - the board's animations between one move and the next:
// ships slide in a straight line from where they were to where they are,
// and planets and the zone's finds flip over when explored (the
// user's: "nothing fancy, a linear slide, an animated flip").
//
//   BoardMotion motion(before, after);      // when a move arrives
//   motion.shipAt(i, t)                     // where ship i is, t from 0 to 1
//   motion.planetTurn(i, t)                 // planet i's turn, if it's flipping
#pragma once

#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

class BoardMotion {
public:
  BoardMotion() = default;
  BoardMotion(const State &before, const State &after);

  // Where ship `ship` (of `after`) is at `t` (0: where it was, 1: where it
  // is): straight from its last corner. A new ship, or one when ships came
  // or went (one settled), is where it is
  ScratchPadd::Scene::Vector3 shipAt(std::size_t ship, float t) const;

  // How far a tile turning over has turned at `t`, in radians: 0 face down
  // to pi face up; nothing if it isn't turning
  std::optional<float> planetTurn(std::size_t planet, float t) const;
  std::optional<float> siteTurn(std::size_t site, float t) const;

private:
  std::vector<ScratchPadd::Scene::Vector3> shipFrom_, shipTo_;
  std::vector<bool> planetFlipping_, siteFlipping_;
};

} // namespace CosmicHex::Handheld
