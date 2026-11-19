#include <CosmicHex/Handheld/BoardPieces.hpp>
#include <CosmicHex/Handheld/Discovery.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>
#include <array>

namespace CosmicHex::Handheld::Discovery {

namespace {

constexpr std::array<const char *, kResources> kResourceNames{"Minerals", "Food", "Fuel", "Water",
                                                              "Crystals"};

const char *nameOf(PlanetKind kind) {
  switch (kind) {
  case PlanetKind::AsteroidBelt:
    return "Asteroid belt";
  case PlanetKind::LushPlanet:
    return "Lush planet";
  case PlanetKind::GasGiant:
    return "Gas giant";
  case PlanetKind::IceMoon:
    return "Ice moon";
  case PlanetKind::Nebula:
    return "Nebula";
  case PlanetKind::Home:
    return "Home world";
  case PlanetKind::Moon:
    return "Barren moon";
  }
  return "";
}

// What a find gives the player who found it (Rules.cpp: every one earns
// an explorer badge besides)
std::string givesOf(Find find, const Settings &settings) {
  switch (find) {
  case Find::Nothing:
    return "Empty: an explorer badge";
  case Find::Salvage:
    return "Take " + std::to_string(settings.salvageResources) + " resources";
  case Find::Ruins:
    return "+" + std::to_string(settings.ruinsBadges + 1) + " badges";
  case Find::AlienStation:
    return "Trade " + std::to_string(settings.alienRate) + " for 1 here";
  case Find::PirateHideout:
    return "Steal from each player";
  }
  return "";
}

const char *nameOf(Find find) {
  switch (find) {
  case Find::Nothing:
    return "Deep space";
  case Find::Salvage:
    return "Salvage";
  case Find::Ruins:
    return "Ancient ruins";
  case Find::AlienStation:
    return "Alien outpost";
  case Find::PirateHideout:
    return "Pirate hideout";
  }
  return "";
}

// A part's progress: 0 before `from`, 1 after `from + length`
float part(float seconds, float from, float length) {
  return std::clamp((seconds - from) / length, 0.0f, 1.0f);
}

} // namespace

std::vector<Found> between(const State &before, const State &after) {
  std::vector<Found> found;
  const int by = before.turn;
  for (std::size_t i = 0; i < after.planets.size() && i < before.planets.size(); ++i) {
    const Planet &planet = after.planets[i];
    if (!planet.flipped || before.planets[i].flipped)
      continue;
    Found f{.hex = planet.hex,
            .planet = true,
            .faceDown = Art::Face::PlanetFaceDown,
            .face = faceOf(planet),
            .number = chipOf(planet),
            .by = by,
            .name = nameOf(planet.kind)};
    f.gives = givesResources(planet.kind)
                  ? std::string(kResourceNames[static_cast<std::size_t>(planet.kind)]) + " on a " +
                        std::to_string(planet.number)
                  : "No resource: an explorer badge";
    found.push_back(std::move(f));
  }
  for (std::size_t i = 0; i < after.zone.size() && i < before.zone.size(); ++i) {
    const ZoneSite &site = after.zone[i];
    if (!site.flipped || before.zone[i].flipped)
      continue;
    found.push_back({.hex = site.hex,
                     .planet = false,
                     .faceDown = Art::Face::FindFaceDown,
                     .face = faceOf(site),
                     .by = by,
                     .name = nameOf(site.find),
                     .gives = givesOf(site.find, after.settings)});
  }
  return found;
}

Moment at(float seconds) {
  using ScratchPadd::Motion::along;
  using ScratchPadd::Motion::Curve;
  constexpr float kChipFrom = kSecondsTurn;
  constexpr float kShrinkFrom = kChipFrom + kSecondsChip + kSecondsHold;
  constexpr float kFadeFrom = kShrinkFrom + kSecondsShrink;
  return {.turn = along(Curve::EaseInOut, part(seconds, 0, kSecondsTurn)),
          .chip = along(Curve::Settle, part(seconds, kChipFrom, kSecondsChip)),
          .shrink = along(Curve::EaseInOut, part(seconds, kShrinkFrom, kSecondsShrink)),
          .fade = part(seconds, kFadeFrom, kSecondsFade),
          .done = seconds >= kSeconds};
}

} // namespace CosmicHex::Handheld::Discovery
