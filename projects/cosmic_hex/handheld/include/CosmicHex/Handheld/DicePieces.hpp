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

#include <CosmicHex/Handheld/BoardPieces.hpp>

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

// The home die (the user's): the home's own tile pulled up out of the
// board, an extruded hexagon, a warm grey, a long prism lying along x, a
// coloured hexagon inset on each of its six sides: side i (0 to 4) in
// `resources[i]`, side 5 the star in `star`. Side i faces +z after turning
// it homeDieTurn(i) about x; 1 across its sides, kHomeDieLength long. Its
// +x end is open: the home's tile is drawn there (homeDieFace), the
// board's own tile mesh with the face the board shows
Scene::MeshBuilder homeDie(const std::array<Scene::Colour, 5> &resources, Scene::Colour star);
inline constexpr float kHomeDieLength = 1.3f;
// The turn about x (radians) that brings side `face` to face +z
float homeDieTurn(int face);

// The home die beside the board: scaled by this, its hexagon is its
// tile's (the board's tiles are kTileInset from middle to corner, so
// sqrt(3) x kTileInset across their sides; the mesh is 1)
inline constexpr float kHomeDieScale = 1.7320508f * kTileInset;
// How far a pose's `lifted` has come when the die has risen straight up
// out of its place, clear of the board (from there it arcs to face you)
inline constexpr float kHomeDieRisen = 0.3f;

// The home die's poses: rigid (no scale; homeDieBody scales its mesh),
// its long axis x, its face the +x end.
// In the board: standing in its tile's place (`tile`: the tile's middle),
// its face where the tile lies, the rest of it below the board, its
// hexagon turned as the tile's
Scene::Transform homeDieInBoard(const Scene::Vector3 &tile);
// Facing you: centred on the pixel (x, y) of a `width` x `height`
// screen, `across` pixels across its sides, its long axis across the
// screen and turned a little so its face (on the right) shows, side 0
// towards you
Scene::Transform homeDieFacing(const Scene::Camera &camera, float width, float height, float x,
                               float y, float across);
// Between the two, `lifted` of the way (0 in the board, 1 facing you):
// first straight up out of its place to kHomeDieRisen, its axis still
// upright (a corner swung down while in its hole would cut the board), then
// along an arc bowing out by `bow`, turning to face you as it goes (path
// and timing apart: the caller eases `lifted`). `sink` lower (a dip
// into the board, a settle), turned `turn` about its long axis
Scene::Transform homeDiePose(const Scene::Transform &inBoard, const Scene::Transform &facing,
                             const Scene::Vector3 &bow, float lifted, float sink, float turn);
// The die's mesh at `pose` (scaled to its tile), and its face there: the
// board's tile mesh (BoardPieces::hexTile) over its +x end, lying as it
// does on the board when the die is in it
Scene::Transform homeDieBody(const Scene::Transform &pose);
Scene::Transform homeDieFace(const Scene::Transform &pose);
// The home's place while its die is out: an opening onto empty space (the
// board's tile mesh, drawn in the background's colour), a little wider
// than the die so its sides never show past it, `below` under where the
// tile lies (`tile`: its middle). Lower than the die's settle sinks and
// higher than its dip, so the die sinks into space and rises out of it, as
// through a wormhole, and the place reads as never having had a tile (the
// user's: no dark pit walls)
Scene::Transform homeDieHole(const Scene::Vector3 &tile, float below);
inline constexpr float kHomeDieHoleWide =
    0.98f / kTileInset; // (the tile mesh scaled: 0.98 of the hex)

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
