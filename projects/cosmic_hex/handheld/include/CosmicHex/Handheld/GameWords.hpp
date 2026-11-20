// GameWords.hpp - the game's names for its things, in one place: the
// resources, the kinds of planet and the deep-space finds, as the screens
// write them ("Minerals", "Gas giant", "Alien outpost"). Rename one here
// and every screen follows.
//
//   GameWords::of(Resource::Fuel)          // "Fuel"
//   GameWords::of(PlanetKind::GasGiant)    // "Gas giant"
//   GameWords::of(Find::Ruins)             // "Ancient ruins"
#pragma once

#include <CosmicHex/Rules.hpp>

#include <array>
#include <string_view>

namespace CosmicHex::Handheld::GameWords {

inline constexpr std::array<std::string_view, kResources> kResources_{"Minerals", "Food", "Fuel",
                                                                      "Water", "Crystals"};
inline constexpr std::array<std::string_view, 7> kPlanetKinds{
    "Asteroid belt", "Lush planet", "Gas giant", "Ice moon", "Nebula", "Home world", "Barren moon"};
inline constexpr std::array<std::string_view, kFinds> kFinds_{
    "Deep space", "Salvage", "Ancient ruins", "Alien outpost", "Pirate hideout"};

constexpr std::string_view of(Resource resource) {
  return kResources_[static_cast<std::size_t>(resource)];
}
// (by a resource's number, 0 to kResources - 1)
constexpr std::string_view resource(std::size_t r) { return kResources_[r]; }
constexpr std::string_view of(PlanetKind kind) {
  return kPlanetKinds[static_cast<std::size_t>(kind)];
}
constexpr std::string_view of(Find find) { return kFinds_[static_cast<std::size_t>(find)]; }

} // namespace CosmicHex::Handheld::GameWords
