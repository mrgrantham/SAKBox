// LabLayouts.hpp - board layouts as the balance lab shows, edits and saves
// them: the homes, planets and finds' places of a board (a line of a layouts
// file), what's on each hex, cycling a hex between empty, planet and find,
// where finds go when a layout doesn't say, and how many corners a
// deep-space start could use. No drawing here, so it's tested on its own.
#pragma once

#include <CosmicHex/Rules.hpp>
#include <HexGrid/CornersAndEdges.hpp>
#include <HexGrid/Hex.hpp>

#include "../../simulate/Batch.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace CosmicHex::Lab {

inline constexpr int kBoardRadius = 4; // (the rules' board)

struct LabLayout {
  std::string name;
  std::string file; // the layouts file it's from (and saved to)
  std::vector<Hex> homes;
  std::vector<Hex> planets; // besides the homes (the core included)
  std::vector<Hex> zone;    // the finds' places; empty: the usual places
  std::string reach;        // (from the file, for people reading it: "2,2,3")
};

enum class HexKind { Empty, Planet, Find, Home };

// The finds' places when a layout doesn't say: the hexes 2 from the
// centre and the board's six corners, less those a planet or home takes
// (the rules' zoneCorners layout)
inline std::vector<Hex> usualZone(const LabLayout &layout) {
  std::vector<Hex> out;
  for (Hex hex : HexGrid::spiral(Hex{}, kBoardRadius)) {
    const int length = HexGrid::length(hex);
    const bool corner = length == kBoardRadius && (hex.q == 0 || hex.r == 0 || hex.s() == 0);
    if (length != 2 && !corner)
      continue;
    const auto taken = [&](const std::vector<Hex> &list) {
      return std::find(list.begin(), list.end(), hex) != list.end();
    };
    if (!taken(layout.planets) && !taken(layout.homes))
      out.push_back(hex);
  }
  return out;
}

// The finds' places the layout plays with
inline std::vector<Hex> zoneOf(const LabLayout &layout) {
  return layout.zone.empty() ? usualZone(layout) : layout.zone;
}

inline HexKind kindAt(const LabLayout &layout, Hex hex) {
  const auto has = [hex](const std::vector<Hex> &list) {
    return std::find(list.begin(), list.end(), hex) != list.end();
  };
  if (has(layout.homes))
    return HexKind::Home;
  if (has(layout.planets))
    return HexKind::Planet;
  if (has(zoneOf(layout)))
    return HexKind::Find;
  return HexKind::Empty;
}

// Turns a hex from empty to a planet, a planet to a find, a find to empty
// (homes stay as they are). The finds' places become the layout's own.
inline void cycle(LabLayout &layout, Hex hex) {
  layout.zone = zoneOf(layout);
  const auto drop = [hex](std::vector<Hex> &list) {
    list.erase(std::remove(list.begin(), list.end(), hex), list.end());
  };
  switch (kindAt(layout, hex)) {
  case HexKind::Home:
    return;
  case HexKind::Empty:
    layout.planets.push_back(hex);
    break;
  case HexKind::Planet:
    drop(layout.planets);
    layout.zone.push_back(hex);
    break;
  case HexKind::Find:
    drop(layout.zone);
    break;
  }
  layout.reach.clear(); // (no longer known)
}

// Corners touching no planet or home: where a deep-space start may put a
// ship (Settings::deepSpaceStart)
inline int emptySpaceCorners(const LabLayout &layout) {
  int count = 0;
  for (Corner corner : HexGrid::everyCorner(HexGrid::spiral(Hex{}, kBoardRadius))) {
    bool onBoard = false, byAPlanet = false;
    for (Hex hex : HexGrid::hexesTouching(corner)) {
      onBoard = onBoard || HexGrid::length(hex) <= kBoardRadius;
      const HexKind kind = kindAt(layout, hex);
      byAPlanet = byAPlanet || kind == HexKind::Planet || kind == HexKind::Home;
    }
    count += onBoard && !byAPlanet;
  }
  return count;
}

namespace Detail {
inline std::string hexList(const std::vector<Hex> &hexes) {
  std::string out;
  for (std::size_t i = 0; i < hexes.size(); ++i)
    out += (i ? "," : "") + std::to_string(hexes[i].q) + "," + std::to_string(hexes[i].r);
  return out;
}
} // namespace Detail

// The layout as a line of a layouts file (Batch::readLayout reads it back)
inline std::string layoutLine(const LabLayout &layout) {
  std::string line = layout.name;
  if (!layout.reach.empty())
    line += " reach=" + layout.reach;
  line += " homes=" + Detail::hexList(layout.homes) + " planets=" + Detail::hexList(layout.planets);
  if (!layout.zone.empty())
    line += " zone=" + Detail::hexList(layout.zone);
  return line;
}

// Every layout in a layouts file, in its order
inline std::vector<LabLayout> readLayouts(const std::string &fileName) {
  std::vector<LabLayout> out;
  std::ifstream file(fileName);
  for (std::string line; std::getline(file, line);) {
    if (line.empty() || line[0] == '#')
      continue;
    std::istringstream words(line);
    LabLayout layout;
    layout.file = fileName;
    words >> layout.name;
    for (std::string field; words >> field;) {
      const std::size_t eq = field.find('=');
      if (eq == std::string::npos)
        continue;
      const std::string key = field.substr(0, eq), value = field.substr(eq + 1);
      if (key == "reach") {
        layout.reach = value;
        continue;
      }
      const auto hexes = Batch::hexesFrom(value);
      if (!hexes)
        continue;
      if (key == "homes")
        layout.homes = *hexes;
      else if (key == "planets")
        layout.planets = *hexes;
      else if (key == "zone")
        layout.zone = *hexes;
    }
    out.push_back(std::move(layout));
  }
  return out;
}

} // namespace CosmicHex::Lab
