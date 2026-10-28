// DisplayPadd.hpp - a window for any ScratchPadd system: a small lit scene
// drawn with the scene layer (ScratchPadd/Scene), each padd's controls as
// panels (it asks for them with a ControlRequest), and ImGui's demo window.
// It runs on the main thread, as windows must on macOS, and draws a frame
// at a time: each frame queues the next, and the renderer's endFrame waits
// for the screen's refresh, so it draws at the screen's rate with no timer
// and no spinning.
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/Timer.hpp>

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>

class DisplayPadd : public ScratchPadd::Base {
public:
  DisplayPadd(ScratchPadd::System *system)
      : Base(system), panels_([this](ScratchPadd::PaddAddress owner,
                                     const ScratchPadd::MessageType::ControlChange &change) {
          sendTo(owner, change);
        }) {
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      std::cout << paddName() << " Text: " << text << "\n";
    });
    // Snapshots are handled on this padd's thread, which also draws, so a
    // panel never changes mid-frame
    receive<ScratchPadd::MessageType::ControlSnapshot>(
        [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
               ScratchPadd::PaddAddress owner) { panels_.show(snapshot, owner); });
  }

  std::string name() override { return __CLASS_NAME__; }
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    namespace Scene = ScratchPadd::Scene;
    display_ = std::make_unique<Scene::Display>(1366, 768, "ScratchPadd");
    if (!display_->isOpen()) {
      spdlog::error("[{}] No window: {}", paddName(), display_->error());
      system_->stop();
      return;
    }
    renderer_ = std::make_unique<Scene::Renderer>(*display_);
    ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
    // A walnut table with three blocks on it, in the theme's colours
    Scene::MeshBuilder blocks;
    blocks.box({0, -0.6f, 0}, {2.4f, 0.1f, 1.6f}, {120, 80, 50})
        .box({-1.2f, 0, 0}, {0.5f, 0.5f, 0.5f}, {40, 150, 140})
        .box({0, 0.1f, 0.3f}, {0.4f, 0.6f, 0.4f}, {230, 130, 50})
        .box({1.2f, -0.2f, -0.2f}, {0.6f, 0.3f, 0.6f}, {220, 180, 60});
    blocks_ = renderer_->load(blocks.data());
    timer_.setTimerName(paddName());
    timer_.start();

    // Ask every padd with controls for them; each reply becomes a panel
    send(ScratchPadd::MessageType::ControlRequest{});
    push([this] { drawFrame(); }); // start drawing frames
  }

  void finishing() override {
    if (display_ && display_->isOpen()) {
      spdlog::info("[{}] Average frame: {}", paddName(), timer_.getAverageIntervalString());
    }
    panels_.clear();
    ui_.reset();
    renderer_.reset();
    display_.reset();
  }

private:
  // Draws one frame, then queues the next
  void drawFrame() {
    namespace Scene = ScratchPadd::Scene;
    if (!display_->isOpen()) {
      system_->stop();
      return;
    }
    timer_.markInterval();
    display_->pollEvents();
    ui_->newFrame();

    const float seconds = static_cast<float>(display_->seconds());
    Scene::Camera camera;
    camera.eye = {6 * std::sin(seconds * 0.3f), 2.5f, 6 * std::cos(seconds * 0.3f)};
    camera.fieldOfView = 45;
    Scene::Environment environment;
    environment.background = {40, 32, 28};
    environment.ambient = {70, 70, 70};
    environment.addLight({.direction = {-0.4f, -1, -0.3f}, .colour = {200, 195, 185}});
    renderer_->beginFrame(camera, environment);
    renderer_->draw(blocks_, Scene::Transform::identity(), {});
    char fps[48];
    std::snprintf(fps, sizeof fps, "%.0f frames a second",
                  1.0 / std::max(timer_.getAverageIntervalInSeconds(5), 1e-6));
    renderer_->overlay().text(12, 12, fps, {240, 230, 210}, 1.5f);

    panels_.draw();
    if (showDemo_) {
      ImGui::ShowDemoWindow(&showDemo_);
    }
    renderer_->endFrame();
    push([this] { drawFrame(); });
  }

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  ScratchPadd::DebugUi::ControlPanels panels_;
  ScratchPadd::Scene::Mesh blocks_;
  ScratchPadd::Timer timer_;
  bool showDemo_ = true;
};
