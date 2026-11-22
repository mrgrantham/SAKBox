#include <CosmicHex/Handheld/ScreenPictures.hpp>

#include <algorithm>
#include <cmath>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

Scene::MeshBuilder pictureSquare(float uMax, float vMax) {
  Scene::MeshBuilder square;
  square.quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {255, 255, 255},
              {Scene::TexCoord{0, vMax}, Scene::TexCoord{uMax, vMax}, Scene::TexCoord{uMax, 0},
               Scene::TexCoord{0, 0}});
  return square;
}

ViewAxes viewAxes(const Scene::Camera &camera) {
  const Scene::Vector3 forward = (camera.target - camera.eye).normalised();
  const Scene::Vector3 right = forward.cross(camera.up).normalised();
  return {.forward = forward, .right = right, .up = right.cross(forward)};
}

Scene::Transform rigidBetween(const Scene::Transform &from, const Scene::Transform &to,
                              float turned, const Scene::Vector3 &place) {
  const float t = std::clamp(turned, 0.0f, 1.0f);
  const auto between = [&](const Scene::Vector3 &axis) {
    const Scene::Vector3 a = from.applyToDirection(axis), b = to.applyToDirection(axis);
    return a + (b - a) * t;
  };
  const Scene::Vector3 y = between({0, 1, 0}).normalised();
  Scene::Vector3 z = between({0, 0, 1});
  z = (z - y * z.dot(y)).normalised();
  return Scene::Transform::fromAxes(y.cross(z), y, z, place);
}

Scene::Transform onScreenAt(const Scene::Camera &camera, float screenWidth, float screenHeight,
                            float x, float y, float w, float h, float distance) {
  const auto [forward, right, up] = viewAxes(camera);
  // The screen at that distance: its height in the world, so a pixel's size
  const float tall =
      2 * distance * std::tan(static_cast<float>(camera.fieldOfView) * 3.14159265f / 360);
  const float pixel = tall / screenHeight;
  const Scene::Vector3 middle = camera.eye + forward * distance;
  // The rectangle's bottom left, from the screen's middle
  const float left = (x - screenWidth / 2) * pixel;
  const float bottom = (screenHeight / 2 - (y + h)) * pixel;
  return Scene::Transform::fromAxes(right * (w * pixel), up * (h * pixel), -forward,
                                    middle + right * left + up * bottom);
}

Scene::Transform solidOnScreen(const Scene::Camera &camera, float screenWidth, float screenHeight,
                               float x, float y, float pixels, float distance) {
  const auto [forward, right, up] = viewAxes(camera);
  const float tall =
      2 * distance * std::tan(static_cast<float>(camera.fieldOfView) * 3.14159265f / 360);
  const float pixel = tall / screenHeight;
  const float unit = pixels * pixel;
  const Scene::Vector3 at = camera.eye + forward * distance +
                            right * ((x - screenWidth / 2) * pixel) +
                            up * ((screenHeight / 2 - y) * pixel);
  return Scene::Transform::fromAxes(right * unit, up * unit, -forward * unit, at);
}

PaddedPicture padded(const std::vector<Scene::Colour> &pixels, int width, int height) {
  const auto powerOfTwo = [](int n) {
    int p = 8; // (the DS's smallest)
    while (p < n)
      p *= 2;
    return p;
  };
  PaddedPicture out;
  out.width = powerOfTwo(width);
  out.height = powerOfTwo(height);
  out.pixels.assign(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height),
                    Scene::Colour{0, 0, 0, 0});
  for (int row = 0; row < height; ++row)
    for (int col = 0; col < width; ++col)
      out.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(out.width) +
                 static_cast<std::size_t>(col)] =
          pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(col)];
  out.uMax = static_cast<float>(width) / static_cast<float>(out.width);
  out.vMax = static_cast<float>(height) / static_cast<float>(out.height);
  return out;
}

} // namespace CosmicHex::Handheld
