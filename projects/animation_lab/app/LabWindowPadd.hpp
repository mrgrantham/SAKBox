// LabWindowPadd.hpp - the animation lab's window (desktop only):
//
//   History  every run (the settings, saved once they've been still a
//            moment): click one to go back to it, "vs" to play it beside
//            this one; named and deleted here. Kept between sessions
//   Making   the way (Throw, Guide, Search or Tumble, each with its
//            settings: drag a value and the motion is made again at once),
//            flat or 3D, the shape, the bake, and Save
//   3D       the shape flying the baked motion (as a game will play it),
//            its path as a line; the run compared in a view of its own
//            beside it, seen from the same camera (or on top, in one).
//            Moved as Blender's viewport is (OrbitView): middle-drag (or
//            left, or Alt-left) turns it, with Shift pans, with Ctrl zooms;
//            the wheel zooms toward the cursor; a trackpad's two-finger
//            scroll turns it (Shift pans, Ctrl or Cmd zooms); 1, 3, 7 (the
//            numpad or the number row) look from the front, right and top,
//            Ctrl for the opposite; Home frames it all; Esc undoes a drag.
//            View changes glide. In its top-right corner, the ViewCube or
//            Blender's axis gizmo (a setting); along its bottom, Rewind's
//            strip (Alt+Left and Alt+Right step back and forward)
//   Plots    from the side, from above, and the timing (how far along
//            against time): the physics as lines, the bake as dots
//
// Runs and saved motions are kept in the app's folder (Storage::Folders:
// ~/Library/Application Support/animation_lab/ on a Mac): history.txt, and
// motions/NAME.txt (writeText, Lab.hpp).
#pragma once

#include "LabShapes.hpp"

#include <AnimationLab/OrbitView.hpp>
#include <AnimationLab/Settings.hpp>
#include <AnimationLab/ViewHistory.hpp>

#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace AnimationLab {

class LabWindowPadd : public ScratchPadd::Base {
public:
  explicit LabWindowPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "AnimationLab"; }
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void starting() override;
  void finishing() override;

private:
  using Clock = std::chrono::steady_clock;

  // A run: settings tried, numbered, perhaps named
  struct Run {
    int number = 0;
    std::string name;
    Settings settings;
  };
  // A run made and ready to play
  struct Playing {
    Settings settings;
    Made made;
    const LabShape *shape = nullptr;
  };

  void drawFrame();
  void drawHistory();
  void drawMaking();
  void drawView(double t);
  // The mouse, trackpad and keys moving the view, over the 3D's part of the
  // window (`area`: x, y, width, height in points; `panes` views across it)
  // (`taken`: the mouse is on the corner widget or Rewind's strip, so a
  // click there doesn't start a drag of the view)
  void moveView(const float area[4], int panes, bool taken);
  // The corner widget (the ViewCube or Blender's axis gizmo) and Rewind's
  // strip, drawn over the 3D and acting on clicks and drags there; true
  // while the mouse is theirs (app/LabNavigation.cpp)
  bool navigate(const float area[4]);
  bool navigateCube(const float area[4]);
  bool navigateAxes(const float area[4]);
  bool navigateRewind(const float area[4]);
  void readViewSettings();
  void writeViewSettings() const;
  void drawPlots(double t);

  Playing play(const Settings &settings) const;
  // The current settings made again; noted for the history once they've
  // been still a moment
  void remake();
  void noteRun(); // (if they differ from the newest run)
  void readHistory();
  void writeHistory() const;
  void save();
  // Seconds into the loop both runs play on (a pause at the end)
  double playTime() const;

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  std::string folder_; // (the app's, for history.txt and motions/)

  std::vector<LabShape> shapes_;
  std::vector<ScratchPadd::Scene::Mesh> meshes_; // (shapes_'s, loaded)
  ScratchPadd::Scene::Texture cardPicture_;

  Settings settings_;
  Playing now_;
  std::optional<Playing> other_; // (the run compared)
  int otherNumber_ = 0;
  bool sideBySide_ = true, showPhysics_ = false;

  std::vector<Run> history_;
  int nextNumber_ = 1;
  int chosen_ = -1; // (the run whose settings these were, in history_)
  bool unnoted_ = false;
  Clock::time_point changedAt_ = Clock::now();

  OrbitView view_;
  bool frameNext_ = true;   // (frame what's shown: at the start, a run chosen, Home)
  bool framedOnce_ = false; // (the first framing is at once; the rest glide)
  Clock::time_point lastFrame_ = Clock::now();
  float frameSeconds_ = 0; // (since the frame before)

  // The view's corner and Rewind (app/LabNavigation.cpp)
  enum class Corner : std::uint8_t { ViewCube, Axes };
  Corner corner_ = Corner::ViewCube; // (kept in view.txt)
  ViewHistory viewHistory_;
  // A press on the corner widget: where, and whether it's become a drag
  // (and on the cube's compass: turning only about the world's up)
  struct CornerPress {
    ScreenPoint from;
    bool dragged = false, compass = false;
  };
  std::optional<CornerPress> cornerPress_;
  std::optional<float> scrubbing_; // (the place on Rewind's strip, while it's held)
  std::string name_ = "throw";
  std::string saved_;
  Clock::time_point playFrom_ = Clock::now();
};

} // namespace AnimationLab
