// Motion.hpp - things moving over time, the same on every platform (round
// 97, the user's): a value carried from one place to another along a curve
// (Gregory, Game Engine Architecture vol. 2, 13.6: ease curves), short
// scripts of such moves one after another or together, and a fixed-step
// clock for physics, so a body simulated by Cyclone can drive a move too
// (13.7.3: the physics sets the pose, copied across after each step). It
// reads a time given to it, usually the game's (GameClock), so pausing the
// game freezes every move.
//
//   Motion::Track<float> rise{.from = 192, .to = 148, .start = now, .seconds = 0.25f,
//                             .curve = Motion::Curve::EaseInOut};
//   float y = rise.at(clock.seconds());     // where it is now
//
//   Motion::Script card;                     // a card: up, then a turn
//   const auto up = card.add(0.25f);         // starts at once, a quarter second
//   const auto turn = card.after(up, 0.2f);  // when it ends
//   float u = card.progress(turn, elapsed, Motion::Curve::EaseOut);
//
//   Motion::FixedSteps physics(1.0 / 60);    // steps due by now, at most 4
//   for (int n = physics.due(clock.seconds(), 4); n > 0; --n) world.step(1.0 / 60);
#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace ScratchPadd::Motion {

// How far along (0 to 1) a move is when its time is `t` of the way through
// (0 to 1), clamped to 0 and 1:
//   Linear     straight, at an even speed
//   EaseIn     starting slowly, speeding up (t squared)
//   EaseOut    starting fast, slowing to stop
//   EaseInOut  slow, fast, slow (smoothstep: 3t^2 - 2t^3)
//   Pull       speeding up evenly to the middle, then slowing the same way:
//              two parabolas, the curve of something pulled at a constant
//              acceleration then braked (Cosmic Hex's dice glass)
enum class Curve { Linear, EaseIn, EaseOut, EaseInOut, Pull };
float along(Curve curve, float t);

// A value carried from `from` to `to`, starting at `start` (seconds, on
// the caller's clock) and taking `seconds`, along `curve`. T is a float, or
// an array of them (a point, a colour)
template <typename T> struct Track {
  T from{}, to{};
  double start = 0;
  float seconds = 1;
  Curve curve = Curve::EaseInOut;

  // How far along it is at `now`, 0 before it starts, 1 once it's done
  float progress(double now) const {
    return seconds <= 0 ? 1.0f : along(curve, static_cast<float>((now - start) / seconds));
  }
  bool done(double now) const { return now >= start + seconds; }
  T at(double now) const { return mix(from, to, progress(now)); }

private:
  static float mix(float a, float b, float u) { return a + (b - a) * u; }
  template <std::size_t N>
  static std::array<float, N> mix(const std::array<float, N> &a, const std::array<float, N> &b,
                                  float u) {
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N; ++i)
      out[i] = mix(a[i], b[i], u);
    return out;
  }
};

// A short script of steps, each a span of time: one starts at once, after
// another ends, or with another (and later by `delay`). A moment such as a
// card flying up and then turning over is a few steps; the script says when
// each runs, and each step's progress (along a curve) drives whatever moves.
class Script {
public:
  using Step = std::size_t;
  Step add(float seconds, float delay = 0);                  // from the script's start
  Step after(Step previous, float seconds, float delay = 0); // when `previous` ends
  Step with(Step other, float seconds, float delay = 0);     // when `other` starts

  // When a step starts and ends, in seconds from the script's start
  float startOf(Step step) const { return steps_[step].start; }
  float endOf(Step step) const { return steps_[step].start + steps_[step].seconds; }
  // How far along a step is, `elapsed` seconds into the script
  float progress(Step step, float elapsed, Curve curve = Curve::Linear) const;
  // How long the whole script takes, and whether it's over
  float seconds() const;
  bool done(float elapsed) const { return elapsed >= seconds(); }

private:
  struct Span {
    float start, seconds;
  };
  Step push(float start, float seconds);
  std::vector<Span> steps_;
};

// Fixed time steps for a simulation driven by a varying frame rate
// (Gregory vol. 1, 8.5): how many steps of `step` seconds are due by `now`,
// at most `most` (a slow frame doesn't make the simulation fall further
// behind: the rest are dropped, so it slows rather than stalls)
class FixedSteps {
public:
  explicit FixedSteps(double step, double startAt = 0) : step_(step), next_(startAt) {}
  int due(double now, int most);
  double step() const { return step_; }

private:
  double step_, next_;
};

} // namespace ScratchPadd::Motion
