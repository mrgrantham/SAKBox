// DiceTray.hpp - Cosmic Hex's dice, thrown in Cyclone (round 87, the user's
// design). The top screen is a glass container: a floor, four walls at the
// screen's edges and a lid. Two dice are thrown in from one side, bounce off
// the walls and each other, and settle; the faces on top are the roll. Then
// the container is pulled off the screen: the dice are still simulated and
// slide on the glass floor (its friction) as it accelerates away.
//
// The throw comes from a seed (where the dice start, how fast and which way
// they go, how they spin), so the same seed throws the same way on the same
// machine. The physics decides the numbers; DiceFairness checks that each
// face comes up a sixth of the time.
//
//   DiceTray tray;
//   tray.throwDice(seed);
//   while (!tray.settled()) tray.step(1.0 / 60);
//   auto [a, b] = tray.faces();       // 1 to 6 each
//   tray.startPull();                 // then step() until offScreen()
//
// Units: a die is 1 across; the container is kWidth by kDepth (the top
// screen's shape), the floor at y = 0, y up, looking down from above.
#pragma once

#include <cyclone/collide_fine.h>
#include <cyclone/world.h>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

namespace CosmicHex::Dice {

class DiceTray {
public:
  static constexpr double kWidth = 10;     // across the screen (x)
  static constexpr double kDepth = 6;      // up the screen (z): 5:3, as the 3DS's top screen
  static constexpr double kHeight = 4;     // the lid's height
  static constexpr double kDieSize = 1.2;  // a die's edge (round 89: a little bigger)
  static constexpr double kFriction = 0.4; // the glass floor and walls
  static constexpr double kBounce = 0.35;  // restitution: how much a die bounces back
  // How fast the glass speeds up as it's pulled away (units a second, a
  // second); the setup's boards slide on the same curve (round 90)
  static constexpr double kPullAcceleration = 30;

  DiceTray();
  // (the dice's shapes point at their own bodies: a tray stays where it's made)
  DiceTray(const DiceTray &) = delete;
  DiceTray &operator=(const DiceTray &) = delete;

  // Two dice in from the left at the seed's speed, height and spin
  void throwDice(std::uint64_t seed);
  // Moves the simulation on by `seconds` (call at a steady rate, 1/60 s)
  void step(double seconds);
  // Both dice at rest (asleep, or barely moving for half a second)
  bool settled() const { return settled_; }
  // The faces on top, 1 to 6 (opposite faces add to 7)
  std::pair<int, int> faces() const;

  // The pull: the container accelerates off to the left; the dice ride it,
  // sliding on its floor. offset() is how far the container has gone, to
  // draw everything shifted by
  void startPull();
  double offset() const { return offset_; }
  bool offScreen() const { return offset_ > kWidth * 1.2; }

  // A die's place and turn (for drawing), in the container's frame
  cyclone::Vector3 position(int die) const;
  cyclone::Quaternion orientation(int die) const;

  // How long the throw took to settle, in seconds (for tuning and tests)
  double secondsToSettle() const { return settleTime_; }

private:
  // A die: its box shape, and the body it moves with (the shape points at it)
  struct Die : cyclone::CollisionBox {
    cyclone::RigidBody rigid;
  };
  int topFace(const Die &die) const;

  cyclone::World world_;
  std::array<Die, 2> dice_;
  bool settled_ = false;
  double still_ = 0; // seconds both dice have been barely moving
  double time_ = 0;  // since the throw
  double settleTime_ = 0;
  bool pulling_ = false;
  double pullSpeed_ = 0, offset_ = 0;
};

} // namespace CosmicHex::Dice
