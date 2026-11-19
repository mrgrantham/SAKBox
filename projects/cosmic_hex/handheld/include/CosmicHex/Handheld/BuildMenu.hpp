// BuildMenu.hpp - the build screen's rows: a row for each thing you can buy, red and not selectable
// when you can't, with why ("needs 1 more Food", "no ship by a free planet"), or where it goes when
// you can.
//
// Every answer comes from the rules' own legal moves, so the screen has no
// rule of its own to get wrong: once as things are (can you build it now?),
// and once with its cost added to your cards (is there somewhere to put it,
// so that only the resources are missing?).
//
//   for (const BuildRow &row : buildRows(state)) ...  // the player to move's
#pragma once

#include <CosmicHex/Rules.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace CosmicHex::Handheld {

enum class Buildable : std::uint8_t { Ship, Station, Colony, Card };

struct BuildRow {
  Buildable what;
  Resources cost;
  bool canBuild = false;
  // Why not, when it can't be built; else where it goes
  std::string note;
  // The rules' actions that build it (where each one would go), when it can
  std::vector<Action> actions;
};

// The rows for the player whose turn it is, Ship, Station, Colony, Card
std::vector<BuildRow> buildRows(const State &state);

// A row's name as the screen shows it ("Ship", ... "Card")
const char *nameOf(Buildable what);

} // namespace CosmicHex::Handheld
