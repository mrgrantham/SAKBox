// RagdollWindowPadd.hpp - the window: draws the ragdoll from the physics
// padd's snapshots, shows every padd's controls, and turns keys into
// RagdollCommands.
//
// It draws with ScratchPadd's scene layer, and its panels with debug_ui, at
// the screen's rate (each frame queues the next; the renderer's endFrame
// waits for the screen's refresh), whatever rate the physics steps at. Snapshots go into a
// SnapshotTimeline, which blends between them so motion is smooth when the
// rates don't line up; the window's own "Blend between snapshots" control
// turns that off, to compare.
#pragma once

#include "RagdollMessages.hpp"
#include "SnapshotTimeline.hpp"

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <chrono>
#include <memory>
#include <string>

namespace CyclonePadds {

class RagdollWindowPadd : public ScratchPadd::Base {
public:
  RagdollWindowPadd(ScratchPadd::System *system);
  ~RagdollWindowPadd() override;

  std::string name() override { return "RagdollWindow"; }
  ScratchPadd::RunOn runOn() override {
    return ScratchPadd::RunOn::MainThread;
  } // windowing must be on the main thread
  ScratchPadd::ControlValueMap generateControls() override {
    return {{"Blend between snapshots", ScratchPadd::ControlType::Boolean(true)}};
  }

  void starting() override;
  void finishing() override;

private:
  void drawFrame();
  void handleKeys();
  void drawStats();
  void showControls(const ScratchPadd::MessageType::ControlSnapshot &snapshot,
                    ScratchPadd::PaddAddress owner);
  // Seconds since the window opened, on the clock snapshots are timed by
  double now() const;

  // Draws the bones as boxes, and each joint as a line over them: green
  // within its slack, red beyond it
  void drawRagdoll(const std::vector<BoneSnapshot> &bones,
                   const std::vector<JointSnapshot> &joints);

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  ScratchPadd::DebugUi::ControlPanels panels_;
  ScratchPadd::Scene::Mesh cube_; // a unit cube, scaled to each bone
  SnapshotTimeline timeline_;
  std::chrono::steady_clock::time_point opened_ = std::chrono::steady_clock::now();

  // Snapshots and frames a second, counted over the last second
  int snapshotsCounted_ = 0, framesCounted_ = 0;
  double countingSince_ = 0, snapshotRate_ = 0, frameRate_ = 0;

  // Camera: degrees around the vertical and above the ground, turned by
  // dragging
  float cameraTurn_ = 60.0f, cameraRise_ = 20.0f;
  float dragX_ = 0, dragY_ = 0;
  bool dragging_ = false;
};

} // namespace CyclonePadds
