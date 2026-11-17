// DicePieces.hpp - the physics dice (dice/DiceTray) as the consoles draw
// them (round 87): a die's mesh (white, with dark pips; opposite faces add
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

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

struct DicePieces {
  DicePieces();
  // A die 1 across, centred: +x shows 1, -x 6, +y 2, -y 5, +z 3, -z 4
  Scene::MeshBuilder die;
};

// The container's coordinates (x across the screen, y towards the viewer,
// z down the screen; DiceTray's, looking down from above) to the world's: in
// front of `camera`, `distance` from its eye, scaled so `width` by `depth`
// fills the screen (of this width over height)
Scene::Transform glassFrame(const Scene::Camera &camera, float aspect, float width, float depth,
                            float distance = 6);

// A die in the container: at `position`, turned by the quaternion `turn`
// (w, x, y, z), the container moved `offset` to the left (the pull)
Scene::Transform dieOnGlass(const Scene::Transform &glass, const Scene::Vector3 &position,
                            const std::array<float, 4> &turn, float offset);

} // namespace CosmicHex::Handheld
