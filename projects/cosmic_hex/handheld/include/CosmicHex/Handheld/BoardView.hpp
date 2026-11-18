// BoardView.hpp - the cameras the top screen sees the board with: the game's,
// from above and in front and turned by L and R, and the setup's, from
// straight above, with the board coming down from one to the other as a game
// starts (round 90, the user's).
//
//   renderer.beginFrame(cameraBetween(topDownCamera(), gameCamera(turn), down), ...);
//
// Their near plane is as far out as the dice allow: a perspective camera's
// depth buffer is finest near that plane, and the DS's is coarse, so the
// board's thin layers (the chips, the chosen hex's ring: BoardPieces.hpp)
// need it there to stay in front of the tiles under them (round 92).
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cmath>

namespace CosmicHex::Handheld {

// The cameras' near plane: the dice roll on a glass 6 in front of the
// camera (DicePieces.hpp's glassFrame), well beyond it, and the board is
// nearer than 9 nowhere; the far plane is past the stars under the board
inline constexpr float kBoardNearPlane = 3;
inline constexpr float kBoardFarPlane = 60;

// The game's camera: from above and in front, turned `turn` radians about
// the board (looking a little in front of the centre, so the near hexes stay
// on the screen)
inline ScratchPadd::Scene::Camera gameCamera(float turn) {
  const float distance = 9.5f, height = 9.5f;
  return {.eye = {distance * std::sin(turn), height, distance * std::cos(turn)},
          .target = {1.2f * std::sin(turn), 0, 1.2f * std::cos(turn)},
          .up = {0, 1, 0},
          .fieldOfView = 50,
          .nearPlane = kBoardNearPlane,
          .farPlane = kBoardFarPlane};
}

// The board from straight above, all of it on the screen (the setup's view,
// like the flat map; up the screen is away from the game's camera)
inline ScratchPadd::Scene::Camera topDownCamera() {
  return {.eye = {0, 18, 0.01f},
          .target = {0, 0, 0},
          .up = {0, 0, -1},
          .fieldOfView = 50,
          .nearPlane = kBoardNearPlane,
          .farPlane = kBoardFarPlane};
}

// `t` of the way from one camera to another (0 to 1)
inline ScratchPadd::Scene::Camera cameraBetween(const ScratchPadd::Scene::Camera &from,
                                                const ScratchPadd::Scene::Camera &to, float t) {
  ScratchPadd::Scene::Camera out = to;
  out.eye = from.eye + (to.eye - from.eye) * t;
  out.target = from.target + (to.target - from.target) * t;
  out.up = (from.up + (to.up - from.up) * t).normalised();
  return out;
}

} // namespace CosmicHex::Handheld
