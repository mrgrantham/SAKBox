// ScorePops.hpp - points and badges popping in as they're won (the user's):
// each new dash of a player's points bar, and each new badge, grows out
// past its size and settles back (Motion's Settle: a small overshoot), the
// dashes one after another when several come at once.
//
//   ScorePops pops;
//   pops.saw(points, badges, now);                // each look at the game
//   const float size = pops.dash(player, dash, now); // 1: its own size
#pragma once

#include <vector>

namespace CosmicHex::Handheld {

class ScorePops {
public:
  static constexpr float kSeconds = 0.45f; // (each pop)
  static constexpr float kStagger = 0.1f;  // (between dashes won together)

  // Each player's points and badges now: any risen since the last look pop
  // from `now` (the first look pops nothing: what was there already)
  void saw(const std::vector<int> &points, const std::vector<int> &badges, double now);

  // How big player `player`'s dash `dash` (0 the first point) or badge
  // `badge` is drawn: 0 not yet in, past 1 popping, 1 its size
  float dash(int player, int dash, double now) const;
  float badge(int player, int badge, double now) const;
  bool popping(double now) const;

private:
  struct Rise {
    int from = 0, to = 0;
    double at = -1e9;
  };
  static float sizeOf(const std::vector<Rise> &rises, int player, int index, double now);
  static void risen(std::vector<Rise> &rises, const std::vector<int> &now, double at, bool first);

  std::vector<Rise> points_, badges_;
  bool seen_ = false;
};

} // namespace CosmicHex::Handheld
