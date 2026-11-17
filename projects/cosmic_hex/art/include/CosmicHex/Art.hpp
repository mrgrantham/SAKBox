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

} // namespace CosmicHex::Art
