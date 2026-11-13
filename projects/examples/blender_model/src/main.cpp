// Models from Blender, cooked by the build (tools/assets): each as desktops
// draw it, beside the DS's version of it. The ship's DS version was made by
// hand in Blender (Ship_DS, a Decimate modifier on a linked duplicate); the
// rock's was simplified by the build to its budget (SIMPLIFY_AGGRESSIVE).
#include <ScratchPadd/Scene/Desktop/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include "rock.h"
#include "rock_ds_preview.h"
#include "ship.h"
#include "ship_ds_preview.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <memory>
#include <string>

namespace Scene = ScratchPadd::Scene;

class ModelPadd : public ScratchPadd::Base {
public:
  ModelPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "ModelPadd"; }
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    display_ = std::make_unique<Scene::Display>(1280, 720, "Models from Blender");
    if (!display_->isOpen()) {
      spdlog::error("No window: {}", display_->error());
      system_->stop();
      return;
    }
    renderer_ = std::make_unique<Scene::Renderer>(*display_);
    models_[0] = {renderer_->load(Assets::kShip), Assets::kShipTriangles, "Ship, desktop"};
    models_[1] = {renderer_->load(Assets::Ds::kShip), Assets::Ds::kShipTriangles,
                  "Ship, DS (Ship_DS, made in Blender)"};
    models_[2] = {renderer_->load(Assets::kRock), Assets::kRockTriangles, "Rock, desktop"};
    models_[3] = {renderer_->load(Assets::Ds::kRock), Assets::Ds::kRockTriangles,
                  "Rock, DS (simplified by the build)"};
    push([this] { drawFrame(); });
  }

  void finishing() override {
    renderer_.reset();
    display_.reset();
  }

private:
  struct Shown {
    Scene::Mesh mesh;
    std::uint32_t triangles = 0;
    const char *label = "";
  };

  void drawFrame() {
    if (!display_->isOpen()) {
      system_->stop();
      return;
    }
    display_->pollEvents();
    if (display_->keyDown(Scene::Key::Escape))
      display_->close();
    const float seconds = static_cast<float>(display_->seconds());
    Scene::Environment environment{.background = {28, 30, 38}, .ambient = {70, 70, 75}};
    environment.addLight({.direction = {-0.5f, -0.8f, -0.6f}, .colour = {220, 215, 205}});
    renderer_->beginFrame({.eye = {0, 0.5f, 7.5f}, .target = {0, 0, 0}, .fieldOfView = 45},
                          environment);
    // The desktop's on the left, the DS's on the right; ships above, rocks below
    for (int i = 0; i < 4; ++i) {
      const Scene::Vector3 where{i % 2 ? 1.6f : -1.6f, i < 2 ? 1.1f : -1.2f, 0};
      renderer_->draw(models_[i].mesh,
                      Scene::Transform::translation(where) *
                          Scene::Transform::rotation({0, 1, 0}, seconds * 0.5f) *
                          Scene::Transform::scaling({0.9f, 0.9f, 0.9f}),
                      {});
      float x = 0, y = 0;
      if (renderer_->toScreen(where + Scene::Vector3{0, -1.0f, 0}, x, y)) {
        char text[96];
        std::snprintf(text, sizeof text, "%s: %u triangles", models_[i].label,
                      static_cast<unsigned>(models_[i].triangles));
        renderer_->overlay().text(x - Scene::Overlay::textWidth(text, 1.5f) / 2, y, text,
                                  {230, 225, 215}, 1.5f);
      }
    }
    renderer_->endFrame();
    push([this] { drawFrame(); });
  }

  std::unique_ptr<Scene::Display> display_;
  std::unique_ptr<Scene::Renderer> renderer_;
  Shown models_[4];
};

int main() {
  ScratchPadd::SystemImplementation<ModelPadd> system;
  system.instantiate();
  system.start(); // returns when the window closes
  return 0;
}
