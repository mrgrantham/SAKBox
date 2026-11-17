#include <ScratchPadd/TitleScene/TitleScene.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ScratchPadd::TitleScene {

namespace {
constexpr float kPi = 3.14159265f;
// Where things are: the camera a little above, looking at the model; the
// title in front of it and higher; the sky far round
constexpr Scene::Vector3 kEye{0, 2.2f, 8.5f};
constexpr Scene::Vector3 kLookAt{0, 1.6f, 0};
constexpr Scene::Vector3 kTitleAt{0, 3.35f, 2.6f};
constexpr float kFieldOfView = 45;  // (degrees, up the screen)
constexpr float kTitleShare = 0.8f; // of the screen's width, the title across
constexpr float kSkyRadius = 40;
} // namespace

TitleScene::TitleScene(Scene::Renderer &renderer, const Scene::MeshData &model, Settings settings)
    : renderer_(renderer), settings_(std::move(settings)) {
  model_ = renderer_.load(model);
  // The model's size and middle, from its vertices: scaled to modelHeight,
  // its foot on the ground and its middle on the axis it turns about
  // (from its first vertex: on the DS a Scalar is 16.16 fixed point, which
  // can't hold a huge starting value)
  Scene::Vector3 low{}, high{};
  if (!model.vertices.empty())
    low = high = model.vertices.front().position;
  for (const Scene::Vertex &v : model.vertices) {
    low = {std::min(low.x, v.position.x), std::min(low.y, v.position.y),
           std::min(low.z, v.position.z)};
    high = {std::max(high.x, v.position.x), std::max(high.y, v.position.y),
            std::max(high.z, v.position.z)};
  }
  if (high.y > low.y)
    modelScale_ = settings_.modelHeight / (high.y - low.y);
  modelMiddle_ = {(low.x + high.x) / 2, low.y, (low.z + high.z) / 2};
  Letters letters = makeLetters(settings_.title, 1, 0.45f, settings_.letters);
  // Scaled to kTitleShare of what the screen shows across at its distance
  const float away = (kTitleAt - kEye).length();
  const float across = 2 * away * std::tan(kFieldOfView * kPi / 360) * settings_.aspect;
  const float fit = letters.width > 0 ? kTitleShare * across / letters.width : 1;
  for (const Scene::Vector3 &p : letters.outlines)
    outlines_.push_back(kTitleAt + p * fit);
  // (the letters' mesh is drawn scaled by its transform)
  letters_ = renderer_.load(letters.mesh.data());
  titleScale_ = fit;
  sky_ = renderer_.load(makeSky(settings_.stars, kSkyRadius, 7).data());
  environment_.background = settings_.background;
  environment_.ambient = {110, 110, 125};
  environment_.addLight({.direction = {-0.4f, -0.8f, -0.6f}, .colour = {210, 210, 200}});
}

TitleScene::~TitleScene() {
  renderer_.unload(model_);
  renderer_.unload(letters_);
  renderer_.unload(sky_);
}

Scene::Camera TitleScene::camera() const {
  return {.eye = kEye,
          .target = kLookAt,
          .up = {0, 1, 0},
          .fieldOfView = kFieldOfView,
          .nearPlane = 0.5f,
          .farPlane = 100};
}

void TitleScene::draw(Scene::Renderer &renderer, float seconds) const {
  // The sky, turning about a tilted axis (as the night sky about the pole)
  const float sky = 2 * kPi * seconds / settings_.skySeconds;
  renderer.draw(sky_, Scene::Transform::rotation({0.2f, 1, 0.35f}, sky),
                {.shading = Scene::Material::Shading::Unlit});
  // The model, turning on its own axis
  const float turn = 2 * kPi * seconds / settings_.modelSeconds;
  renderer.draw(model_,
                Scene::Transform::rotation({0, 1, 0}, turn) *
                    Scene::Transform::scaling({modelScale_, modelScale_, modelScale_}) *
                    Scene::Transform::translation(-modelMiddle_),
                {.shading = Scene::Material::Shading::Toon});
  // The title in front, its faces lit, its outlines bright
  renderer.draw(letters_,
                Scene::Transform::translation(kTitleAt) *
                    Scene::Transform::scaling({titleScale_, titleScale_, titleScale_}),
                {.shading = Scene::Material::Shading::Lit});
  renderer.drawLines(outlines_, settings_.edges);
}

} // namespace ScratchPadd::TitleScene
