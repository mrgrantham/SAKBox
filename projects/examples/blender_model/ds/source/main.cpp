// The example's models from Blender on a DS: the hangar scene (a ship and
// two rocks, which the build checked fit a DS frame) drawn with ScratchPadd's
// scene layer, from the DS versions the build cooked: Ship_DS, made by hand
// in the .blend, and the rock, simplified by the build to 200 triangles.
//
// The ship is lit with its vertex colours; one rock is solid and one
// translucent; a pane of glass behind them is see-through by its vertex
// colours' alpha; lines mark the floor; fog fades what's far; each model is
// labelled (overlay text over a translucent rectangle, placed with
// toScreen), under a title at twice the size. The bottom screen gives what
// each frame drew against the DS's limits.
//
// A: the ship as a wireframe   B: fog off and on   X: toon shading or
// smooth   Y: outlines   R: toon's bands (2 to 5)   L: the toon table's
// Highlights or Shades (ScratchPadd/Scene/Types.hpp)   SELECT: a screenshot
// to the SD card (fat:/data/blender-model-ds/screenshots/)   START: quit
#include <nds.h>

#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/Input/Reader.hpp>
#include <ScratchPadd/Logger.hpp>
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/DS/Screenshot.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include "Look.hpp"
#include "rock_ds.h"
#include "ship_ds.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
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
    const Scene::Scalar at = i;
    ends.insert(ends.end(), {{at, -1, -4}, {at, -1, 4}, {-4, -1, at}, {4, -1, at}});
  }
  return ends;
}

} // namespace

