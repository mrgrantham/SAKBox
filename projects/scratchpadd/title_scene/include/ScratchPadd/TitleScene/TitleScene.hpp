// TitleScene.hpp - a game's title screen in 3D (Cosmic Hex's round 87, for
// any game): its title in blocky letters in front (stb_easy_font's strokes
// made into boxes, outlined in a bright line: the Tron look), a model turning
// slowly behind it (a game piece on its pedestal), and a starfield all round
// turning very slowly, as the night sky does.
//
//   TitleScene title(renderer, Assets::kRocket, {.title = "COSMIC HEX"});
//   renderer.beginFrame(title.camera(), title.environment());
//   title.draw(renderer, secondsSinceStart);
//   renderer.endFrame();
//
// The scene draws within a frame the caller begins, so a game can show it
// at its start, and again when it likes (paused, or a game won), with its
// own words over it.
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <string>

namespace ScratchPadd::TitleScene {

struct Settings {
  std::string title;
  Scene::Colour letters{232, 238, 250}; // the letters' faces
  Scene::Colour edges{90, 220, 255};    // their outlines
  Scene::Colour background{14, 20, 36};
  float modelSeconds = 14;     // a whole turn of the model
  float skySeconds = 300;      // a whole turn of the sky
  float modelHeight = 3.0f;    // the model, scaled to this tall
  float aspect = 400.0f / 240; // the screen's width over its height (the title fits it)
  int stars = 220;             // (fewer on the DS: each is two polygons)
};

// The title's letters as boxes, centred on the origin, `height` tall (a
// capital), `depth` deep, in `colour`; and their front faces' outlines
// (pairs of points, for drawLines)
struct Letters {
  Scene::MeshBuilder mesh;
  std::vector<Scene::Vector3> outlines;
  float width = 0; // across, as made
};
Letters makeLetters(const std::string &text, float height, float depth, Scene::Colour colour);

// Stars on a sphere round the origin, each a small square facing in
Scene::MeshBuilder makeSky(int stars, float radius, unsigned seed);

class TitleScene {
public:
  TitleScene(Scene::Renderer &renderer, const Scene::MeshData &model, Settings settings);
  ~TitleScene();
  TitleScene(const TitleScene &) = delete;
  TitleScene &operator=(const TitleScene &) = delete;

  Scene::Camera camera() const;
  const Scene::Environment &environment() const { return environment_; }
  // The scene `seconds` after it started: the model and the sky turned
  void draw(Scene::Renderer &renderer, float seconds) const;

private:
  Scene::Renderer &renderer_;
  Settings settings_;
  Scene::Mesh model_, letters_, sky_;
  std::vector<Scene::Vector3> outlines_;
  float modelScale_ = 1, titleScale_ = 1;
  Scene::Vector3 modelMiddle_{};
  Scene::Environment environment_;
};

} // namespace ScratchPadd::TitleScene
