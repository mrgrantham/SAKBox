// Shader playground: Shadertoy-style fragment shaders, each drawn into a
// texture of its own (the scene layer's Desktop::ShaderPanel) and shown in
// an ImGui window (debug_ui). The shaders get iTime, iResolution, iMouse and
// uniform_color; they're data files, read when it starts.
#include "backward/backward.hpp"

#include <DataDepRetriever/DataDependencies.hpp>
#include <ScratchPadd/DebugUi/DebugUi.hpp>
#include <ScratchPadd/Scene/GL/Desktop.hpp>
#include <ScratchPadd/Scene/GL/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include "imgui.h"
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <numbers>
#include <sstream>
#include <vector>

namespace Scene = ScratchPadd::Scene;

namespace {

// The shapes the shaders are drawn on, in -1 to 1: the whole square, a
// parallelogram, and a disc
struct Shape {
  std::vector<Scene::Vertex> vertices;
  std::vector<std::uint32_t> indices;
  Scene::MeshData data() const { return {vertices, indices}; }
};

Scene::Vertex at(float x, float y) { return {{x, y, 0}, {0, 0, 1}, {}}; }

Shape square() { return {{at(-1, -1), at(-1, 1), at(1, -1), at(1, 1)}, {0, 1, 2, 1, 2, 3}}; }

Shape parallelogram() {
  return {{at(-0.5f, -0.5f), at(-0.25f, 0.5f), at(0.25f, -0.5f), at(0.5f, 0.5f)},
          {0, 1, 2, 1, 2, 3}};
}

Shape disc(float radius, int sides) {
  Shape shape;
  shape.vertices.push_back(at(0, 0));
  for (int i = 0; i < sides; ++i) {
    const float angle = 2 * std::numbers::pi_v<float> * static_cast<float>(i) / sides;
    shape.vertices.push_back(at(radius * std::cos(angle), radius * std::sin(angle)));
  }
  for (std::uint32_t i = 1; i <= static_cast<std::uint32_t>(sides); ++i)
    shape.indices.insert(shape.indices.end(), {i, i % static_cast<std::uint32_t>(sides) + 1, 0});
  return shape;
}

// A shader file's text, from this example's data files
std::string readShader(const std::string &name) {
  // With Bzlmod the main repository's runfiles directory is always "_main"
  const std::string relative = "_main/projects/examples/shader_playground/shaders/" + name;
  const auto path = DataDepRetriever::GetFullDependencyPath(relative);
  if (!path) {
    spdlog::error("Shader file not found: {}", relative);
    std::exit(1);
  }
  std::ifstream file(*path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// A panel: its window's title and place, its texture's size, its shaders and
// the shape they're drawn on
struct PanelSetup {
  const char *name;
  float x, y;
  int width, height;
  const char *vertexShader, *fragmentShader;
  Shape shape;
};

} // namespace

class Playground : public ScratchPadd::Base {
public:
  Playground(ScratchPadd::System *system) : Base(system) {}

  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  std::string name() override { return __CLASS_NAME__; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    display_ = std::make_unique<Scene::Display>(1600, 1000, "Shader playground");
    if (!display_->isOpen()) {
      spdlog::error("No window: {}", display_->error());
      system_->stop();
      return;
    }
    renderer_ = std::make_unique<Scene::Renderer>(*display_);
    ui_ = std::make_unique<ScratchPadd::DebugUi::Ui>(*display_);
    const PanelSetup setups[] = {
        {"Blob", 20, 40, 800, 600, "play2.vert", "blob.frag", disc(0.8f, 100)},
        {"Play", 20, 350, 300, 300, "play.vert", "play.frag", parallelogram()},
        {"Play 3", 400, 40, 800, 600, "play3.vert", "play3.frag", square()},
        {"Gradient", 800, 40, 300, 300, "gradient.vert", "gradient.frag", square()},
        {"Disk", 1200, 40, 300, 300, "default.vert", "disk.frag", square()},
        {"Parallelogram Disk", 400, 350, 300, 300, "default.vert", "disk.frag", parallelogram()},
        {"Cool S", 800, 350, 300, 300, "default.vert", "cools.frag", square()},
        {"Cyber Fuji 2020", 1200, 350, 300, 300, "default.vert", "cyberfuji2020.frag", square()},
        {"Shading Art Code Intro", 20, 670, 300, 300, "default.vert", "shadingartcodingintro.frag",
         square()},
        {"Fractal Land", 400, 670, 300, 300, "default.vert", "fractalland.frag", square()},
        {"Mandala Flowers", 800, 670, 300, 300, "default.vert", "mandalaflowers.frag", square()},
        {"Flame", 1200, 670, 300, 300, "default.vert", "flame.frag", square()},
    };
    for (const PanelSetup &setup : setups) {
      panels_.push_back({setup.name, setup.x, setup.y,
                         std::make_unique<Scene::Desktop::ShaderPanel>(
                             setup.width, setup.height, readShader(setup.vertexShader),
                             readShader(setup.fragmentShader), setup.shape.data())});
    }
    push([this] { drawFrame(); }); // start drawing frames
  }

  void finishing() override {
    panels_.clear();
    ui_.reset();
    renderer_.reset();
    display_.reset();
  }

private:
  struct Panel {
    std::string name;
    float x, y;
    std::unique_ptr<Scene::Desktop::ShaderPanel> shader;
  };

  // Draws one frame, then queues the next (the renderer's endFrame waits
  // for the screen's refresh)
  void drawFrame() {
    if (!display_->isOpen()) {
      system_->stop();
      return;
    }
    display_->pollEvents();
    ui_->newFrame();
    renderer_->beginFrame({}, {.background = {30, 30, 30}});
    const float seconds = static_cast<float>(display_->seconds());
    for (Panel &panel : panels_) {
      ImGui::SetNextWindowPos(ImVec2(panel.x, panel.y), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      Theme::beginWindow(panel.name.c_str());
      const ImVec2 corner = ImGui::GetCursorScreenPos();
      const ImVec2 size = ImGui::GetContentRegionAvail();
      // The mouse in the texture's pixels, from its bottom-left
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const float mouseX = (mouse.x - corner.x) / size.x * panel.shader->width();
      const float mouseY = (1 - (mouse.y - corner.y) / size.y) * panel.shader->height();
      panel.shader->render(seconds, mouseX, mouseY, ImGui::IsMouseDown(ImGuiMouseButton_Left),
                           {13, 242, 77});
      // Textures are upside down to ImGui
      ImGui::Image(static_cast<ImTextureID>(panel.shader->texture()), size, ImVec2(0, 1),
                   ImVec2(1, 0));
      ImGui::End();
      ImGui::PopStyleVar();
    }
    renderer_->endFrame();
    push([this] { drawFrame(); });
  }

  std::unique_ptr<Scene::Display> display_;
  std::unique_ptr<Scene::Renderer> renderer_;
  std::unique_ptr<ScratchPadd::DebugUi::Ui> ui_;
  std::vector<Panel> panels_;
};

int main(int argc, char **argv) {
  // Makes the BUILD file's data dependencies (the shaders) findable
  DataDepRetriever::ConfigureDependencies(argv[0]);
  backward::SignalHandling signalHandling;
  ScratchPadd::System *system = ScratchPadd::SystemBuilder<Playground>();
  system->instantiate();
  system->start();
  return EXIT_SUCCESS;
}