int main() {
#if defined(SAKBOX_DEBUG)
  // The app's log (a debug build, bazel build -c dbg), fat:/data/blender-model-ds/logs/
  // (ScratchPadd/DebugLog/Session.hpp): static, so it outlives the exit
  // handlers, which log the last step too
  static ScratchPadd::DebugLog::Session log;
#endif
  // Quitting, step by step (to find where START hangs on a DS Lite: the
  // bottom screen goes dark too soon to read), each line written at once
  const std::shared_ptr<spdlog::logger> quitting = ScratchPadd::Logger::loggerNamed("Quitting");
  quitting->info("started");
  std::atexit([] {
    ScratchPadd::Logger::loggerNamed("Quitting")
        ->info("exit handlers: the last step before libnds powers off");
  });
  // The app in a scope of its own, so what it holds (the renderer, the
  // display) is closed before the ended screen
  {
    Scene::Display display; // the top screen
    consoleDemoInit();      // the bottom screen's console, for printf
    Scene::Renderer renderer(display);

    const Scene::Mesh ship = renderer.load(Assets::kShip), rock = renderer.load(Assets::kRock);
    // A pane of glass, see-through by its vertices' alpha (its material solid)
    Scene::MeshBuilder paneBuilder;
    paneBuilder.quad({-3, -1, -2.5f}, {3, -1, -2.5f}, {3, 1.6f, -2.5f}, {-3, 1.6f, -2.5f},
                     {120, 200, 255, 110});
    const Scene::Mesh pane = renderer.load(paneBuilder.data());
    std::array<Shown, 3> shown{{
        {ship, {0, 0, 0}, {}, "Ship_DS"},
        {rock, {-2.4f, -0.4f, 0.5f}, {}, "Rock"},
        {rock, {2.4f, -0.4f, 0.5f}, {.alpha = 14}, "Alpha 14/31"},
    }};
    const std::vector<Scene::Vector3> lines = floorLines();

    Scene::Environment environment{.background = {28, 30, 38}, .ambient = {70, 70, 75}};
    const Scene::Fog fog{.colour = {28, 30, 38}, .density = 0.12f}; // into the background
    environment.fog = fog;
    environment.addLight({.direction = {-0.5f, -0.8f, -0.6f}, .colour = {220, 215, 205}});
    const Scene::Camera camera{.eye = {0, 1.6f, 6.4f}, .target = {0, -0.2f, 0}, .fieldOfView = 50};

    std::printf("\x1b[0;0HModels from Blender on a DS\n\n");
    // (Polygons as the DS draws them: triangles joined into quads and strips)
    std::printf("Ship_DS: %lu triangles,\n  %lu polygons\n",
                static_cast<unsigned long>(Assets::kShipTriangles),
                static_cast<unsigned long>(Assets::kShipPolygons));
    std::printf("Rock:    %lu triangles,\n  %lu polygons (x2)\n",
                static_cast<unsigned long>(Assets::kRockTriangles),
                static_cast<unsigned long>(Assets::kRockPolygons));
    std::printf(
        "\nA: wireframe ship  B: fog\nX: toon  Y: outlines  R: bands\nL: highlights/shades\n"
        "SELECT: screenshot  START: quit\n");
    Look::Settings look;
    Look::Settings lookShown;
    lookShown.bands = 0; // (printed the first frame)

    Scene::Scalar angle = 0;
    Scene::FrameCounts shownCounts{};
    Input::Reader reader;
    Input::Gamepad before = reader.read();
    while (pmMainLoop()) {
      // The pad, once a frame, and what was pressed since the last
      const Input::Gamepad pad = reader.read();
      const Input::Edges pressed(before, pad);
      before = pad;
      if (pressed.pressed(Input::Button::Start))
        break;
      if (pressed.pressed(Input::Button::FaceRight)) // A
        shown[0].material.wireframe = !shown[0].material.wireframe;
      if (pressed.pressed(Input::Button::FaceBottom)) // B
        environment.fog = environment.fog.density > Scene::Scalar(0) ? Scene::Fog{} : fog;
      if (pressed.pressed(Input::Button::FaceTop)) // X
        look.toon = !look.toon;
      if (pressed.pressed(Input::Button::FaceLeft)) // Y
        look.outlines = !look.outlines;
      if (pressed.pressed(Input::Button::R))
        look.nextBands();
      if (pressed.pressed(Input::Button::L))
        look.switchDsTable();
      look.apply(environment);
      const bool screenshot = pressed.pressed(Input::Button::Select);

      renderer.beginFrame(camera, environment);
      renderer.drawLines(lines, {90, 95, 110});
      renderer.draw(pane, Scene::Transform::identity(),
                    {.shading = Scene::Material::Shading::Unlit});
      renderer.overlay().text(8, 6, "Hangar", {255, 220, 140}, 2);
      angle += Scene::Scalar(0.02f);
      const Scene::Transform turn = Scene::Transform::rotation({0, 1, 0}, angle);
      for (const Shown &model : shown) {
        renderer.draw(model.mesh, Scene::Transform::translation(model.where) * turn,
                      look.material(model.material));
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
      if (screenshot) {
        std::string error;
        const std::string path =
            Scene::saveScreenshot("fat:/data/blender-model-ds/screenshots/", error);
        std::printf("\x1b[19;0H%s\x1b[K\n", path.empty() ? error.c_str() : path.c_str());
      }

      // What the last frame drew, when it changes (printing is slow). (libnds'
      // console counts rows and columns from 0)
      const Scene::FrameCounts counts = renderer.lastFrame();
      if (counts.polygons != shownCounts.polygons || counts.vertices != shownCounts.vertices) {
        shownCounts = counts;
        std::printf("\x1b[11;0H3D a frame (the DS's limit):\x1b[K\n");
        std::printf("%4u polygons (2048)\x1b[K\n", counts.polygons);
        std::printf("%4u vertices (6144)\x1b[K\n", counts.vertices);
      }
      if (look.toon != lookShown.toon || look.outlines != lookShown.outlines ||
          look.bands != lookShown.bands || look.dsTable != lookShown.dsTable) {
        lookShown = look;
        char looks[64];
        look.describe(looks, sizeof looks);
        std::printf("\x1b[16;0H%s\x1b[K\n%s\x1b[K\n", looks,
                    look.dsTable == Scene::DsToonTable::Shades ? "Table: shades"
                                                               : "Table: highlights");
      }
    }
    quitting->info("left the main loop");
  }
  quitting->info("closed the renderer and display");
  // "Models from Blender has ended.", until a button: then main returns and
  // the DS switches off (a DS app that just returns leaves its last frame
  // on the screens: ScratchPadd/AppEnded/AppEnded.hpp)
  ScratchPadd::AppEnded::show();
  quitting->info("returning from main");
  return 0;
}
