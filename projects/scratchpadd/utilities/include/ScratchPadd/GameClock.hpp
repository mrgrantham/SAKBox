// GameClock.hpp - a game's own time, apart from real time (Gregory, Game
// Engine Architecture, 8.4 "Abstract Timelines"): it runs while the game
// runs, stops while it's paused, and can run faster or slower, while the
// program's loop goes on in real time. Animations and simulations read it,
// so pausing the game freezes them too.
//
//   GameClock clock;                    // starts at 0, running
//   clock.setPaused(true);              // game time stops...
//   clock.setRate(2);                   // ...and, once going again, runs twice as fast
//   double t = clock.seconds();         // game seconds so far
//
// Every call can be given the real time to use (for tests); by default it's
// now.
#pragma once

#include <chrono>

namespace ScratchPadd {

class GameClock {
public:
  using RealClock = std::chrono::steady_clock;

  explicit GameClock(RealClock::time_point now = RealClock::now()) : since_(now) {}

  // Game seconds since the clock started
  double seconds(RealClock::time_point now = RealClock::now()) const {
    if (paused_)
      return banked_;
    return banked_ + rate_ * std::chrono::duration<double>(now - since_).count();
  }

  bool paused() const { return paused_; }
  void setPaused(bool paused, RealClock::time_point now = RealClock::now()) {
    if (paused == paused_)
      return;
    bank(now);
    paused_ = paused;
  }

  // How fast game time runs against real time (1: the same)
  double rate() const { return rate_; }
  void setRate(double rate, RealClock::time_point now = RealClock::now()) {
    bank(now);
    rate_ = rate;
  }

private:
  // The time so far kept, and counted afresh from `now`
  void bank(RealClock::time_point now) {
    banked_ = seconds(now);
    since_ = now;
  }

  RealClock::time_point since_;
  double banked_ = 0;
  double rate_ = 1;
  bool paused_ = false;
};

} // namespace ScratchPadd
