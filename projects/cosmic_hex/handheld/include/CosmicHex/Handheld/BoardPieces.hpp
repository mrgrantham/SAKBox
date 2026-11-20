// BoardPieces.hpp - Cosmic Hex's board in 3D, as the consoles draw it: where
// each hex and corner is (the board lies flat, x across and z towards the
// viewer, y up), the pieces' meshes, made in code in vertex colours (no
// textures: the Tron look), and their colours: the balance lab's, so a board
// looks the same in both.
//
//   BoardPieces pieces;
//   renderer.load(pieces.hexTile.data());        // once
//   Transform at = BoardPieces::onHex(planet.hex); // where to draw it
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Handheld/Palette.hpp>
#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

// Colours: space and the lab's, planets by kind once turned over, players'
// pieces each in their own
struct BoardColours {
  static constexpr Scene::Colour kSpace = Palette::kSpace;
  static constexpr Scene::Colour kFaceDown{140, 214, 150}; // the lab's planets
  static constexpr Scene::Colour kHome{240, 180, 90};
  static constexpr Scene::Colour kZone{56, 74, 106};     // the lab's finds
  static constexpr Scene::Colour kFound{84, 102, 136};   // a find turned over
  static constexpr Scene::Colour kOutpost{70, 200, 190}; // an alien outpost found
  static constexpr Scene::Colour kBlockade{30, 30, 34};
  static Scene::Colour ofKind(PlanetKind kind);
  static Scene::Colour ofPlayer(int player);
  // A tile face's main colour, where tiles can't show their faces (the DS,
  // until the scene layer has textures there)
  static Scene::Colour ofFace(Art::Face face);
};

// The face a tile shows: a planet face down or by its
// kind, a home, an exhausted home by what it gives; a find face down or what
// was found
Art::Face faceOf(const Planet &planet);
Art::Face faceOf(const ZoneSite &site);

// The number chip a tile shows: a turned-over planet's number, or
// 0 for none (face down, a moon, a home not yet exhausted)
int chipOf(const Planet &planet);

// Where a tile's number chip sits, as a part of the hex (1 to a corner):
// over the lower part of the face, towards the viewer, and how big
inline constexpr float kChipDown = 0.42f;
inline constexpr float kChipRadius = 0.3f;

// How much of a hex its tile covers, centre to corner: the rest is the grid
// between the tiles (the user's)
inline constexpr float kTileInset = 0.94f;

// How far the thin layers sit above their tile: the number chip, and the
// rings marking the chosen corner or hex. Enough that the DS's coarse depth buffer keeps them
// in front from anywhere the board's cameras see them (BoardView.hpp;
// At 0.015 and 0.02 they could lose triangles to the tile)
inline constexpr float kChipLift = 0.04f;
inline constexpr float kRingLift = 0.04f;

// The pieces' meshes, each centred on its own place, 1 unit to a hex's
// corner-to-centre
struct BoardPieces {
  BoardPieces();
  Scene::MeshBuilder hexTile;    // a flat hexagon, its face (Art.hpp) on it by texture
  Scene::MeshBuilder numberChip; // a small square over a tile's lower part, its chip by texture
  Scene::MeshBuilder station;    // a small box
  Scene::MeshBuilder colony;     // a taller box with a roof
  Scene::MeshBuilder ship;       // a dart, pointing along +x
  Scene::MeshBuilder flame;      // a thin spike along -x from its base, 1 long (scaled to the burn)
  Scene::MeshBuilder hexRing;    // a thin ring round a hex, for the blockade's chosen hex
  Scene::MeshBuilder cornerRing; // a small ring round a corner, for the touched corner
  // A tall hexagonal column round a tile, its six sides both ways (seen
  // from in front and behind), drawn see-through: a planet paying on a
  // roll lit up (the user's)
  Scene::MeshBuilder column;
  // A sparse, dim starfield below the board (the user's): small
  // squares scattered wide, drawn unlit, the same every game
  Scene::MeshBuilder stars;

  // Where things are, on the table
  static Scene::Vector3 centre(Hex hex);
  static Scene::Vector3 at(Corner corner);
  static Scene::Transform onHex(Hex hex);
  static Scene::Transform onCorner(Corner corner);

  // The gaps between the tiles (the user's): a thin ring round each
  // tile, from its edge (kTileInset) out to its hex's, in the tiles' own
  // plane; one mesh for the board, drawn light grey and translucent. Nothing
  // of it lies under a tile (a full hexagon a hair under each
  // tile won the DS's depth test for whole triangles of the far tiles)
  static Scene::MeshBuilder gapsBetween(const std::vector<Hex> &tiles);
};

} // namespace CosmicHex::Handheld
