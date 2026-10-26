// RagdollWindowPadd.hpp - the window: draws the ragdoll from the physics
// padd's snapshots, shows every padd's controls, and turns keys into
// RagdollCommands.
//
// It draws at the screen's rate (each frame queues the next; the buffer swap
// waits for vsync), whatever rate the physics steps at. Snapshots go into a
// SnapshotTimeline, which blends between them so motion is smooth when the
// rates don't line up; the window's own "Blend between snapshots" control
// turns that off, to compare.
#pragma once

#include "RagdollMessages.hpp"
#include "SnapshotTimeline.hpp"

#include <ScratchPadd/Base.hpp>

#include <chrono>
#include <memory>
#include <string>

struct GLFWwindow;

namespace CyclonePadds {

struct RagdollRenderer;

class RagdollWindowPadd : public ScratchPadd::Base {
public:
  RagdollWindowPadd(ScratchPadd::System *system);
  ~RagdollWindowPadd() override;

  std::string name() override { return "RagdollWindow"; }
  bool shouldUseMainThread() override { return true; } // windowing must be on the main thread
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

  GLFWwindow *window_ = nullptr;
  std::unique_ptr<RagdollRenderer> renderer_;
  SnapshotTimeline timeline_;
  std::chrono::steady_clock::time_point opened_ = std::chrono::steady_clock::now();

  // Snapshots and frames a second, counted over the last second
  int snapshotsCounted_ = 0, framesCounted_ = 0;
  double countingSince_ = 0, snapshotRate_ = 0, frameRate_ = 0;

  // Camera: degrees around the vertical and above the ground, turned by
  // dragging
  float cameraTurn_ = 60.0f, cameraRise_ = 20.0f;
  double dragX_ = 0, dragY_ = 0;
  bool dragging_ = false;

  bool resetHeld_ = false, pauseHeld_ = false, nudged_ = false;

  struct ControlPanels;
  std::unique_ptr<ControlPanels> controlPanels_;
};

} // namespace CyclonePadds
