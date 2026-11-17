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
#include <CosmicHex/Rules.hpp>

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

namespace CosmicHex::Handheld {

namespace Scene = ScratchPadd::Scene;

// Colours: space and the lab's, planets by kind once turned over, players'
// pieces each in their own
struct BoardColours {
  static constexpr Scene::Colour kSpace{24, 34, 55};
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

// The face a tile shows (round 75's designs): a planet face down or by its
// kind, a home, an exhausted home by what it gives; a find face down or what
// was found
Art::Face faceOf(const Planet &planet);
Art::Face faceOf(const ZoneSite &site);

// The number chip a tile shows (round 87): a turned-over planet's number, or
// 0 for none (face down, a moon, a home not yet exhausted)
int chipOf(const Planet &planet);

// Where a tile's number chip sits, as a part of the hex (1 to a corner):
// over the lower part of the face, towards the viewer, and how big
inline constexpr float kChipDown = 0.42f;
inline constexpr float kChipRadius = 0.3f;

// The pieces' meshes, each centred on its own place, 1 unit to a hex's
// corner-to-centre
struct BoardPieces {
  BoardPieces();
  Scene::MeshBuilder hexTile;    // a flat hexagon, its face (Art.hpp) on it by texture
  Scene::MeshBuilder numberChip; // a small square over a tile's lower part, its chip by texture
  Scene::MeshBuilder station;    // a small box
  Scene::MeshBuilder colony;     // a taller box with a roof
  Scene::MeshBuilder ship;       // a dart, pointing along +x
  Scene::MeshBuilder highlight;  // a thin ring round a hex, for what's chosen
  // A sparse, dim starfield below the board (round 72, the user's): small
  // squares scattered wide, drawn unlit, the same every game
  Scene::MeshBuilder stars;

  // Where things are, on the table
  static Scene::Vector3 centre(Hex hex);
  static Scene::Vector3 at(Corner corner);
  static Scene::Transform onHex(Hex hex);
  static Scene::Transform onCorner(Corner corner);

  // The gaps between the tiles (round 79, the user's): a full-size hexagon
  // under each tile, a hair below it, so the 6% each tile is inset shows as
  // a thin line; one mesh for the board, drawn light grey and translucent
  static Scene::MeshBuilder gapsUnder(const std::vector<Hex> &tiles);
};

} // namespace CosmicHex::Handheld
