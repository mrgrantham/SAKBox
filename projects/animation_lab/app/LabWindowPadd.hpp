// LabWindowPadd.hpp - the animation lab's window (desktop only):
//
//   History  every run (the settings, saved once they've been still a
//            moment): click one to go back to it, "vs" to play it beside
//            this one; named and deleted here. Kept between sessions
//   Making   the way (Throw, Guide, Search or Tumble, each with its
//            settings: drag a value and the motion is made again at once),
//            flat or 3D, the shape, the bake, and Save
//   3D       the shape flying the baked motion (as a game will play it),
//            its path as a line; and the run compared beside it. Drag to
//            turn the view, scroll to zoom
//   Plots    from the side, from above, and the timing (how far along
//            against time): the physics as lines, the bake as dots
//
// Runs and saved motions are kept in the app's folder (Storage::Folders:
// ~/Library/Application Support/animation_lab/ on a Mac): history.txt, and
// motions/NAME.txt (writeText, Lab.hpp).
#pragma once

#include "LabShapes.hpp"

#include <AnimationLab/Settings.hpp>

#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <chrono>
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

  float yaw_ = -0.6f, pitch_ = 0.35f, zoom_ = 1;
  std::string name_ = "throw";
  std::string saved_;
  Clock::time_point playFrom_ = Clock::now();
};

} // namespace AnimationLab
