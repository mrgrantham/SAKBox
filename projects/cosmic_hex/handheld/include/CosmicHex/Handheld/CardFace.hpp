// CardFace.hpp - every card the consoles show, drawn one way: layered from
// cheap pieces (the user's), in one shape everywhere, 2:3 (kHeightPerWidth).
// A card is its edge, its cream body, its title band in the card's colour
// and its art band, each a plain tinted rounded rectangle cut from one small
// shared alpha texture (shapesPixels); only its icon is a picture (Art's
// cardIconRecipeOf). The top screen draws the layers as one mesh
// (layersMesh) and the icon over them; the bottom screen's canvas draws the
// same layers into pixels (pixelsOf). A card turning over from its back to
// its face is turnOf's, on both screens.
//
//   const auto mesh = renderer.load(CardFace::layersMesh(Art::Card::Fuel).data());
//   const float height = CardFace::heightOf(width);
//   const CardFace::Turn turn = CardFace::turnOf(0.3f); // (its back, narrowing)
#pragma once

#include <CosmicHex/Art.hpp>

#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Types.hpp>
#include <ScratchPadd/TextureRecipe/TextureRecipe.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace CosmicHex::Handheld::CardFace {

using Colour = ScratchPadd::Scene::Colour;

// The one shape of every card, faces and backs, on both screens: its
// height for its width, 2:3, a playing card's (the user's). Nothing else
// sizes a card's height: heightOf and widthToFit are the ways to it. The
// card's units (Art.hpp's 63 x 94.5) and its backs' drawings keep to it
inline constexpr float kHeightPerWidth = 1.5f;
static_assert(Art::kCardHeight == Art::kCardWidth * kHeightPerWidth,
              "Art's card units are a card's shape");
constexpr float heightOf(float width) { return width * kHeightPerWidth; }
inline int heightOf(int width) {
  return static_cast<int>(std::lround(heightOf(static_cast<float>(width))));
}
// The widest card that fits `width` x `height`
constexpr float widthToFit(float width, float height) {
  const float tallest = height / kHeightPerWidth;
  return width < tallest ? width : tallest;
}

// Where a card's art band and its icon are, in its units. An action card
// keeps Art's art band (kCardArt), its words in the cream under it
// (kCardWords); a resource card has no words, so its art band goes down to
// its foot and its icon sits in the card's middle (the user's: no empty
// cream under it)
bool hasWords(Art::Card card);
Art::CardArea artOf(Art::Card card);

// The icon: its drawing (Art's, kCardIcon's shape, 2:1, on a see-through
// ground) drawn kIconScale bigger than Art's area for it, a resource's
// bigger still (its drawing is a round mark in the middle of its ground,
// and its art band is tall), centred where Art's area is (a resource's in
// the card's middle): the user's, the icons a little larger than the
// layered cards first drew them. The drawing's ground spills past the art
// band, so only its part inside the band, kIconMargin in from its edges,
// is shown (iconOf), the drawing itself inside that (CardFace_test)
inline constexpr float kIconScale = 1.2f, kResourceIconScale = 1.65f;
inline constexpr float kIconMargin = 2;
// The whole drawing's area on the card (past the band where its ground
// is), and the part of it shown, both in the card's units
Art::CardArea iconDrawingOf(Art::Card card);
Art::CardArea iconOf(Art::Card card);
// The part shown as a share of the drawing, 0 to 1 each way from its top
// left: where a texture or the pixels of it are cut from
struct IconCut {
  float left = 0, top = 0, right = 1, bottom = 1;
};
IconCut iconCutOf(Art::Card card);
// The shown part's pixels, `width` x `height` (RGBA, rows from the top):
// the drawing drawn at the size that makes its shown part that big, and
// that part cut out
std::vector<Colour> iconPixels(
    Art::Card card, int width, int height,
    ScratchPadd::TextureRecipe::Smoothing smoothing = ScratchPadd::TextureRecipe::Smoothing::Fine);

// The card's colours: its title band's (cards.txt), and its title's letters
// on it (Palette::inkOnBand: dark on a light band)
Colour bandOf(Art::Card card);
Colour titleInkOf(Art::Card card);

// The shared texture's rounded rectangles, one for each shape of layer, so
// each is stretched only in its own proportions (a resource card's art
// band, much taller than an action card's, is a shape of its own)
enum class Shape : std::uint8_t { Card, TitleBand, ArtBand, TallArtBand };
inline constexpr std::array kShapes{Shape::Card, Shape::TitleBand, Shape::ArtBand,
                                    Shape::TallArtBand};
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

// Where `card`'s icon's shown part goes on it `width` wide, in whole
// pixels from its top left
struct PixelArea {
  int x, y, width, height;
};
PixelArea iconPixelsOn(Art::Card card, int width);
// The whole card as pixels (RGBA, rows from the top), `width` x
// heightOf(width): its layers and `icon` (iconPixels at iconPixelsOn's
// size) over them; its words are the caller's
std::vector<Colour> pixelsOf(Art::Card card, int width, std::span<const Colour> icon);

// A card turning over, from its back (`turned` 0) to its face (1), about
// its upright middle, as a card is turned at a table (the user's): how much
// of its width shows (its width through |cos|, none edge on, halfway), and
// whether its face shows yet (from halfway). Every card turned over, on
// either screen, is drawn by it; the shows time it (kSecondsTurnOver)
struct Turn {
  float across = 1;
  bool face = true;
};
Turn turnOf(float turned);

} // namespace CosmicHex::Handheld::CardFace
