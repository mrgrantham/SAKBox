// The example's models from Blender on a DS: the hangar scene (a ship and
// two rocks, which the build checked fit a DS frame) drawn with ScratchPadd's
// scene layer, from the DS versions the build cooked: Ship_DS, made by hand
// in the .blend, and the rock, simplified by the build to 200 triangles.
//
// The ship is lit with its vertex colours; one rock is solid and one
// translucent; lines mark the floor; each model is labelled (overlay text
// over a translucent rectangle, placed with toScreen). The bottom screen
// gives what each frame drew against the DS's limits.
//
// A: the ship as a wireframe   START: quit
#include <nds.h>

#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include "rock_ds.h"
#include "ship_ds.h"

#include <array>
#include <cstdio>
#include <vector>

namespace Scene = ScratchPadd::Scene;

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
  Scene::Display display; // the top screen
  consoleDemoInit();      // the bottom screen's console, for printf
  Scene::Renderer renderer(display);

  const Scene::Mesh ship = renderer.load(Assets::ship), rock = renderer.load(Assets::rock);
  std::array<Shown, 3> shown{{
      {ship, {0, 0, 0}, {}, "Ship_DS"},
      {rock, {-2.4f, -0.4f, 0.5f}, {}, "Rock"},
      {rock, {2.4f, -0.4f, 0.5f}, {.alpha = 14}, "Alpha 14/31"},
  }};
  const std::vector<Scene::Vector3> lines = floorLines();

  Scene::Environment environment{.background = {28, 30, 38}, .ambient = {70, 70, 75}};
  environment.addLight({.direction = {-0.5f, -0.8f, -0.6f}, .colour = {220, 215, 205}});
  const Scene::Camera camera{.eye = {0, 1.6f, 6.4f}, .target = {0, -0.2f, 0}, .fieldOfView = 50};

  std::printf("\x1b[1;1HModels from Blender on a DS\n\n");
  std::printf("Ship_DS: %lu triangles\n", static_cast<unsigned long>(Assets::shipTriangles));
  std::printf("Rock:    %lu triangles (x2)\n", static_cast<unsigned long>(Assets::rockTriangles));
  std::printf("\nA: wireframe ship  START: quit\n");

  Scene::Scalar angle = 0;
  Scene::FrameCounts shownCounts{};
  while (pmMainLoop()) {
    scanKeys();
    const int down = keysDown();
    if (down & KEY_START)
      break;
    if (down & KEY_A)
      shown[0].material.wireframe = !shown[0].material.wireframe;

    renderer.beginFrame(camera, environment);
    renderer.drawLines(lines, {90, 95, 110});
    angle += Scene::Scalar(0.02f);
    const Scene::Transform turn = Scene::Transform::rotation({0, 1, 0}, angle);
    for (const Shown &model : shown) {
      renderer.draw(model.mesh, Scene::Transform::translation(model.where) * turn, model.material);
      float x = 0, y = 0;
      if (renderer.toScreen(model.where + Scene::Vector3{0, -1.1f, 0}, x, y)) {
        const float width = Scene::Overlay::textWidth(model.label);
        // On the text layer's 8-point cells, so the box fits the text
        const float left = static_cast<float>(static_cast<int>(x - width / 2) / 8 * 8);
        const float top = static_cast<float>(static_cast<int>(y) / 8 * 8);
        renderer.overlay().rectangle(left - 2, top - 2, width + 4, Scene::Overlay::lineHeight() + 4,
                                     {0, 0, 0, 150});
        renderer.overlay().text(left, top, model.label, {255, 255, 255});
      }
    }
    renderer.endFrame();

    // What the last frame drew, when it changes (printing is slow)
    const Scene::FrameCounts counts = renderer.lastFrame();
    if (counts.polygons != shownCounts.polygons || counts.vertices != shownCounts.vertices) {
      shownCounts = counts;
      std::printf("\x1b[8;1H3D a frame (the DS's limit):\x1b[K\n");
      std::printf("%4u polygons (2048)\x1b[K\n", counts.polygons);
      std::printf("%4u vertices (6144)\x1b[K\n", counts.vertices);
    }
  }
  return 0;
}
