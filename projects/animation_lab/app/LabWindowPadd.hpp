// LabWindowPadd.hpp - the animation lab's window (desktop only), in two
// columns:
//
//   making   the way to make the motion (Throw, Guide or Search, each with
//            its settings: drag a value and the motion is made again at
//            once), flat or 3D, then the bake (how many path and timing
//            points, and how far the baked motion strays), and Save
//   seeing   the motion from the side (x up y) and from above (x along z),
//            the recording as a line and the baked motion's path as dots,
//            a marker on each playing it back; and the timing: how far along
//            against time, the recording's and the baked
//
// Saved motions go to animation_lab_motions/NAME.txt (writeText, Lab.hpp).
#pragma once

#include <AnimationLab/Lab.hpp>

#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <chrono>
#include <memory>
#include <string>

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
  enum class Way { Throw, Guide, Search };

  void drawFrame();
  void drawMaking();
  void drawSeeing();
  // The motion made again from the settings, and baked
  void remake();
  void save();

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;

  Way way_ = Way::Throw;
  bool flat_ = false;
  Throw throw_{.from = {0, 0, 0}, .to = {4, 0, 1}, .seconds = 1.2};
  Guide guide_{.keys = {{0, {0, 0, 0}}, {0.35, {1.5, 1.2f, 0}}, {0.7, {3, 0, 0}}}};
  Search search_{.goal = {.from = {0, 1, 0}, .to = {5, 0.3f, 0}, .seconds = 1.5},
                 .drag = 0.8,
                 .floor = 0,
                 .bounce = 0.6};
  int pathPoints_ = 24, timingPoints_ = 24;

  Recording made_;
  ScratchPadd::Motion::BakedMotion baked_;
  double miss_ = 0, error_ = 0;
  std::string name_ = "throw";
  std::string saved_;
  std::chrono::steady_clock::time_point playFrom_ = std::chrono::steady_clock::now();
};

} // namespace AnimationLab
