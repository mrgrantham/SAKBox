// The example's models from Blender on a 3DS: the hangar scene (a ship and
// two rocks, which the build checked fit a 3DS frame) drawn with
// ScratchPadd's scene layer, from the 3DS versions the build cooked: the
// full ship (the 3DS has room for it) and the rock. In stereo while the 3D
// slider is up.
//
// The ship is lit with its vertex colours; one rock is solid and one
// translucent; a pane of glass behind them is see-through by its vertex
// colours' alpha; lines mark the floor; fog fades what's far; each model is
// labelled (overlay text over a translucent rectangle, placed with
// toScreen), under a title at twice the size. The bottom screen gives what
// each frame drew and how long it took.
//
// A: the ship as a wireframe   B: fog off and on   SELECT: the scene layer's
// debug panel (ScratchPadd/Scene/3DS/Debug.hpp)   START: quit
//
// A watchdog runs all along: if the GPU hangs, it says which draws it was
// sent last, on the bottom screen and in sdmc:/scene-3ds-hang.txt.
#include <3ds.h>

#include <ScratchPadd/Input/Reader.hpp>
#include <ScratchPadd/Scene/3DS/Debug.hpp>
#include <ScratchPadd/Scene/3DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include "rock_3ds.h"
#include "ship_3ds.h"

#include <array>
#include <cstdio>
#include <vector>

namespace Scene = ScratchPadd::Scene;
namespace Input = ScratchPadd::Input;

namespace {

struct Shown {
  Scene::Mesh mesh;
  Scene::Vector3 where;
  Scene::Material material;
  const char *label;
};

// Lines across the floor, a metre apart
std::vector<Scene::Vector3> floorLines() {
  std::vector<Scene::Vector3> ends;
  for (int i = -4; i <= 4; ++i) {
    const Scene::Scalar at = static_cast<Scene::Scalar>(i);
    ends.insert(ends.end(), {{at, -1, -4}, {at, -1, 4}, {-4, -1, at}, {4, -1, at}});
  }
  return ends;
}

} // namespace

int main() {
  gfxInitDefault();
  {
    Scene::Display display; // the top screen
    consoleInit(GFX_BOTTOM, nullptr);
    Scene::Renderer renderer(display);

    const Scene::Mesh ship = renderer.load(Assets::ship), rock = renderer.load(Assets::rock);
    // A pane of glass, see-through by its vertices' alpha (its material solid)
    Scene::MeshBuilder paneBuilder;
    paneBuilder.quad({-3, -1, -2.5f}, {3, -1, -2.5f}, {3, 1.6f, -2.5f}, {-3, 1.6f, -2.5f},
                     {120, 200, 255, 110});
    const Scene::Mesh pane = renderer.load(paneBuilder.data());
    std::array<Shown, 3> shown{{
        {ship, {0, 0, 0}, {}, "Ship"},
        {rock, {-2.4f, -0.4f, 0.5f}, {}, "Rock"},
        {rock, {2.4f, -0.4f, 0.5f}, {.alpha = 14}, "Alpha 14/31"},
    }};
    const std::vector<Scene::Vector3> lines = floorLines();

    Scene::Environment environment{.background = {28, 30, 38}, .ambient = {70, 70, 75}};
    const Scene::Fog fog{.colour = {28, 30, 38}, .density = 0.12f}; // into the background
    environment.fog = fog;
    environment.addLight({.direction = {-0.5f, -0.8f, -0.6f}, .colour = {220, 215, 205}});
    const Scene::Camera camera{.eye = {0, 1.6f, 6.4f}, .target = {0, -0.2f, 0}, .fieldOfView = 40};

    Scene::Debug::Watchdog watchdog(renderer);
    Scene::Debug::Panel debugPanel(renderer);
    const auto printIntro = [] {
      std::printf("\x1b[1;1HModels from Blender on a 3DS\n\n");
      std::printf("Ship: %lu triangles\n", static_cast<unsigned long>(Assets::shipTriangles));
      std::printf("Rock: %lu triangles (x2)\n\n",
                  static_cast<unsigned long>(Assets::rockTriangles));
      std::printf("A: wireframe ship  B: fog\n3D slider: stereo  SELECT: debug\nSTART: quit\n");
    };
    printIntro();

    float angle = 0;
    int frame = 0;
    Input::Reader reader;
    Input::Gamepad before = reader.read();
    while (aptMainLoop()) {
      // The pad, once a frame, and what was pressed since the last
      const Input::Gamepad pad = reader.read();
      const Input::Edges pressed(before, pad);
      if (pressed.pressed(Input::Button::Start))
        break;
      if (pressed.pressed(Input::Button::Select)) {
        debugPanel.setVisible(!debugPanel.visible());
        if (!debugPanel.visible())
          printIntro();
      } else if (!debugPanel.handle(before, pad)) {
        if (pressed.pressed(Input::Button::FaceRight)) // A
          shown[0].material.wireframe = !shown[0].material.wireframe;
        if (pressed.pressed(Input::Button::FaceBottom)) // B
          environment.fog = environment.fog.density > 0 ? Scene::Fog{} : fog;
      }
      before = pad;

      renderer.beginFrame(camera, environment);
      renderer.drawLines(lines, {90, 95, 110});
      renderer.draw(pane, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::unlit});
      renderer.overlay().text(8, 6, "Hangar", {255, 220, 140}, 2);
      angle += 0.02f;
      const Scene::Transform turn = Scene::Transform::rotation({0, 1, 0}, angle);
      for (const Shown &model : shown) {
        renderer.draw(model.mesh, Scene::Transform::translation(model.where) * turn,
                      model.material);
        float x = 0, y = 0;
        if (renderer.toScreen(model.where + Scene::Vector3{0, -1.1f, 0}, x, y)) {
          const float width = Scene::Overlay::textWidth(model.label);
          const float left = x - width / 2, top = y;
          renderer.overlay().rectangle(left - 2, top - 2, width + 4,
                                       Scene::Overlay::lineHeight() + 4, {0, 0, 0, 150});
          renderer.overlay().text(left, top, model.label, {255, 255, 255});
        }
      }
      renderer.endFrame();

      debugPanel.draw();
      // What the last frame drew and how long it took, twice a second
      if (++frame % 30 == 0 && !debugPanel.visible()) {
        const Scene::FrameCounts counts = renderer.lastFrame();
        const Scene::FrameTimes times = renderer.lastFrameTimes();
        std::printf("\x1b[11;1H3D a frame:\x1b[K\n");
        std::printf("%6u triangles (build limit 45000)\x1b[K\n", counts.polygons);
        std::printf("%6u draws\x1b[K\n", counts.draws);
        std::printf("GPU %5.2f ms, CPU %5.2f ms\x1b[K\n", times.gpu, times.cpu);
        std::printf("Stereo: %s\x1b[K\n", osGet3DSliderState() > 0 ? "on" : "off");
      }
    }
  }
  gfxExit();
  return 0;
}
