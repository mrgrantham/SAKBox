// Production.cpp - what a turn starts with: the home's income (chosen, or
// rolled on the home die) and the home exhausted at exhaustHomesAt points;
// the roll and the planets it pays; and on a 7 the hand limit, the
// blockade and stealing (an Enforcer's blockade too).
#include "RulesInternal.hpp"

namespace CosmicHex {

using namespace Internal;

namespace {

void produce(State &state, int roll) {
  // (what payouts() says, paid)
  for (const Payout &pay : payouts(state, roll))
    state.player[static_cast<std::size_t>(pay.player)]
        .resources[static_cast<std::size_t>(pay.resource)] += pay.count;
}

// The hand limit (Settings::discardOver): on a 7, each player holding more
// than the limit gives back half, one at a time from whichever resource they
// hold most of (the lowest-numbered of a tie)
void discardOverLimit(State &state) {
  const int limit = state.settings.discardOver;
  if (limit <= 0)
    return;
  for (Player &player : state.player) {
    const int held = total(player.resources);
    if (held <= limit)
      continue;
    for (int give = held / 2; give > 0; --give) {
      const auto most = std::max_element(player.resources.begin(), player.resources.end());
      --*most;
    }
  }
}

// (corners) Which players have stations on each planet: bit p for player
// p, in one pass over the stations (asked planet by planet, player
// by player, the blockade's moves were the bots' largest cost)
std::vector<std::uint8_t> planetOwners(const State &state) {
  std::vector<std::uint8_t> owners(state.planets.size(), 0);
  for (const Station &station : state.stations)
    for (Hex hex : HexGrid::hexesTouching(station.at))
      if (const int planet = planetAt(state, hex); planet >= 0)
        owners[static_cast<std::size_t>(planet)] |= static_cast<std::uint8_t>(1u << station.owner);
  return owners;
}

// Who the blockade on `planet` would steal from: owners of stations on it
// (or docks) with something to steal, but not whoever moves it. `owners`:
// planetOwners, if already worked out
std::vector<int> blockadeVictims(const State &state, int planet,
                                 const std::vector<std::uint8_t> *owners = nullptr) {
  std::vector<int> out;
  if (planet < 0)
    return out;
  if (state.settings.corners) {
    const std::uint8_t on = owners ? (*owners)[static_cast<std::size_t>(planet)]
                                   : planetOwners(state)[static_cast<std::size_t>(planet)];
    for (int owner = 0; owner < state.players; ++owner)
      if (((on >> owner) & 1) != 0 && owner != state.turn &&
          total(state.player[static_cast<std::size_t>(owner)].resources) > 0)
        out.push_back(owner);
    return out;
  }
  for (int d = 0; d < kDocks; ++d) {
    const int owner =
        state.planets[static_cast<std::size_t>(planet)].docks[static_cast<std::size_t>(d)];
    if (owner >= 0 && owner != state.turn &&
        total(state.player[static_cast<std::size_t>(owner)].resources) > 0)
      out.push_back(owner);
  }
  return out;
}

std::vector<int> blockadeTargets(const State &state) {
  std::vector<int> out;
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    if (settleable(state.planets[i]) && static_cast<int>(i) != state.blockade)
      out.push_back(static_cast<int>(i));
  }
  return out;
}

// After the roll's dealt with: trading first, if it's on and not done
void afterTheRoll(State &state) {
  state.phase = state.settings.playerTrading && !state.tradingDone ? Phase::Trading : Phase::Main;
}

// The exhausted home: at exhaustHomesAt points, the resource just taken
// was its last; it becomes a planet of that kind, its number drawn next
void exhaustHomeIfDue(State &state, int resource) {
  const auto home = std::find_if(state.planets.begin(), state.planets.end(),
                                 [&](const Planet &p) { return p.homeOf == state.turn; });
  if (state.settings.exhaustHomesAt > 0 && home != state.planets.end() &&
      points(state, state.turn) >= state.settings.exhaustHomesAt) {
    home->kind = static_cast<PlanetKind>(resource);
    home->formerHomeOf = state.turn;
    home->homeOf = -1;
    updateDominion(state);
    state.homeIncomeLeft = 0;
    state.pendingPlanet = static_cast<int>(home - state.planets.begin());
    state.phase = Phase::HomeNumberDraw;
  }
}

} // namespace

std::vector<Payout> payouts(const State &state, int roll) {
  std::vector<Payout> out;
  for (std::size_t i = 0; i < state.planets.size(); ++i) {
    const Planet &planet = state.planets[i];
    if (!planet.flipped || !givesResources(planet.kind) || planet.number != roll ||
        static_cast<int>(i) == state.blockade)
      continue;
    const auto resource = static_cast<Resource>(planet.kind);
    if (state.settings.corners) {
      // Each planet showing the roll pays every station touching it
      for (const Station &station : state.stations)
        if (touches(station.at, planet.hex))
          out.push_back({static_cast<int>(i), station.owner, resource,
                         station.colony ? state.settings.colonyPays : state.settings.stationPays});
      continue;
    }
    for (int d = 0; d < kDocks; ++d)
      if (const int owner = planet.docks[static_cast<std::size_t>(d)]; owner >= 0)
        out.push_back({static_cast<int>(i), owner, resource,
                       planet.colony[static_cast<std::size_t>(d)] ? state.settings.colonyPays
                                                                  : state.settings.stationPays});
  }
  return out;
}

