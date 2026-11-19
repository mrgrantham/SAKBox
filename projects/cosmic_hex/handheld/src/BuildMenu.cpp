#include <CosmicHex/Handheld/BuildMenu.hpp>

#include <algorithm>
#include <array>

namespace CosmicHex::Handheld {

namespace {

constexpr std::array<const char *, kResources> kResourceNames{"Minerals", "Food", "Fuel", "Water",
                                                              "Crystals"};

Action::Kind kindOf(Buildable what) {
  switch (what) {
  case Buildable::Ship:
    return Action::Kind::BuildShip;
  case Buildable::Station:
    return Action::Kind::Settle;
  case Buildable::Colony:
    return Action::Kind::BuildColony;
  case Buildable::Card:
    return Action::Kind::BuyContract;
  }
  return Action::Kind::EndTurn;
}

Resources costOf(const Settings &settings, Buildable what) {
  switch (what) {
  case Buildable::Ship:
    return settings.shipCost;
  case Buildable::Station:
    return settings.stationCost;
  case Buildable::Colony:
    return settings.colonyCost;
  case Buildable::Card:
    return settings.contractCost;
  }
  return {};
}

std::vector<Action> actionsOf(const std::vector<Action> &legal, Buildable what) {
  std::vector<Action> out;
  std::ranges::copy_if(legal, std::back_inserter(out),
                       [&](const Action &a) { return a.kind == kindOf(what); });
  return out;
}

// "needs 1 more Food", or "needs 1 Minerals, 1 Fuel" for more than one kind
std::string missingFrom(const Resources &held, const Resources &cost) {
  std::string out;
  int kinds = 0;
  for (std::size_t r = 0; r < cost.size(); ++r) {
    const int short_ = cost[r] - held[r];
    if (short_ <= 0)
      continue;
    out += (kinds++ ? ", " : "") + std::to_string(short_) + " " + kResourceNames[r];
  }
  return kinds == 1 ? "needs " + out.insert(out.find(' ') + 1, "more ") : "needs " + out;
}

// Why there's nowhere to put it, even with the resources
std::string nowhereFor(const State &state, Buildable what) {
  switch (what) {
  case Buildable::Ship: {
    const auto mine = std::ranges::count_if(
        state.ships, [&](const Ship &ship) { return ship.owner == state.turn; });
    if (mine >= state.settings.maxShips)
      return "all " + std::to_string(state.settings.maxShips) + " ships are out";
    return "no colony to launch from";
  }
  case Buildable::Station:
    return "no ship by a free planet";
  case Buildable::Colony:
    return "no station to grow";
  case Buildable::Card:
    return "the deck is empty";
  }
  return "";
}

// Where it goes, when it can be built
std::string whereFor(Buildable what) {
  switch (what) {
  case Buildable::Ship:
    return "at a colony";
  case Buildable::Station:
    return "where a ship is";
  case Buildable::Colony:
    return "on a station";
  case Buildable::Card:
    return "from the deck";
  }
  return "";
}

} // namespace

const char *nameOf(Buildable what) {
  switch (what) {
  case Buildable::Ship:
    return "Ship";
  case Buildable::Station:
    return "Station";
  case Buildable::Colony:
    return "Colony";
  case Buildable::Card:
    return "Card";
  }
  return "";
}

std::vector<BuildRow> buildRows(const State &given) {
  // While the turn's offers to players are open: the rows as they'll be
  // once they're over (building ends them first: BuildRow::endsTrading)
  const bool trading = given.phase == Phase::Trading;
  State afterTrading = given;
  if (trading)
    Game::apply(afterTrading, Action{Action::Kind::EndTrading});
  const State &state = trading ? afterTrading : given;
  const std::vector<Action> legal = Game::legalActions(state);
  const Resources &held = state.player[static_cast<std::size_t>(state.turn)].resources;
  std::vector<BuildRow> rows;
  for (const Buildable what :
       {Buildable::Ship, Buildable::Station, Buildable::Colony, Buildable::Card}) {
    BuildRow row{.what = what, .cost = costOf(state.settings, what), .endsTrading = trading};
    row.actions = actionsOf(legal, what);
    row.canBuild = !row.actions.empty();
    if (row.canBuild) {
      row.note = whereFor(what);
    } else if (state.phase != Phase::Main) {
      row.note = "not until your roll's done";
    } else {
      // With its cost added to your cards: somewhere to put it, so only the
      // resources are missing; or nowhere, resources or not
      State richer = state;
      Resources &more = richer.player[static_cast<std::size_t>(state.turn)].resources;
      for (std::size_t r = 0; r < more.size(); ++r)
        more[r] += row.cost[r];
      row.note = actionsOf(Game::legalActions(richer), what).empty() ? nowhereFor(state, what)
                                                                     : missingFrom(held, row.cost);
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

} // namespace CosmicHex::Handheld
