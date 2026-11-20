// BoardMotion.hpp - the board's animations between one move and the next:
// ships fly from where they were to where they are, and planets and the
// zone's anomalies flip over when explored.
//
// A ship flies like a rocket (the user's: a tiny flame that swivels to
// steer, like a Falcon 9): along a shallow arc, nose first, speeding up
// then slowing (the Gentle curve's ease in and out); while it speeds up
// its engine burns, the flame as long as the push, swivelled (gimballed)
// against the push's sideways part, which turns the ship along the arc.
// It then coasts in, its flame out, and keeps facing the way it flew.
//
// A station or colony built drops in from above (falling faster and
// faster), squashes a little as it lands and springs back (a settle), and
// a ring of dust spreads from it and fades.
//
//   BoardMotion motion(before, after, &lastMotion); // when a move arrives
//   const ShipPose pose = motion.ship(i, t);         // t from 0 to 1
//   motion.planetTurn(i, t)                          // planet i's turn, if it's flipping
#pragma once

#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <optional>
#include <vector>

namespace CosmicHex::Handheld {

// A ship at a moment of its flight: where, which way its nose points (an
// angle about the up axis: 0 along +x, as the ship's mesh points), and its
// engine's flame: how strong (0 out, to 1) and swivelled how far from
// straight back (radians about the up axis, the ship's own)
struct ShipPose {
  ScratchPadd::Scene::Vector3 at;
  float heading = 0;
  float flame = 0;
  float gimbal = 0;
};

// A building at a moment of its landing: how high above its place, how
// squashed (its height times this), and its dust ring once it's down (0
// just landed to 1 gone)
struct Landing {
  float drop = 0;
  float squash = 1;
  std::optional<float> dust;
};

class BoardMotion {
public:
  BoardMotion() = default;
  // The moves from `before` to `after`; ships that didn't move keep the
  // heading they had at the end of `last`
  BoardMotion(const State &before, const State &after, const BoardMotion *last = nullptr);

  // Whether anything moves: a ship flying, a building landing, a tile
  // turning over, or ships coming or going (a snapshot that changes none
  // of them shouldn't restart what's moving)
  bool anything() const;

  // Ship `ship` (of `after`) at `t` (0: where it was, 1: where it is). A
  // new ship, or one when ships came or went (one settled), is where it is
  ShipPose ship(std::size_t ship, float t) const;
  // The blockade (the pirates' ship) at `t`: swooping from the planet it
  // was on, up off the board and diving down onto the next, as a ship
  // flies; dropping onto the first; nothing when there's none
  std::optional<ShipPose> blockade(float t) const;
  static constexpr float kSwoop = 1.2f; // (how high it swoops, in hexes)
  ScratchPadd::Scene::Vector3 shipAt(std::size_t ship, float t) const {
    return this->ship(ship, t).at;
  }

  // Station `station` (of `after`) at `t`: one built (new, or a station
  // become a colony) landing; any other where it is. `t` runs on past 1
  // while the dust fades (to kLandingEnds)
  Landing landing(std::size_t station, float t) const;
  static constexpr float kDropHeight = 1.4f; // (hexes above the board)
  static constexpr float kLanded = 0.55f;    // (when it touches down)
  static constexpr float kLandingEnds = 1.6f;

  // How far the arc bows out, as a share of the flight's length
  static constexpr float kBow = 0.12f;
  // The most a flame swivels from straight back
  static constexpr float kMostGimbal = 0.5f;

  // How far a tile turning over has turned at `t`, in radians: 0 face down
  // to pi face up; nothing if it isn't turning
  std::optional<float> planetTurn(std::size_t planet, float t) const;
  std::optional<float> siteTurn(std::size_t site, float t) const;

private:
  // A flight from one place to another (BoardMotion's ships and blockade):
  // along an arc bowing to the left, hopping `hop` up off the table
  struct Flight {
    ScratchPadd::Scene::Vector3 from, to;
    float hop = 0;
    ScratchPadd::Scene::Vector3 at(float t) const;
    ShipPose pose(float t, float restingHeading) const;
    std::optional<float> arrivingHeading() const; // (none if it doesn't go anywhere)
  };

  std::vector<ScratchPadd::Scene::Vector3> shipFrom_, shipTo_;
  std::vector<float> headingAtRest_; // (each ship's, once it's arrived)
  std::vector<bool> planetFlipping_, siteFlipping_, stationBuilt_;
  bool shipsCameOrWent_ = false;
  std::optional<Flight> blockade_;
  bool blockadeMoves_ = false;
  float blockadeHeading_ = 0;
};

} // namespace CosmicHex::Handheld
