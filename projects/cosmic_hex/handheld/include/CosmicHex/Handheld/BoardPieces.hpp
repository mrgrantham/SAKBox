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
  static constexpr Scene::Colour kZone{56, 74, 106}; // the lab's finds
  static constexpr Scene::Colour kBlockade{30, 30, 34};
  static Scene::Colour ofKind(PlanetKind kind);
  static Scene::Colour ofPlayer(int player);
};

// The pieces' meshes, each centred on its own place, 1 unit to a hex's
// corner-to-centre
struct BoardPieces {
  BoardPieces();
  Scene::MeshBuilder hexTile;   // a flat hexagon, white (coloured by the material)
  Scene::MeshBuilder station;   // a small box
  Scene::MeshBuilder colony;    // a taller box with a roof
  Scene::MeshBuilder ship;      // a dart, pointing along +x
  Scene::MeshBuilder highlight; // a thin ring round a hex, for what's chosen

  // Where things are, on the table
  static Scene::Vector3 centre(Hex hex);
  static Scene::Vector3 at(Corner corner);
  static Scene::Transform onHex(Hex hex);
  static Scene::Transform onCorner(Corner corner);
};

} // namespace CosmicHex::Handheld
