// WinCelebration.hpp - the winner's ships flying round above the board
// (the user's): each takes off from where it is and joins a ring circling
// over the board, nose first, flames burning, swivelled inwards to hold the
// turn. A winner with fewer than three ships on the board gets the rest
// faded in at their colonies, taking off from there.
//
//   WinCelebration party(winnersShips, winnersColonies, now);
//   for (std::size_t i = 0; i < party.ships(); ++i) {
//     const ShipPose pose = party.pose(i, now); // (BoardMotion's ShipPose)
//     draw it, at party.fadedIn(i, now) of its strength
//   }
//   if (party.over(now)) on to the title
#pragma once

#include <CosmicHex/Handheld/BoardMotion.hpp>

#include <ScratchPadd/Scene/Types.hpp>

#include <vector>

namespace CosmicHex::Handheld {

class WinCelebration {
public:
  static constexpr float kSeconds = 6;          // (all told)
  static constexpr float kSecondsUp = 1.6f;     // (taking off, into the ring)
  static constexpr float kSecondsFadeIn = 0.5f; // (the extra ships appearing)
  static constexpr float kRadius = 3.2f;        // (the ring, round the board's middle)
  static constexpr float kHeight = 2.2f;        // (above the board)
  static constexpr float kTurnsASecond = 0.18f;
  static constexpr std::size_t kAtLeast = 3;

  WinCelebration(const std::vector<ScratchPadd::Scene::Vector3> &ships,
                 const std::vector<ScratchPadd::Scene::Vector3> &colonies, double now);

  std::size_t ships() const { return from_.size(); }
  ShipPose pose(std::size_t ship, double now) const;
  // How far ship `ship` has faded in (1: the winner's own ships, there all along)
  float fadedIn(std::size_t ship, double now) const;
  bool over(double now) const { return now - start_ >= kSeconds; }

private:
  ScratchPadd::Scene::Vector3 at(std::size_t ship, float t) const;

  std::vector<ScratchPadd::Scene::Vector3> from_;
  std::size_t own_ = 0; // (the first own_ were on the board; the rest faded in)
  double start_;
};

} // namespace CosmicHex::Handheld
