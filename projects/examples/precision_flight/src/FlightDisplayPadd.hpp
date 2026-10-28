// FlightDisplayPadd.hpp - the window: input, rendering (ScratchPadd's scene
// layer), the HUD and the control panels (debug_ui).
//
// Frames are a chain with the simulation padd: the display sends a
// FrameInput, the simulation answers with that frame's events and a
// WorldSnapshot, and drawing the snapshot (paced by vsync) sends the next
// FrameInput. Purely visual state (the eased ship position, hit flashes,
// fading messages) lives here; the game itself lives in FlightSimPadd.
#pragma once

#include "FlightMessages.hpp"

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/DebugUi/ControlPanel.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrecisionFlight {

class FlightDisplayPadd : public ScratchPadd::Base {
public:
  FlightDisplayPadd(ScratchPadd::System *system);
  ~FlightDisplayPadd() override;

  std::string name() override { return "FlightDisplay"; }
  ScratchPadd::RunOn runOn() override {
    return ScratchPadd::RunOn::MainThread;
  } // windowing must be on the main thread
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override;
  void finishing() override;

private:
  struct Flash {
    float seconds = 0.0f;
    glm::vec3 color{1.0f};
  };
  struct OnScreenMessage {
    std::string text;
    unsigned color; // ImU32
    float age = 0.0f;
  };

  // Draws `world`, then sends the next FrameInput.
  void drawFrame(const WorldSnapshot &world);
  void showEvent(const FlightEvent &event);
  void drawWorld(float time);
  void drawHud();
  // `base`, blended toward the obstacle's flash colour while it flashes.
  glm::vec3 flashed(const Obstacle &o, glm::vec3 base) const;

  // The meshes it draws: a unit cube (scaled to each box), the ship and the
  // pickups' octahedron
  struct Meshes {
    ScratchPadd::Scene::Mesh cube, ship, octahedron;
  };

  std::unique_ptr<ScratchPadd::Scene::Display> display_;
  std::unique_ptr<ScratchPadd::Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  ScratchPadd::DebugUi::ControlPanels panels_;
  Meshes meshes_;
  WorldSnapshot world_;

  // Visual state
  glm::vec2 smoothed_{0.0f};               // drawn ship position, eased toward the observed one
  std::unordered_map<int, Flash> flashes_; // by obstacle id
  std::deque<OnScreenMessage> messages_;
  bool showHitbox_ = true;

  // Input
  double previousTime_ = 0.0;
};

} // namespace PrecisionFlight
