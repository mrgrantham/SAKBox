// RollDice.hpp - a roll's two physics dice (Dice::DiceTray) on the top
// screen, from the throw to the glass pulled away: thrown from the table's
// seed, stepped at a fixed rate on the game's clock until they settle,
// shown a moment (the roll popping in over them), their faces handed back
// once to send to the table, then the glass pulled off the screen.
//
//   RollDice dice(1.0 / 30, 2);        // the step, and the most steps a frame
//   dice.throwFrom(seed, now);
//   if (const auto faces = dice.update(now)) send(DiceRolled{faces->first, faces->second});
//   drawRoll(frame, dice);              // (TopViews)
#pragma once

#include <CosmicHex/Dice/DiceTray.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace CosmicHex::Handheld {

class RollDice {
public:
  enum class Stage : std::uint8_t { Idle, Rolling, Showing, Pulling };
  static constexpr double kSecondsShowing = 1.0;   // (the pop, the words, a moment to read)
  static constexpr float kSecondsPop = 0.3f;       // (the roll's number popping in)
  static constexpr double kMostSecondsRolling = 8; // (then thrown again: a watchdog)

  RollDice(double stepSeconds, int mostStepsAFrame);

  // Thrown from `seed` at `now` (the game's clock)
  void throwFrom(std::uint64_t seed, double now);
  // Each frame: the physics stepped to `now`; the faces, once, when they've
  // been shown long enough (to send to the table), the glass then pulled
  std::optional<std::pair<int, int>> update(double now);
  // A stage on (B, or Animations Off): rolling, the physics run on to
  // where they land (all at once: a moment on a DS) and the roll shown;
  // showing, sent next frame and pulled; being pulled, gone
  void skip(double now);
  void stop() { stage_ = Stage::Idle; } // (the game stopped)

  Stage stage() const { return stage_; }
  bool idle() const { return stage_ == Stage::Idle; }
  std::pair<int, int> rolled() const { return rolled_; }
  float shownSeconds() const { return static_cast<float>(shownSeconds_); }
  const Dice::DiceTray &tray() const { return tray_; }

private:
  double step_;
  int mostSteps_;
  Dice::DiceTray tray_;
  Stage stage_ = Stage::Idle;
  std::uint64_t seed_ = 0;
  double owed_ = 0, last_ = 0, thrownAt_ = 0, shownSeconds_ = 0;
  std::pair<int, int> rolled_{};
};

} // namespace CosmicHex::Handheld
