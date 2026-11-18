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

// A planet's number chip (round 87): the number on a round chip, a dot for
// each way two dice make it; 2 to 12 but 7 (no planet has one). The chip's
// recipe, or empty for a number without one
inline constexpr int kLowestChip = 2;
inline constexpr int kHighestChip = 12;
std::string_view chipRecipeOf(int number);

// The board screen's buttons' icons (round 94: icons/*.svg, to swap for
// other drawings), each on a transparent square, drawn on its button
enum class Icon : std::uint8_t { Roll, Build, Trade, ShowResources, Cards, EndTurn };
inline constexpr int kIcons = 6;
std::string_view iconRecipeOf(Icon icon);

// The cards (round 75's designs: cards/*.svg). Each drawing is the card
// without its words: its frame, the title band in the card's colour and the
// art. The game writes the title and the card's words over it, in the areas
// below (round 94: titles centred, up to two lines, the same band on every
// card), so a new drawing keeps to them.
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
std::string_view cardRecipeOf(Card card);

// A card's areas, in its drawing's units (63 x 88, a playing card's shape)
struct CardArea {
  float x, y, width, height;
};
inline constexpr float kCardWidth = 63, kCardHeight = 88;
inline constexpr CardArea kCardTitle{3, 3, 57, 16}; // the title band
inline constexpr CardArea kCardArt{3, 21, 57, 30};  // the picture (a small card shows down to here)
inline constexpr CardArea kCardWords{5, 54, 53, 31}; // what it does

// A card's words (round 97: cards/cards.txt, embedded by the build, so
// they're edited as text): its title, its short title for small cards, and
// what it does
struct CardWords {
  std::string_view title, shortTitle, words;
};
CardWords wordsOf(Card card);
// The card's drawing's file name in cards/, without ".svg"
std::string_view fileOf(Card card);

} // namespace CosmicHex::Art
