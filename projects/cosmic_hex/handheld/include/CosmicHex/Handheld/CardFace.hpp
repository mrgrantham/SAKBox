// CardFace.hpp - every card the consoles show, drawn one way: layered from
// cheap pieces (the user's), in a true card's shape, 2:3. A card is its
// edge, its cream body, its title band in the card's colour and its art
// band, each a plain tinted rounded rectangle cut from one small shared
// alpha texture (shapesPixels); only its icon is a picture (Art's
// cardIconRecipeOf). The top screen draws the layers as one mesh
// (layersMesh) and the icon over them; the bottom screen's canvas draws the
// same layers into pixels (pixelsOf).
//
//   const auto mesh = renderer.load(CardFace::layersMesh(Art::Card::Fuel).data());
//   const float height = CardFace::heightOf(width);
#pragma once

#include <CosmicHex/Art.hpp>

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace CosmicHex::Handheld::CardFace {

using Colour = ScratchPadd::Scene::Colour;

// A card's height for its width: 2:3, a playing card's shape
constexpr float heightOf(float width) { return width * Art::kCardHeight / Art::kCardWidth; }
inline int heightOf(int width) {
  return static_cast<int>(std::lround(heightOf(static_cast<float>(width))));
}
// How far down a card its top goes: its title band and art, with the
// cream under them (the bottom screen's rows of cards, where a whole card
// has no room)
constexpr float kTopUnits = Art::kCardArt.y + Art::kCardArt.height + 3;
inline int topHeightOf(int width) {
  return static_cast<int>(std::lround(static_cast<float>(width) * kTopUnits / Art::kCardWidth));
}
// The widest card that fits `width` x `height`
constexpr float widthToFit(float width, float height) {
  const float tallest = height * Art::kCardWidth / Art::kCardHeight;
  return width < tallest ? width : tallest;
}

// The card's colours: its title band's (cards.txt), and its title's letters
// on it (Palette::inkOnBand: dark on a light band)
Colour bandOf(Art::Card card);
Colour titleInkOf(Art::Card card);

// The shared texture's rounded rectangles, one for each shape of layer, so
// each is stretched only in its own proportions
enum class Shape : std::uint8_t { Card, TitleBand, ArtBand };
// A layer: a rounded rectangle in the card's units (Art.hpp), tinted
struct Layer {
  Art::CardArea area;
  Shape shape;
  Colour colour;
};
inline constexpr int kLayers = 4;
// A card's layers, back to front: its edge, its body, its title band and
// its art band
std::array<Layer, kLayers> layersOf(Art::Card card);

// The shared texture: kShapesSize a side, white, each shape's rounded
// rectangle its alpha, in its own area (whole texels, a clear texel round
// each)
inline constexpr int kShapesSize = 64;
struct ShapeArea {
  int x, y, width, height;
  float radius; // (its corners', in texels)
};
ShapeArea areaOf(Shape shape);
std::vector<Colour> shapesPixels();

// How far each layer sits in front of the one before it, in the world's
// units (pictures are kPictureDistance from the camera), and the icon in
// front of them all: kIconLift. Cards drawn over one another must be
// further apart than that (kCardStep)
inline constexpr float kLayerStep = 0.005f;
inline constexpr float kIconLift = kLayerStep * kLayers;
inline constexpr float kCardStep = kIconLift + kLayerStep;
// A card's layers as one mesh over a unit square (its corners (0, 0)
// bottom left to (1, 1) top right, as ScreenPictures' pictureSquare), each
// layer a quad tinted by its vertices' colour, its texture the shapes'
// (one draw a card, four quads)
ScratchPadd::Scene::MeshBuilder layersMesh(Art::Card card);

// Where the icon goes on a card `width` wide, in whole pixels from its top
// left
struct PixelArea {
  int x, y, width, height;
};
PixelArea iconPixelsOn(int width);
// The whole card as pixels (RGBA, rows from the top), `width` x
// heightOf(width): its layers and `icon` (drawn iconPixelsOn(width)'s size)
// over them; its words are the caller's
std::vector<Colour> pixelsOf(Art::Card card, int width, std::span<const Colour> icon);

} // namespace CosmicHex::Handheld::CardFace
