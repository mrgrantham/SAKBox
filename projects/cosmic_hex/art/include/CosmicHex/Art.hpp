// Art.hpp - Cosmic Hex's tile faces (the SVG files beside this package's
// BUILD), each as the texture recipe the build made of it. Draw one with
// ScratchPadd::TextureRecipe:
//
//   auto recipe = TextureRecipe::read(Art::recipeOf(Art::Face::PlanetFuel));
//   TextureRecipe::Image image = TextureRecipe::draw(*recipe, 64, 64,
//   TextureRecipe::Format::Rgba8);
//
// A face fills its square: the hex, centred, 47% of the square from its
// centre to each corner, pointy-topped (a corner at the top and bottom),
// as the board's hexes are.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace CosmicHex::Art {

enum class Face : std::uint8_t {
  PlanetFaceDown,
  PlanetMinerals,
  PlanetFood,
  PlanetFuel,
  PlanetWater,
  PlanetCrystals,
  PlanetMoon,
  Home,
  HomeExhaustedMinerals,
  HomeExhaustedFood,
  HomeExhaustedFuel,
  HomeExhaustedWater,
  HomeExhaustedCrystals,
  FindFaceDown,
  FindNothing,
  FindSalvage,
  FindRuins,
  FindOutpost,
  FindPirates,
};
inline constexpr int kFaces = 19;

// The face's recipe (its text, as TextureRecipe::read takes it)
std::string_view recipeOf(Face face);

// A planet's number chip: the number on a round chip, a dot for
// each way two dice make it; 2 to 12 but 7 (no planet has one). The chip's
// recipe, or empty for a number without one
inline constexpr int kLowestChip = 2;
inline constexpr int kHighestChip = 12;
std::string_view chipRecipeOf(int number);

// The board screen's buttons' icons (icons/*.svg, to swap for
// other drawings), each on a transparent square, drawn on its button
enum class Icon : std::uint8_t {
  Roll,
  Build,
  Trade,
  ShowResources,
  Cards,
  EndTurn, // (an arrow round to itself, in its own colour)
  OtherPlayers,
  MoveList, // (debug builds: the list of moves on the top screen, on or off)
  Confirm   // (the green check: a chosen move or placement goes ahead)
};
inline constexpr int kIcons = 9;
std::string_view iconRecipeOf(Icon icon);

// The cards. Each is drawn in layers (the consoles' CardFace.hpp): its
// edge and cream body, its title band in the card's own colour (bandOf,
// from cards.txt) and its dark art band, plain rounded rectangles; then its
// icon, the only picture: the drawing in cards/, on a see-through ground
// the shape of the icon's area (kCardIcon, 2:1). The game writes the title
// and the card's words over them, in the areas below (titles centred, the
// same band on every card), so a new icon keeps to its area.
enum class Card : std::uint8_t {
  Minerals,
  Food,
  Fuel,
  Water,
  Crystals,
  Enforcer,
  InterstellarCartel,
  ShipyardSurplus,
  SupplyDrop,
  CharterThreeOfAKind,
  CharterOccupier,
  CharterDeepSpaceRelay,
};
inline constexpr int kCards = 12;
std::string_view cardIconRecipeOf(Card card);
// A card's back, the same shape as the cards (2:3): the six kinds of planet for
// resource cards, rings of hexagons for action cards (the
// user's: one design for each kind of card)
enum class CardBack : std::uint8_t { Rings, Planets }; // back_rings.svg, back_planets.svg
std::string_view cardBackRecipe(CardBack design);
inline constexpr CardBack kResourceBack = CardBack::Planets;
inline constexpr CardBack kActionBack = CardBack::Rings;

// A card's areas, in its units (63 x 94.5: 2:3, the user's true card shape)
struct CardArea {
  float x, y, width, height;
};
inline constexpr float kCardWidth = 63, kCardHeight = 94.5f;
inline constexpr CardArea kCardTitle{3, 3, 57, 16};     // the title band
inline constexpr CardArea kCardArt{3, 21, 57, 30};      // the art band
inline constexpr CardArea kCardIcon{3.5f, 22, 56, 28};  // the icon, on the art band
inline constexpr CardArea kCardWords{5, 54, 53, 37.5f}; // what it does
// The layers' corners' radii (the card's, the bands'), in those units
inline constexpr float kCardCorner = 4, kBandCorner = 2;

// A colour, 0 to 255 each
struct Rgb {
  std::uint8_t r, g, b;
  bool operator==(const Rgb &) const = default;
};
// The layers' colours every card shares: its edge, its cream body, its art
// band; and each card's own title band (cards.txt)
inline constexpr Rgb kCardEdge{0xc9, 0xc2, 0xad};
inline constexpr Rgb kCardBody{0xf4, 0xf1, 0xe8};
inline constexpr Rgb kCardArtBand{0x18, 0x22, 0x37};
Rgb bandOf(Card card);

// A card's words (cards/cards.txt, embedded by the build, so
// they're edited as text): its title, its short title for small cards, and
// what it does
struct CardWords {
  std::string_view title, shortTitle, words;
};
CardWords wordsOf(Card card);
// The card's icon's file name in cards/, without ".svg"
std::string_view fileOf(Card card);

} // namespace CosmicHex::Art
