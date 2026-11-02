// Backend.hpp - the DS backend's state behind Scene::Renderer (private to
// the backend)
#pragma once

#include "DisplayList.hpp"

#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>

#include <string>
#include <utility>
#include <vector>

namespace ScratchPadd::Scene {

struct Renderer::Backend {
  explicit Backend(Display &display) : display(display) {}

  struct LoadedMesh {
    bool loaded = false;
    DisplayList::Source source;
    // Its display lists, packed the first time it's drawn each way: a few,
    // the oldest dropped first
    std::vector<std::pair<DisplayList::Variant, std::vector<std::uint32_t>>> lists;
    static constexpr std::size_t kMostLists = 4;
  };
  // A translucent draw, kept until the end of the frame
  struct Later {
    Mesh mesh;
    Transform where;
    Material material;
    Scalar distance; // from the eye, to draw the farthest first
  };

  const std::vector<std::uint32_t> &listFor(LoadedMesh &mesh, const DisplayList::Variant &variant);
  void drawNow(Mesh mesh, const Transform &where, const Material &material, unsigned polygonId);
  void drawOverlayShapes(const Overlay &overlay);
  void drawOverlayText(const Overlay &overlay);

  Display &display;
  std::vector<LoadedMesh> meshes; // by Mesh::id - 1
  std::vector<Later> later;

  // This frame's
  Camera camera;
  Colour ambient;
  std::uint32_t lightBits = 0;     // the polygon format's bits for the lights in use
  float viewProjection[4][4] = {}; // for toScreen, row-major, column vectors
  FrameCounts last;
  // What the text layer shows, rewritten only when the overlay's text changes
  std::vector<Overlay::Text> textShown;
};

} // namespace ScratchPadd::Scene
