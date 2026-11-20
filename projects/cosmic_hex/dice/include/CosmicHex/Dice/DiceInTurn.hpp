// DiceInTurn.hpp - dice thrown into the glass one after another, each to
// land on a number already chosen: who goes first (the user's), a die for
// each player, thrown in turn, each staying where it settles.
//
// The physics decides how a die lands; the game has already decided the
// number. A die is the same whichever number is on which face, so each
// throw is rehearsed first, unseen, from the same start (a seed): that
// shows which side will land up, and the die is drawn with its numbers
// turned so that side shows the chosen number (opposite faces still add
// to 7). Then it's thrown for real and lands exactly as rehearsed: the
// steps are fixed and the start is the same. Dice already settled are
// fixed in place, so a later die bounces off them but can't turn them over.
//
//   DiceInTurn dice(1.0 / 30);                 // the step every step() takes
//   dice.throwDie(0, seed, 4);                 // die 0, to show a 4
//   while (!dice.settled(0)) dice.step();
//   dice.settleNow(0);                         // or: at rest at once (A)
//   draw die i at dice.position(i), turned dice.orientation(i)
//
// The container is DiceTray's (its size, walls and glass).
#pragma once

#include <cyclone/collide_fine.h>
#include <cyclone/world.h>

#include <array>
#include <cstdint>
#include <memory>

namespace CosmicHex::Dice {

class DiceInTurn {
public:
  static constexpr int kMostDice = 4;

  explicit DiceInTurn(double stepSeconds);
  ~DiceInTurn();
  DiceInTurn(const DiceInTurn &) = delete;
  DiceInTurn &operator=(const DiceInTurn &) = delete;

  // Die `die` thrown from `seed` to land showing `face` (1 to 6). A die
  // still moving is set at rest first (settleNow)
  void throwDie(int die, std::uint64_t seed, int face);
  // One step of the simulation (stepSeconds)
  void step();

  bool thrown(int die) const;
  bool settled(int die) const;
  // The moving die put where it lands, at once (impatient: the user's A)
  void settleNow(int die);
  // Whether any thrown die is still moving
  bool moving() const { return moving_ >= 0; }

  // Where die `die` is and how it's drawn turned (its numbers turned
  // included), in the container's frame (DiceTray's units)
  cyclone::Vector3 position(int die) const;
  cyclone::Quaternion orientation(int die) const;

private:
  struct Die;
  struct Slot {
    bool thrown = false, settled = false;
    cyclone::Vector3 restAt;
    cyclone::Quaternion restTurn;
    cyclone::Quaternion numbersTurned; // (the drawing's turn inside the die)
  };
  // A world with the glass, the settled dice fixed, and die `die` thrown from
  // `seed`: built the same way for the rehearsal and for real
  struct Throw;
  std::unique_ptr<Throw> makeThrow(int die, std::uint64_t seed) const;

  double step_;
  std::array<Slot, kMostDice> slots_{};
  std::unique_ptr<Throw> live_;
  int moving_ = -1;
  double still_ = 0, time_ = 0;
};

// The turn inside a die that puts its `face` side where its `up` side is
// (both 1 to 6, as DiceTray numbers them: +x 1, -x 6, +y 2, -y 5, +z 3,
// -z 4): on its own, to test
cyclone::Quaternion turnFaceTo(int face, int up);

} // namespace CosmicHex::Dice
