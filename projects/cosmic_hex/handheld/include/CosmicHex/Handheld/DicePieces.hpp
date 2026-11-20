// DicePieces.hpp - the physics dice (dice/DiceTray) as the consoles draw
// them: a die's mesh (white, with dark pips; opposite faces add
// to 7, as DiceTray reads them), and where the glass container is: in front
// of the camera, facing it, its width and depth filling the screen, so the
// dice seem to roll on the screen's glass, over the board.
//
//   DicePieces pieces;  renderer.load(pieces.die.data());
//   const auto glass = glassFrame(camera, 400.0f / 240, DiceTray::kWidth, DiceTray::kDepth);
//   renderer.draw(die, dieOnGlass(glass, position, turn, tray.offset()), material);
#pragma once

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <array>
#include <optional>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

struct DicePieces {
  explicit DicePieces(float size = 1);
  // A die `size` across, centred, its corners rounded off: +x shows 1, -x 6,
  // +y 2, -y 5, +z 3, -z 4
  Scene::MeshBuilder die;
};

// The home die (the user's): an extruded hexagon, a warm grey, a long prism
// lying along x, a coloured hexagon inset on each of its six sides: side
// i (0 to 4) in `resources[i]`, side 5 the star in `star`. Side i faces
// +z after turning it homeDieTurn(i) about x; 1 across its sides
Scene::MeshBuilder homeDie(const std::array<Scene::Colour, 5> &resources, Scene::Colour star);
// The turn about x (radians) that brings side `face` to face +z
float homeDieTurn(int face);

// The container's coordinates (x across the screen, y towards the viewer,
// z down the screen; DiceTray's, looking down from above) to the world's: in
// front of `camera`, `distance` from its eye, scaled so `width` by `depth`
// fills the screen (of this width over height)
Scene::Transform glassFrame(const Scene::Camera &camera, float aspect, float width, float depth,
                            float distance = 6);

// Where a point in the world shows on a screen `width` by `height` pixels
// seen from `camera` (pixels from the top left), or nothing if it's behind
// the camera: for words over the dice (the roll, sliding off with
// them)
std::optional<std::array<float, 2>> onScreen(const Scene::Camera &camera, float width, float height,
                                             const Scene::Vector3 &world);

// How much of the screen's width the glass's floor fills (a die's top is
// nearer the camera than the floor, so looks bigger: room for it at the edge)
inline constexpr float kGlassShare = 0.82f;

// A slide across the screen on the dice's curve (the user's: the
// setup's boards, switched): speeding up as the glass does when it's pulled
// away (DiceTray::kPullAcceleration, the glass kGlassShare of the screen),
// then slowing the same way to stop. How far through (0 to 1) after
// `seconds`, and how long it takes
float slideAcross(float seconds);
float slideSeconds();

// A die in the container: at `position`, turned by the quaternion `turn`
// (w, x, y, z), the container moved `offset` to the left (the pull)
Scene::Transform dieOnGlass(const Scene::Transform &glass, const Scene::Vector3 &position,
                            const std::array<float, 4> &turn, float offset);

} // namespace CosmicHex::Handheld
