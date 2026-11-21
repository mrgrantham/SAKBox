// OtherPlayers.hpp - what everyone can see of a player, for the other
// players' screens (the user's option B: a top screen for each, chosen
// with tabs on the bottom screen): their points, how many resource and
// action cards they hold (not which), their badges, ships, stations and
// colonies, the planets their stations touch with each one's number, and
// the Charters they've revealed. Nothing hidden from the table.
//
//   for (int p : othersOf(state, 0)) tab(p);
//   const PlayerView view = viewOf(state, chosen);
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <vector>

namespace CosmicHex::Handheld {

struct PlayerView {
  int player = 0;
  int points = 0;
  int resourceCards = 0, actionCards = 0; // (held, face down)
  int badges = 0;                         // held, not yet a point (0 to 2)
  int ships = 0, stations = 0, colonies = 0;
  struct Planet {
    PlanetKind kind;
    int number;
    bool colony; // (their colony touches it, rather than a station)
  };
  std::vector<Planet> planets;     // touched by their stations and colonies, each once
  std::vector<Art::Card> charters; // revealed
};
PlayerView viewOf(const State &state, int player);

// Every player but `you`, in turn order from the one after you
std::vector<int> othersOf(const State &state, int you);

} // namespace CosmicHex::Handheld
