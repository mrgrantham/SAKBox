// ViewCube.hpp - a cube in the view's corner that shows which way the view
// faces and turns it (Khan et al., "ViewCube", Autodesk, 2008). Its faces,
// edges and corners are 26 views to click; dragging it turns the view, and
// letting go within 10 degrees of one of them settles on it. A compass ring
// under it (N away from the front) turns the view about the world's up.
//
// This is the cube's shape on the screen and what's under the cursor; the
// lab's window draws it (with ImGui) and acts on it.
//
//   const auto cells = ViewCube::cells(view, {x, y}, 40);   // the cells facing you
//   if (auto way = ViewCube::pick(cells, mouse))
//     view.glideTo(view.facing(ViewCube::directionOf(*way)));
//
// Each face is split 3 x 3: its middle is the face's view, the strips along
// its sides the edges' (between two faces), its corner squares the
// corners' (between three). A view is its `Way`: from the cube's middle
// toward the eye, each of x, y and z -1, 0 or 1.
#pragma once

#include <AnimationLab/OrbitView.hpp>

#include <array>
#include <optional>
#include <vector>

namespace AnimationLab {

class ViewCube {
public:
  using Way = std::array<int, 3>;

  // How far across a face its middle goes (of the face's half-width)
  static constexpr float kMiddle = 0.6f;
  // Letting go of a drag this near a view (radians: 10 degrees) settles on it
  static constexpr float kSnapRadians = 10 * OrbitView::kPi / 180;
  // The compass: its height under the cube's middle and its radius (in the
  // cube's half-widths)
  static constexpr float kCompassBelow = 1.25f, kCompassRadius = 1.75f;

  // One cell of a face, on the screen
  struct Cell {
    Way way{};                            // (the view it turns to)
    int face = 0;                         // (its face: an index into kFaceNames)
    std::array<ScreenPoint, 4> corners{}; // (in order round it)
    float facing = 0;                     // (how squarely its face faces you: 0 to 1)
  };

  // Right (+x), Left, Top (+y), Bottom, Front (+z), Back: the faces' names,
  // as the numpad's views name them
  static constexpr std::array<const char *, 6> kFaceNames = {"RIGHT",  "LEFT",  "TOP",
                                                             "BOTTOM", "FRONT", "BACK"};

  // The cells of the faces turned toward you, for a cube centred at
  // `centre`, `half` pixels from its middle to a face
  static std::vector<Cell> cells(const OrbitView &view, ScreenPoint centre, float half);
  // Where a face's name goes (its middle), and whether it faces you
  static std::optional<ScreenPoint> faceMiddle(const OrbitView &view, ScreenPoint centre,
                                               float half, int face);
  // A face's own directions on the screen, for writing its name on it in
  // 3D: its middle, and how far across the screen one of the cube's units
  // goes along the face's right and its down (as you'd read it, upright:
  // the side faces' up is the world's; the top's down is toward the front,
  // the bottom's toward the back)
  struct FaceAxes {
    ScreenPoint middle, right, down;
  };
  static FaceAxes faceAxes(const OrbitView &view, ScreenPoint centre, float half, int face);
  // The cell under `at`, if any
  static std::optional<Way> pick(const std::vector<Cell> &cells, ScreenPoint at);

  // The compass ring as points round it (for drawing), where its letters
  // go (N, E, S, W), and whether `at` is on the ring (within `band` pixels)
  static std::vector<ScreenPoint> compass(const OrbitView &view, ScreenPoint centre, float half,
                                          int points = 48);
  static std::array<ScreenPoint, 4> compassLetters(const OrbitView &view, ScreenPoint centre,
                                                   float half);
  static bool onCompass(const OrbitView &view, ScreenPoint centre, float half, ScreenPoint at,
                        float band = 6);

  // A way as a direction (unit length), and the view nearest where the
  // view faces now, if it's within `radians`
  static ScratchPadd::Scene::Vector3 directionOf(const Way &way);
  static std::optional<Way> nearest(const OrbitView &view, float radians = kSnapRadians);
};

} // namespace AnimationLab
