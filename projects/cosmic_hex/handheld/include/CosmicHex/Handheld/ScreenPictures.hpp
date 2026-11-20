// ScreenPictures.hpp - pictures on the top screen, placed as flat pieces
// just in front of the camera (the user's choice A; pictures in
// the overlay itself, choice B, kept as a later option): a card, a card's
// back, a planet's face, covering a rectangle of the screen's pixels as the
// overlay's shapes do, drawn with the textured meshes every platform has.
//
//   const auto square = renderer.load(pictureSquare(card.uMax, card.vMax).data());
//   renderer.draw(square, onScreenAt(camera, 256, 192, x, y, w, h),
//                 {.shading = Unlit, .texture = cardTexture});
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <vector>

namespace CosmicHex::Handheld {

// How far in front of the camera pictures go: just past its near plane
// (the board's cameras: BoardView's 3), so they're in front of everything,
// where the DS's depth buffer is finest
inline constexpr float kPictureDistance = 3.4f;

// A square picture: its corners (0, 0) bottom left to (1, 1) top right,
// facing +z, its texture from (0, 0) to (uMax, vMax) (a picture padded to
// the DS's power-of-two sizes uses only part of its texture)
ScratchPadd::Scene::MeshBuilder pictureSquare(float uMax = 1, float vMax = 1);

// Where to draw pictureSquare so it covers the pixels (x, y) to (x + w,
// y + h) of a `screenWidth` x `screenHeight` screen (y down from the top),
// `distance` in front of the camera, facing it
ScratchPadd::Scene::Transform onScreenAt(const ScratchPadd::Scene::Camera &camera,
                                         float screenWidth, float screenHeight, float x, float y,
                                         float w, float h, float distance = kPictureDistance);

// Where to draw a solid (a mesh in 3D) centred on the pixel (x, y),
// `pixels` on the screen to its unit, `distance` in front of the camera,
// its x right, y up and z towards the camera
ScratchPadd::Scene::Transform solidOnScreen(const ScratchPadd::Scene::Camera &camera,
                                            float screenWidth, float screenHeight, float x, float y,
                                            float pixels, float distance = kPictureDistance + 0.4f);

// A picture (RGBA, rows from the top) padded with see-through pixels to
// the next power of two each way, as the DS's textures must be; uMax and
// vMax are the picture's share of it
struct PaddedPicture {
  int width = 0, height = 0;
  std::vector<ScratchPadd::Scene::Colour> pixels;
  float uMax = 1, vMax = 1;
};
PaddedPicture padded(const std::vector<ScratchPadd::Scene::Colour> &pixels, int width, int height);

} // namespace CosmicHex::Handheld
