#include <CosmicHex/Handheld/RollDice.hpp>

namespace CosmicHex::Handheld {

RollDice::RollDice(double stepSeconds, int mostStepsAFrame)
    : step_(stepSeconds), mostSteps_(mostStepsAFrame) {}

void RollDice::throwFrom(std::uint64_t seed, double now) {
  seed_ = seed;
  tray_.throwDice(seed);
  stage_ = Stage::Rolling;
  owed_ = 0;
  last_ = thrownAt_ = now;
}

std::optional<std::pair<int, int>> RollDice::update(double now) {
  if (stage_ == Stage::Idle)
    return std::nullopt;
  // Fixed steps, as many as the clock has moved on (Gregory, Game Engine
  // Architecture 8.5: integrators want a constant step; Doom's tics, Black
  // Book 5.4): the same throw whatever the frame rate (the DS runs 41 to
  // 60), at most mostSteps a frame so a stall slows the dice rather than
  // snowballing
  owed_ += now - last_;
  last_ = now;
  int steps = 0;
  for (; owed_ >= step_ && steps < mostSteps_; ++steps) {
    tray_.step(step_);
    owed_ -= step_;
  }
  if (steps == mostSteps_)
    owed_ = 0; // (behind: let it go)
  // A watchdog (White, Making Embedded Systems, "Watchdog"): dice still
  // moving after kMostSecondsRolling are thrown again (the next seed), so a
  // roll can't hold the game up (none of 60,000 test throws needed it)
  if (stage_ == Stage::Rolling && !tray_.settled() && now - thrownAt_ > kMostSecondsRolling) {
    tray_.throwDice(++seed_);
    thrownAt_ = now;
    return std::nullopt;
  }
  if (stage_ == Stage::Rolling && tray_.settled()) {
    stage_ = Stage::Showing;
    shownSeconds_ = 0;
    rolled_ = tray_.faces();
  } else if (stage_ == Stage::Showing && (shownSeconds_ += steps * step_) >= kSecondsShowing) {
    tray_.startPull();
    stage_ = Stage::Pulling;
    return tray_.faces();
  } else if (stage_ == Stage::Pulling && tray_.offScreen()) {
    stage_ = Stage::Idle;
  }
  return std::nullopt;
}

void RollDice::skip(double now) {
  if (stage_ == Stage::Rolling) {
    const auto most = static_cast<int>(kMostSecondsRolling / step_);
    for (int i = 0; i < most && !tray_.settled(); ++i)
      tray_.step(step_);
    if (!tray_.settled())
      return; // (the watchdog throws them again)
    stage_ = Stage::Showing;
    shownSeconds_ = 0;
    rolled_ = tray_.faces();
    owed_ = 0;
    last_ = now;
  } else if (stage_ == Stage::Showing) {
    shownSeconds_ = kSecondsShowing; // (sent and pulled next frame)
  } else if (stage_ == Stage::Pulling) {
    stage_ = Stage::Idle;
  }
}

} // namespace CosmicHex::Handheld
