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
#include <optional>
#include <vector>

namespace ScratchPadd::Motion {

// How far along (0 to 1) a move is when its time is `t` of the way through
// (0 to 1), clamped to 0 and 1:
//   Linear     straight, at an even speed
//   EaseIn     starting slowly, speeding up (t squared)
//   EaseOut    starting fast, slowing to stop
//   EaseInOut  slow, fast, slow (smoothstep: 3t^2 - 2t^3)
//   Gentle     the same, softer at both ends (smootherstep: 6t^5 - 15t^4 +
//              10t^3, no jolt starting or stopping; Cosmic Hex's tiles
//              lifting off the board and back)
//   Pull       speeding up evenly to the middle, then slowing the same way:
//              two parabolas, the curve of something pulled at a constant
//              acceleration then braked (Cosmic Hex's dice glass)
//   Settle     fast, past the end by about a tenth, and back to settle: a
//              piece dropped into place ("ease out back", the animators'
//              follow-through; Cosmic Hex's number chips). Goes past 1
//   Anticipate back a little first (about a tenth), then away, slowly
//              starting: the wind-up before a move ("ease in back", the
//              animators' anticipation; a tile dipping before it lifts
//              off the board). Goes below 0
// (docs/research/animation-whimsy.md: the animation principles behind
// Settle, Anticipate and arc)
enum class Curve { Linear, EaseIn, EaseOut, EaseInOut, Gentle, Pull, Settle, Anticipate };
float along(Curve curve, float t);

// A point on the arc from `from` to `to`, `t` of the way (0 to 1), its
// middle pushed out by `bulge`: a curve rather than a straight line, as
// things thrown or lifted move (a quadratic Bezier through the middle
// lifted by `bulge`). T needs +, - and * by a float: a float, a 3D vector
template <typename T> T arc(const T &from, const T &to, const T &bulge, float t) {
  // (the control point that puts the curve's middle at the straight
  // line's middle plus `bulge`)
  const T middle = from + (to - from) * 0.5f;
  const T control = middle + bulge * 2.0f;
  const float u = 1 - t;
  return from * (u * u) + control * (2 * u * t) + to * (t * t);
}

// Something shown since a moment, on the caller's clock (seconds): a
// view's animation started, how long it's been, and stopped again.
//
//   Showing dealt;           // not showing
//   dealt.start(now);
//   dealt.seconds(now)       // since it started
//   dealt.stop();
struct Showing {
  std::optional<double> since;
  void start(double now) { since = now; }
  void stop() { since.reset(); }
  bool on() const { return since.has_value(); }
  double seconds(double now) const { return since ? now - *since : 0; }
};

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
