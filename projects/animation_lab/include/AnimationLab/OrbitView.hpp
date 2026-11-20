// OrbitView.hpp - a 3D view moved as Blender's viewport is (ideas from
// Blender's view3d navigation, our own small code): a point it looks at,
// how it's turned (a turntable: turning about the world's up, tilting about
// the view's own sideways), and how far back it is.
//
//   OrbitView view;                       // (or view.frame(low, high))
//   view.orbit(dx, dy);                   // a drag: pixels across and down
//   view.pan(dx, dy, viewHeight);         // what's under the cursor stays under it
//   view.zoomAt(steps, x, y, w, h);       // toward the cursor; wheel steps
//   view.look(OrbitView::Side::Front);    // the numpad's views
//   Scene::Camera camera = view.camera();
//
// Drags can be undone (Esc, as Blender's): begin() keeps the view, cancel()
// puts it back.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <optional>

namespace AnimationLab {

class OrbitView {
public:
  // The views on the numpad: 1 front, 3 right, 7 top; with Ctrl, the
  // opposite (back, left, bottom)
  enum class Side : std::uint8_t { Front, Back, Right, Left, Top, Bottom };

  static constexpr float kRadiansAPixel = 0.007f; // (turning: about 0.4 degrees)
  static constexpr float kZoomStep = 1.2f;        // (a wheel step: 20% nearer or farther)
  static constexpr float kNearest = 0.05f, kFarthest = 1000;

  ScratchPadd::Scene::Vector3 target{0, 0, 0};
  float yaw = 0.6f;    // (radians about the world's up: 0 looks along -z)
  float pitch = 0.35f; // (radians up: 0 level, up to just under a right angle)
  float distance = 10;
  float fieldOfView = 40; // (degrees, vertical)

  // A drag of (dx, dy) pixels: across turns about the world's up, down tilts
  // the view (the horizon kept level)
  void orbit(float dx, float dy);
  // A drag of (dx, dy) pixels in a view `height` pixels tall: the point at
  // the target's depth under the cursor stays under it
  void pan(float dx, float dy, float height);
  // Wheel steps (up, positive: nearer), toward the point under the cursor
  // at (x, y) in a view w x h
  void zoomAt(float steps, float x, float y, float w, float h);
  // Looking at a side, square on
  void look(Side side);
  // Everything from `low` to `high` in view, in a view of this aspect
  void frame(const ScratchPadd::Scene::Vector3 &low, const ScratchPadd::Scene::Vector3 &high,
             float aspect);

  // The view's camera, and its axes (right, up, and towards the eye)
  ScratchPadd::Scene::Camera camera() const;
  ScratchPadd::Scene::Vector3 right() const;
  ScratchPadd::Scene::Vector3 up() const;
  ScratchPadd::Scene::Vector3 towardEye() const;

  // A drag's start, and putting the view back as it was then (Esc)
  void begin() { before_ = Kept{target, yaw, pitch, distance}; }
  void cancel();
  void end() { before_.reset(); }
  bool dragging() const { return before_.has_value(); }

private:
  struct Kept {
    ScratchPadd::Scene::Vector3 target;
    float yaw, pitch, distance;
  };
  std::optional<Kept> before_;
};

} // namespace AnimationLab
