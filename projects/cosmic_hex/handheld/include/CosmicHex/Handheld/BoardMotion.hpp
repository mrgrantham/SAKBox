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

#include <array>
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

// A ring of dust where something lands on the board (a building, the
// pointer's tap, the home die set back in its place), `dust` of the way
// from the touch (0) to gone (1): spreading out fast then slowing, to
// `spreadsTo` times its ring's size, a pale dust fading as it goes
struct DustRing {
  float spread = 1;
  ScratchPadd::Scene::Material material;
};
DustRing dustRing(float dust, float spreadsTo = 3);

// A ripple of space where the home die leaves its place and where it comes
// back, as though through a wormhole, `ripple` of the way from its start
// (0) to gone (1): two rings (the board's hex ring), the second starting a
// little after the first (overlap), each spreading out from inside the
// tile's edge and turning as it goes, the two opposite ways (a swirl), a
// pale violet showing quickly and fading slowly. Each ring's spread (times
// the ring), turn (radians about the up axis) and look
struct RippleRing {
  float spread = 1, turn = 0;
  ScratchPadd::Scene::Material material;
};
std::array<RippleRing, 2> spaceRipple(float ripple);

class BoardMotion {
public:
  BoardMotion() = default;
  // The moves from `before` to `after`; ships that didn't move keep the
  // heading they had at the end of `last` (when ships came or went too: a
  // ship where one of its owner's was keeps that one's)
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
  // Which way ship `ship` (of `after`) faces once it's arrived, and the
  // blockade once it's landed (ShipPose::heading's angle): what the flat
  // board draws, so both screens point a ship the same way
  float restingHeading(std::size_t ship) const;
  float blockadeHeading() const { return blockadeHeading_; }

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
