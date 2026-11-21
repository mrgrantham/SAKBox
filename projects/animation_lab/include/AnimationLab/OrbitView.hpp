// OrbitView.hpp - a 3D view moved as Blender's viewport is (ideas from
// Blender's view3d navigation, our own small code): a point it looks at,
// how it's turned (a turntable: turning about the world's up, tilting about
// the view's own sideways), and how far back it is.
//
//   OrbitView view;
//   view.orbit(dx, dy);                       // a drag: pixels across and down
//   view.pan(dx, dy, viewHeight);             // what's under the cursor stays under it
//   view.zoomAt(steps, x, y, w, h);           // toward the cursor; wheel steps
//   view.glideTo(view.looking(OrbitView::Side::Front)); // the numpad's views
//   view.advance(secondsSinceLastFrame);      // (each frame: moves a glide on)
//   Scene::Camera camera = view.camera();
//
// Jumps confuse: the ViewCube paper (Khan et al., Autodesk, 2008) moves the
// view over half a second so you see where you went. So the numpad's
// views, framing, Rewind and the corner widgets glide (glideTo), eased, the
// short way round; a drag, pan or zoom takes over from a glide at once.
//
// Drags can be undone (Esc, as Blender's): begin() keeps the view, cancel()
// puts it back.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cstdint>
#include <optional>

namespace AnimationLab {

// A point on the screen, in pixels (points), y down
struct ScreenPoint {
  float x = 0, y = 0;
};

class OrbitView {
public:
  // The views on the numpad: 1 front, 3 right, 7 top; with Ctrl, the
  // opposite (back, left, bottom)
  enum class Side : std::uint8_t { Front, Back, Right, Left, Top, Bottom };

  // Where the view is: everything a glide or Rewind moves
  struct Pose {
    ScratchPadd::Scene::Vector3 target{0, 0, 0};
    float yaw = 0.6f;    // (radians about the world's up: 0 looks along -z)
    float pitch = 0.35f; // (radians up: 0 level, up to just under a right angle)
    float distance = 10;
  };

  static constexpr float kRadiansAPixel = 0.007f; // (turning: about 0.4 degrees)
  static constexpr float kZoomStep = 1.2f;        // (a wheel step: 20% nearer or farther)
  static constexpr float kNearest = 0.05f, kFarthest = 1000;
  static constexpr float kGlideSeconds = 0.4f;
  static constexpr float kPi = 3.14159265f;
  static constexpr float kMostPitch = kPi / 2 - 0.001f; // (never quite straight down: up stays up)

  Pose pose;
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

  // Poses to go to (with glideTo, or set at once: view.pose = ...).
  // A numpad view: looking at a side, square on, from where it is now
  Pose looking(Side side) const;
  // Looking back along `towardEye` (from the eye's side; it needn't be
  // unit length), from where it is now. Straight down or up keeps the
  // turn it's nearest of the four square ones, so the scene doesn't spin
  Pose facing(const ScratchPadd::Scene::Vector3 &towardEye) const;
  // Everything from `low` to `high` in view, in a view of this aspect
  Pose framing(const ScratchPadd::Scene::Vector3 &low, const ScratchPadd::Scene::Vector3 &high,
               float aspect) const;

  // Moving to a pose over `seconds` (0: at once), eased, the short way
  // round; advance() moves it on each frame
  void glideTo(const Pose &to, float seconds = kGlideSeconds);
  void advance(float seconds);
  bool gliding() const { return glide_.has_value(); }
  // Where a glide ends (or the pose, when not gliding)
  const Pose &destination() const { return glide_ ? glide_->to : pose; }

  // Part of the way from `a` to `b` (0 to 1): the target straight, the
  // turn the short way round, the distance evenly in ratio
  static Pose between(const Pose &a, const Pose &b, float along);
  // Whether two poses look the same (to within a small turn and move)
  static bool same(const Pose &a, const Pose &b);

  // The view's camera, and its axes (right, up, and towards the eye)
  ScratchPadd::Scene::Camera camera() const;
  ScratchPadd::Scene::Vector3 right() const;
  ScratchPadd::Scene::Vector3 up() const;
  ScratchPadd::Scene::Vector3 towardEye() const;
  // A direction as the view sees it: x across (right), y up the screen,
  // z toward the eye (for drawing the corner widgets)
  ScratchPadd::Scene::Vector3 inView(const ScratchPadd::Scene::Vector3 &direction) const;

  // A drag's start, and putting the view back as it was then (Esc)
  void begin();
  void cancel();
  void end() { before_.reset(); }
  bool dragging() const { return before_.has_value(); }

private:
  struct Glide {
    Pose from, to;
    float seconds = 0, done = 0;
  };
  std::optional<Pose> before_;
  std::optional<Glide> glide_;
};

} // namespace AnimationLab
