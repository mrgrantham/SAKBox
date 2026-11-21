// AxisGizmo.hpp - Blender's navigation gizmo: the world's three axes from
// the view's corner, a ball at each end (X red, Y green, Z blue; the far
// ends hollow). Click a ball to look from that end (click it again, once
// there, for the opposite: Blender's way); drag the gizmo to turn round.
//
// The lab's world has y up (Blender's has z up), so here Y is the one
// pointing up the screen when level.
//
// This is where the balls are on the screen and which is under the
// cursor; the lab's window draws it (with ImGui) and acts on it.
//
//   const auto balls = AxisGizmo::balls(view, {x, y}, 40);
//   if (auto ball = AxisGizmo::pick(balls, mouse, 9)) ...
#pragma once

#include <AnimationLab/OrbitView.hpp>

#include <array>
#include <optional>

namespace AnimationLab {

class AxisGizmo {
public:
  // One end of an axis on the screen
  struct Ball {
    int axis = 0; // (0 x, 1 y, 2 z)
    int sign = 1; // (1: the axis's own end, drawn filled and named; -1 the far end)
    ScreenPoint at{};
    float depth = 0; // (toward you: higher is nearer)
  };

  // The six ends, for a gizmo centred at `centre` with arms `radius`
  // pixels long, farthest first (the order to draw them in)
  static std::array<Ball, 6> balls(const OrbitView &view, ScreenPoint centre, float radius);
  // The nearest ball under `at` (within `ballRadius` of its middle)
  static std::optional<Ball> pick(const std::array<Ball, 6> &balls, ScreenPoint at,
                                  float ballRadius);
  // The way to look from, clicking `ball`: its end, or (when the view
  // already looks from there) the opposite end
  static ScratchPadd::Scene::Vector3 lookFrom(const OrbitView &view, const Ball &ball);
};

} // namespace AnimationLab
