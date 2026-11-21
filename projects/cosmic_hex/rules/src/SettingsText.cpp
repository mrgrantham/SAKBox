// SettingsText.cpp - the settings as one line of text ("settings win=12
// ships=3 ..."), as a state's text carries them, and read back: every
// number a rule uses, so a log replays under the rules it was played with.
#include "RulesInternal.hpp"

#include <sstream>

namespace CosmicHex {

using namespace Internal;

namespace {

// The home's income steps as one list (from, amount, from, amount...), and back
std::vector<int> flatSteps(const std::vector<std::array<int, 2>> &steps) {
  std::vector<int> out;
  for (const auto &[from, amount] : steps) {
    out.push_back(from);
    out.push_back(amount);
  }
  return out;
}

} // namespace

namespace Internal {

std::string settingsText(const Settings &s) {
  std::ostringstream out;
  out << "settings win=" << s.winPoints << " ships=" << s.maxShips << " move=" << s.shipMove
      << " boost=" << s.boostMove << " core=" << s.coreRadius
      << " control=" << s.coreStationsForControl << "," << s.coreControlPoints
      << " badges=" << s.badgesPerPoint << " ruins=" << s.ruinsBadges
      << " salvage=" << s.salvageResources << " bank=" << s.bankRate << " alien=" << s.alienRate
      << "," << s.alienStationRate << " pays=" << s.stationPays << "," << s.colonyPays
      << " start=" << s.startResources
      << (s.startCards > 0 ? " startcards=" + std::to_string(s.startCards) : "")
      << " bonus=" << joined(s.seatBonus) << " corners=" << (s.zoneCorners ? 1 : 0)
      << " tiles=" << joined(s.planetTiles) << " numbers=" << joined(s.numbers)
      << " finds=" << joined(s.finds) << " ship=" << joined(s.shipCost)
      << " station=" << joined(s.stationCost) << " kinds=" << joined(s.kindWeights)
      << " exhaust=" << s.exhaustHomesAt << "," << (s.exhaustedKindRandom ? 1 : 0)
      << " colony=" << joined(s.colonyCost) << " contract=" << joined(s.contractCost)
      << " corehexes=";
  std::vector<int> core;
  for (Hex hex : s.coreHexes) {
    core.push_back(hex.q);
    core.push_back(hex.r);
  }
  out << joined(core) << " homes=";
  std::vector<int> homeList;
  for (Hex hex : s.homeHexes) {
    homeList.push_back(hex.q);
    homeList.push_back(hex.r);
  }
  out << joined(homeList) << " extratiles=" << joined(s.extraTiles)
      << " extranumbers=" << joined(s.extraNumbers) << " catchup=" << s.catchUpBelow << ","
      << s.catchUpGap << " homesteps=" << joined(flatSteps(s.homeIncomeSteps))
      << " skipfirst=" << (s.firstSkipsIncome ? 1 : 0) << " discardover=" << s.discardOver
      << " charters=" << joined(s.charters) << " enforcers=" << s.enforcers << ","
      << s.embargoEnforcers << "," << s.embargoPoints << " cartels=" << s.cartels << ","
      << s.cartelTake << " autocash=" << (s.autoCashBadges ? 1 : 0) << " relay=" << s.relayPoints
      << " dominion=" << s.dominionPoints << "," << s.dominionColonies
      << " finishround=" << (s.finishRound ? 1 : 0) << " homedie=" << (s.homeDie ? 1 : 0)
      << " shipyards=" << s.shipyards << " supply=" << s.supplyDrops << " moons=" << s.moons
      << " planets=";
  std::vector<int> planetList;
  for (Hex hex : s.planetHexes) {
    planetList.push_back(hex.q);
    planetList.push_back(hex.r);
  }
  out << joined(planetList) << " zone=";
  std::vector<int> zoneList;
  for (Hex hex : s.zoneHexes) {
    zoneList.push_back(hex.q);
    zoneList.push_back(hex.r);
  }
  out << joined(zoneList) << " edges=" << (s.corners ? 1 : 0) << "," << s.startShipReach << ","
      << (s.deepSpaceStart ? 1 : 0) << " firstboost=" << (s.firstRoundBoost ? 1 : 0)
      << " choosehome=" << (s.chooseHome ? 1 : 0) << " piles=" << (s.fitPiles ? 1 : 0) << ","
      << (s.redNumbersApart ? 1 : 0);
  return out.str();
}

std::optional<Settings> readSettings(std::string_view line) {
  if (!line.starts_with("settings "))
    return std::nullopt;
  Settings s;
  const auto five = [](const std::vector<int> &v, Resources &into) {
    if (v.size() != kResources)
      return false;
    std::copy(v.begin(), v.end(), into.begin());
    return true;
  };
  for (std::string_view field : split(line.substr(9), ' ')) {
    const std::size_t eq = field.find('=');
    if (eq == std::string_view::npos)
      return std::nullopt;
    const std::string_view key = field.substr(0, eq);
    const auto values = intList(field.substr(eq + 1));
    if (values && (key == "extratiles" || key == "extranumbers")) {
      (key == "extratiles" ? s.extraTiles : s.extraNumbers) = *values;
      continue;
    }
    if (values && key == "homesteps" && values->size() % 2 == 0) {
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.homeIncomeSteps.push_back({(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "catchup" && values->size() == 2) {
      s.catchUpBelow = (*values)[0];
      s.catchUpGap = (*values)[1];
      continue;
    }
    if (values && key == "skipfirst" && values->size() == 1) {
      s.firstSkipsIncome = (*values)[0] != 0;
      continue;
    }
    if (values && key == "charters") {
      s.charters = *values;
      continue;
    }
    if (values && key == "enforcers" && values->size() == 3) {
      s.enforcers = (*values)[0];
      s.embargoEnforcers = (*values)[1];
      s.embargoPoints = (*values)[2];
      continue;
    }
    if (values && key == "cartels" && values->size() == 2) {
      s.cartels = (*values)[0];
      s.cartelTake = (*values)[1];
      continue;
    }
    if (values && key == "autocash" && values->size() == 1) {
      s.autoCashBadges = (*values)[0] != 0;
      continue;
    }
    if (values && key == "relay" && values->size() == 1) {
      s.relayPoints = (*values)[0];
      continue;
    }
    if (values && key == "dominion" && values->size() == 2) {
      s.dominionPoints = (*values)[0];
      s.dominionColonies = (*values)[1];
      continue;
    }
    if (values && key == "finishround" && values->size() == 1) {
      s.finishRound = (*values)[0] != 0;
      continue;
    }
    if (values && key == "homedie" && values->size() == 1) {
      s.homeDie = (*values)[0] != 0;
      continue;
    }
    if (values && key == "shipyards" && values->size() == 1) {
      s.shipyards = (*values)[0];
      continue;
    }
    if (values && key == "supply" && values->size() == 1) {
      s.supplyDrops = (*values)[0];
      continue;
    }
    if (values && key == "discardover" && values->size() == 1) {
      s.discardOver = (*values)[0];
      continue;
    }
    if (values && key == "homes" && values->size() % 2 == 0) {
      s.homeHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.homeHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "planets" && values->size() % 2 == 0) {
      s.planetHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.planetHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "piles" && values->size() == 2) {
      s.fitPiles = (*values)[0] != 0;
      s.redNumbersApart = (*values)[1] != 0;
      continue;
    }
    if (values && key == "edges" && values->size() == 3) {
      s.corners = (*values)[0] != 0;
      s.startShipReach = (*values)[1];
      s.deepSpaceStart = (*values)[2] != 0;
      continue;
    }
    if (values && key == "choosehome" && values->size() == 1) {
      s.chooseHome = (*values)[0] != 0;
      continue;
    }
    if (values && key == "firstboost" && values->size() == 1) {
      s.firstRoundBoost = (*values)[0] != 0;
      continue;
    }
    if (values && key == "zone" && values->size() % 2 == 0) {
      s.zoneHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.zoneHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (values && key == "moons" && values->size() == 1) {
      s.moons = (*values)[0];
      continue;
    }
    if (values && key == "corehexes" && values->size() % 2 == 0) {
      s.coreHexes.clear();
      for (std::size_t i = 0; i < values->size(); i += 2)
        s.coreHexes.push_back(Hex{(*values)[i], (*values)[i + 1]});
      continue;
    }
    if (!values || values->empty())
      return std::nullopt;
    const std::vector<int> &v = *values;
    bool ok = true;
    if (key == "win")
      s.winPoints = v[0];
    else if (key == "ships")
      s.maxShips = v[0];
    else if (key == "move")
      s.shipMove = v[0];
    else if (key == "boost")
      s.boostMove = v[0];
    else if (key == "core")
      s.coreRadius = v[0];
    else if (key == "control" && v.size() == 2) {
      s.coreStationsForControl = v[0];
      s.coreControlPoints = v[1];
    } else if (key == "badges")
      s.badgesPerPoint = v[0];
    else if (key == "ruins")
      s.ruinsBadges = v[0];
    else if (key == "salvage")
      s.salvageResources = v[0];
    else if (key == "bank")
      s.bankRate = v[0];
    else if (key == "alien") {
      s.alienRate = v[0];
      if (v.size() >= 2)
        s.alienStationRate = v[1];
    } else if (key == "pays" && v.size() == 2) {
      s.stationPays = v[0];
      s.colonyPays = v[1];
    } else if (key == "bonus" && v.size() == kMaxPlayers) {
      std::copy(v.begin(), v.end(), s.seatBonus.begin());
    } else if (key == "start")
      s.startResources = v[0];
    else if (key == "startcards")
      s.startCards = v[0];
    else if (key == "corners")
      s.zoneCorners = v[0] != 0;
    else if (key == "tiles")
      ok = five(v, s.planetTiles);
    else if (key == "numbers")
      s.numbers = v;
    else if (key == "finds" && v.size() == kFinds)
      std::copy(v.begin(), v.end(), s.finds.begin());
    else if (key == "ship")
      ok = five(v, s.shipCost);
    else if (key == "station")
      ok = five(v, s.stationCost);
    else if (key == "kinds")
      ok = five(v, s.kindWeights);
    else if (key == "exhaust") {
      s.exhaustHomesAt = v[0];
      s.exhaustedKindRandom = v.size() >= 2 && v[1] != 0;
    } else if (key == "colony")
      ok = five(v, s.colonyCost);
    else if (key == "contract")
      ok = five(v, s.contractCost);
    else
      ok = false;
    if (!ok)
      return std::nullopt;
  }
  return s;
}

} // namespace Internal

std::string settingsLine(const Settings &settings) { return settingsText(settings); }
std::optional<Settings> settingsFromLine(std::string_view line) { return readSettings(line); }

} // namespace CosmicHex