namespace Internal {

void nextHomeIncome(State &state) {
  state.phase = state.homeIncomeLeft <= 0 ? Phase::RollDraw
                : state.settings.homeDie  ? Phase::HomeDieDraw
                                          : Phase::HomeIncome;
}

int homeIncomeFor(const State &state, int player) {
  const Settings &settings = state.settings;
  // An exhausted home gives nothing of its own (it pays by the dice)
  const bool hasHome = std::any_of(state.planets.begin(), state.planets.end(),
                                   [&](const Planet &p) { return p.homeOf == player; });
  if (!hasHome)
    return 0;
  const int mine = points(state, player);
  if (!settings.homeIncomeSteps.empty()) {
    int income = 1;
    for (const auto &[from, amount] : settings.homeIncomeSteps)
      if (mine >= from)
        income = amount;
    return income;
  }
  int leader = 0;
  for (int p = 0; p < state.players; ++p) {
    if (p != player)
      leader = std::max(leader, points(state, p));
  }
  const bool behind = (settings.catchUpBelow > 0 && mine < settings.catchUpBelow) ||
                      (settings.catchUpGap > 0 && leader - mine >= settings.catchUpGap);
  return behind ? 2 : 1;
}

// ---- Trading between players (Settings::playerTrading)

// The home's resource: chosen, or rolled on the home die (where its sixth
// face, "your choice", hands the choice to the player)
bool applyHomeIncome(State &state, const Action &action) {
  if (state.phase == Phase::HomeDieDraw && action.kind == Kind::RollHomeDie &&
      action.a == kResources) {
    state.phase = Phase::HomeIncome;
    return true;
  }
  const bool taking = state.phase == Phase::HomeIncome ? action.kind == Kind::ChooseHome
                                                       : action.kind == Kind::RollHomeDie;
  if (!taking || action.a < 0 || action.a >= kResources)
    return false;
  ++current(state).resources[static_cast<std::size_t>(action.a)];
  --state.homeIncomeLeft;
  nextHomeIncome(state);
  exhaustHomeIfDue(state, action.a);
  return true;
}

// The exhausted home's number (and, with exhaustedKindRandom, its kind)
bool applyHomeNumber(State &state, const Action &action) {
  const std::vector<int> &numbers = state.settings.exhaustedNumbers;
  if (action.kind != Kind::NumberHome ||
      std::find(numbers.begin(), numbers.end(), action.a) == numbers.end())
    return false;
  Planet &home = state.planets[static_cast<std::size_t>(state.pendingPlanet)];
  if (state.settings.exhaustedKindRandom) {
    if (action.b < 0 || action.b >= kResources)
      return false;
    home.kind = static_cast<PlanetKind>(action.b);
  }
  home.number = action.a;
  state.pendingPlanet = -1;
  state.phase = Phase::RollDraw;
  return true;
}

// The dealer's two dice: a 7 brings the hand limit and the blockade; any
// other number pays the planets showing it
bool applyRoll(State &state, const Action &action) {
  if (action.kind != Kind::Roll || action.a < 1 || action.a > 6 || action.b < 1 || action.b > 6)
    return false;
  state.lastRoll = action.a + action.b;
  if (state.lastRoll == 7) {
    discardOverLimit(state);
    state.phase = Phase::Blockade;
  } else {
    produce(state, state.lastRoll);
    afterTheRoll(state);
  }
  return true;
}

// The blockade moved to a planet (none to move to: -1), and who on it is
// stolen from (nobody: -1)
bool applyBlockade(State &state, const Action &action) {
  if (action.kind != Kind::MoveBlockade)
    return false;
  const std::vector<int> targets = blockadeTargets(state);
  if (targets.empty() ? action.a != -1
                      : std::find(targets.begin(), targets.end(), action.a) == targets.end())
    return false;
  const std::vector<int> victims = blockadeVictims(state, action.a);
  if (victims.empty() ? action.b != -1
                      : std::find(victims.begin(), victims.end(), action.b) == victims.end())
    return false;
  if (action.a >= 0)
    state.blockade = action.a;
  if (action.b >= 0) {
    state.victims = {action.b};
    state.phase = Phase::StealDraw;
  } else {
    afterTheRoll(state); // (on a 7; after an Enforcer, trading's done)
  }
  return true;
}

// The dealer draws a resource from the next victim's hand (the blockade's,
// or a pirate hideout's)
bool applySteal(State &state, const Action &action) {
  if (action.kind != Kind::Steal || state.victims.empty() || action.a < 0 || action.a >= kResources)
    return false;
  Player &victim = state.player[static_cast<std::size_t>(state.victims.front())];
  if (victim.resources[static_cast<std::size_t>(action.a)] <= 0)
    return false;
  --victim.resources[static_cast<std::size_t>(action.a)];
  ++current(state).resources[static_cast<std::size_t>(action.a)];
  state.victims.erase(state.victims.begin());
  if (state.victims.empty())
    afterTheRoll(state);
  return true;
}

void blockadeActions(const State &state, std::vector<Action> &out) {
  const std::vector<int> targets = blockadeTargets(state);
  if (targets.empty())
    out.push_back({Kind::MoveBlockade, -1, -1});
  const std::vector<std::uint8_t> owners =
      state.settings.corners ? planetOwners(state) : std::vector<std::uint8_t>{};
  for (int planet : targets) {
    const std::vector<int> victims =
        blockadeVictims(state, planet, state.settings.corners ? &owners : nullptr);
    if (victims.empty())
      out.push_back({Kind::MoveBlockade, planet, -1});
    for (int victim : victims)
      out.push_back({Kind::MoveBlockade, planet, victim});
  }
}

} // namespace Internal

} // namespace CosmicHex
