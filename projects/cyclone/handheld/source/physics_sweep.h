// physics_sweep.h - the handheld ragdoll's sweep (R): it runs the ragdoll
// with each of a set of settings in turn (steps a second, sub-steps, frames
// a second), each from the same shove, and ranks what each did. Plain C++,
// tested on desktops (../tests/physics_sweep_test.cpp); the app feeds it
// each second's figures and logs and shows what it says.
//
// What's best: a setting that keeps up with real time (every second of its
// run, at least kKeepsUp of a second simulated), draws its frames (at least
// kFramesDrawn of them) and holds its joints (no joint stretched past
// kStretchLimit); among those, the most frames a second, then the shortest
// delay to the screen (one step and one frame: drawn blended between the
// last two steps, so a step behind), then the least busy physics. Settings
// that don't keep up come after, the closest to keeping up first.
#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace physics_sweep {

struct Setting {
  int stepsPerSecond = 30;
  int subSteps = 4;
  int framesPerSecond = 30;
};

// What one setting did over its run
struct Result {
  Setting setting;
  double realSeconds = 0;      // how long it ran
  double simulatedSeconds = 0; // how much it simulated meanwhile
  double steppingSeconds = 0;  // of realSeconds, how long the steps took
  int steps = 0;
  int framesDrawn = 0;
  double slowestSecond = 0; // the least simulated in any second of the run
  double worstStretch = 0;  // the most a joint was beyond its slack (m)
  int seconds = 0;          // whole seconds measured

  double stepMilliseconds() const { return steps ? 1000 * steppingSeconds / steps : 0; }
  double speed() const { return realSeconds > 0 ? simulatedSeconds / realSeconds : 0; }
  double busy() const { return realSeconds > 0 ? steppingSeconds / realSeconds : 0; }
  double framesPerSecond() const { return realSeconds > 0 ? framesDrawn / realSeconds : 0; }
  // From a change to the screen showing it: a step behind, and a frame
  double delayMilliseconds() const {
    return 1000.0 / setting.stepsPerSecond + 1000.0 / setting.framesPerSecond;
  }
};

inline constexpr double kKeepsUp = 0.95;          // of a second simulated, every second
inline constexpr double kFramesDrawn = 0.95;      // of the frames a second asked for
inline constexpr double kStretchLimit = 0.01;     // m: 1 cm
inline constexpr double kRunSimulatedSeconds = 5; // a run ends after simulating this
inline constexpr double kRunRealSeconds = 10;     // or after this, if it can't keep up

inline bool keepsUp(const Result &result) {
  return result.slowestSecond >= kKeepsUp &&
         result.framesPerSecond() >= kFramesDrawn * result.setting.framesPerSecond;
}
inline bool holdsItsJoints(const Result &result) { return result.worstStretch <= kStretchLimit; }
// Whether `a` ranks ahead of `b` (see the top)
inline bool ranksAhead(const Result &a, const Result &b) {
  const bool aGood = keepsUp(a) && holdsItsJoints(a), bGood = keepsUp(b) && holdsItsJoints(b);
  if (aGood != bGood)
    return aGood;
  if (!aGood) {
    if (keepsUp(a) != keepsUp(b))
      return keepsUp(a);
    return a.slowestSecond > b.slowestSecond;
  }
  if (a.setting.framesPerSecond != b.setting.framesPerSecond)
    return a.setting.framesPerSecond > b.setting.framesPerSecond;
  if (a.delayMilliseconds() != b.delayMilliseconds())
    return a.delayMilliseconds() < b.delayMilliseconds();
  return a.busy() < b.busy();
}
inline std::vector<Result> ranked(std::vector<Result> results) {
  std::stable_sort(results.begin(), results.end(), ranksAhead);
  return results;
}

// Every combination of these, fewest steps first
inline constexpr int kStepsPerSecond[] = {10, 15, 20, 30, 60};
inline constexpr int kSubSteps[] = {1, 2, 4, 8};
inline constexpr int kFramesPerSecond[] = {30, 60};
inline std::vector<Setting> allSettings() {
  std::vector<Setting> settings;
  for (const int steps : kStepsPerSecond)
    for (const int subSteps : kSubSteps)
      for (const int frames : kFramesPerSecond)
        settings.push_back({steps, subSteps, frames});
  return settings;
}

class Sweep {
public:
  explicit Sweep(std::vector<Setting> settings = allSettings()) : settings_(std::move(settings)) {}

  void start() {
    results_.clear();
    index_ = 0;
    running_ = !settings_.empty();
    beginRun();
  }
  void stop() { running_ = false; }
  bool running() const { return running_; }
  // Once it's run every setting
  bool finished() const { return !running_ && results_.size() == settings_.size(); }
  const Setting &current() const { return settings_[index_]; }
  std::size_t index() const { return index_; }
  std::size_t count() const { return settings_.size(); }
  const std::vector<Result> &results() const { return results_; }

  // A second of the current run: how long it really was, how much it
  // simulated, how long its steps took and how many, the frames drawn, and
  // the worst stretch since the run began. Returns true when this ends the
  // run: its Result is then results().back(), and current() is the next
  // setting (or running() is false, the sweep done).
  bool addSecond(double realSeconds, double simulatedSeconds, double steppingSeconds, int steps,
                 int framesDrawn, double worstStretch) {
    if (!running_)
      return false;
    Result &run = run_;
    run.realSeconds += realSeconds;
    run.simulatedSeconds += simulatedSeconds;
    run.steppingSeconds += steppingSeconds;
    run.steps += steps;
    run.framesDrawn += framesDrawn;
    run.worstStretch = std::max(run.worstStretch, worstStretch);
    const double thisSecond = realSeconds > 0 ? simulatedSeconds / realSeconds : 0;
    run.slowestSecond = run.seconds == 0 ? thisSecond : std::min(run.slowestSecond, thisSecond);
    ++run.seconds;
    if (run.simulatedSeconds < kRunSimulatedSeconds && run.realSeconds < kRunRealSeconds)
      return false;
    results_.push_back(run);
    if (++index_ == settings_.size()) {
      running_ = false;
      index_ = 0;
    } else {
      beginRun();
    }
    return true;
  }

private:
  void beginRun() { run_ = Result{.setting = settings_[index_]}; }

  std::vector<Setting> settings_;
  std::vector<Result> results_;
  Result run_;
  std::size_t index_ = 0;
  bool running_ = false;
};

} // namespace physics_sweep
