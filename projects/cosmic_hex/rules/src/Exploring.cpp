// Exploring.cpp - turning tiles over: the dealer draws a planet's kind and
// number (the red-number rule keeps 6s and 8s apart) or a zone tile's find;
// what each find gives (salvage, ruins' badges, a pirate hideout's
// stealing); explorer badges earned, and cashed for points.
#include "RulesInternal.hpp"

namespace CosmicHex {

using namespace Internal;

namespace {

void earnBadges(State &state, int count) {
  current(state).badges += count;
  current(state).badgesEarned += count;
}

// What a find gives the player who turned it over: salvage to take, ruins'
// badges, or a steal from each other player with something to steal
void collectFind(State &state, Find find) {
  Player &me = current(state);
  switch (find) {
  case Find::Salvage:
    ++me.findsKept;
    state.salvageLeft = state.settings.salvageResources;
    state.phase = Phase::Salvage;
    break;
  case Find::Ruins:
    ++me.findsKept;
    earnBadges(state, state.settings.ruinsBadges);
    break;
  case Find::PirateHideout:
    ++me.findsKept;
    for (int i = 1; i < state.players; ++i) {
      const int other = (state.turn + i) % state.players;
      if (total(state.player[static_cast<std::size_t>(other)].resources) > 0)
        state.victims.push_back(other);
    }
    if (!state.victims.empty())
      state.phase = Phase::StealDraw;
    break;
  default:
    break;
  }
}

} // namespace

namespace Internal {

void turnNextOver(State &state) {
  while (!state.turning.empty()) {
    const Hex hex = state.turning.front();
    state.turning.erase(state.turning.begin());
    const int planet = planetAt(state, hex);
    if (planet >= 0 && !state.planets[static_cast<std::size_t>(planet)].flipped) {
      state.pendingPlanet = planet;
      state.phase = Phase::PlanetDraw;
      return;
    }
    const int site = siteAt(state, hex);
    if (site >= 0 && !state.zone[static_cast<std::size_t>(site)].flipped) {
      state.pendingSite = site;
      state.phase = Phase::ZoneDraw;
      return;
    }
  }
}

bool isRed(int number) { return number == 6 || number == 8; }

bool redNumberNextTo(const State &state, int planet) {
  const Hex hex = state.planets[static_cast<std::size_t>(planet)].hex;
  for (const Planet &other : state.planets)
    if (other.flipped && HexGrid::distance(other.hex, hex) == 1 &&
        (other.number == 6 || other.number == 8))
      return true;
  return false;
}

bool pileHasOtherThanRed(const State &state) {
  return std::any_of(state.numbersLeft.begin(), state.numbersLeft.end(),
                     [](int n) { return !isRed(n); });
}

// The dealer's draw for the planet a ship turned over: a moon (no number),
// or a kind and a number from their piles
bool applyPlanetDraw(State &state, const Action &action) {
  if (action.kind != Kind::RevealPlanet)
    return false;
  if (action.a == static_cast<int>(PlanetKind::Moon)) {
    if (state.moonsLeft <= 0 || action.b != 0)
      return false;
    --state.moonsLeft;
  } else {
    // (the red-number rule: no 6 or 8 by a face-up 6 or 8, while there's another)
    if (state.settings.redNumbersApart && isRed(action.b) &&
        redNumberNextTo(state, state.pendingPlanet) && pileHasOtherThanRed(state))
      return false;
    if (action.a < 0 || action.a >= kResources ||
        state.kindsLeft[static_cast<std::size_t>(action.a)] <= 0 ||
        !removeOne(state.numbersLeft, action.b))
      return false;
    --state.kindsLeft[static_cast<std::size_t>(action.a)];
  }
  Planet &planet = state.planets[static_cast<std::size_t>(state.pendingPlanet)];
  planet.flipped = true;
  planet.kind = static_cast<PlanetKind>(action.a);
  planet.number = action.b;
  state.pendingPlanet = -1;
  earnBadges(state, 1);
  state.phase = Phase::Main;
  return true;
}

// The dealer's draw for the zone tile a ship turned over, and what it gives
bool applyZoneDraw(State &state, const Action &action) {
  if (action.kind != Kind::RevealFind || action.a < 0 || action.a >= kFinds ||
      state.findsLeft[static_cast<std::size_t>(action.a)] <= 0)
    return false;
  --state.findsLeft[static_cast<std::size_t>(action.a)];
  ZoneSite &site = state.zone[static_cast<std::size_t>(state.pendingSite)];
  site.flipped = true;
  site.find = static_cast<Find>(action.a);
  state.pendingSite = -1;
  earnBadges(state, 1);
  state.phase = Phase::Main;
  collectFind(state, site.find);
  return true;
}

// Salvage: a resource of the player's choice, salvageResources times
bool applySalvage(State &state, const Action &action) {
  if (action.kind != Kind::TakeSalvage || action.a < 0 || action.a >= kResources)
    return false;
  ++current(state).resources[static_cast<std::size_t>(action.a)];
  if (--state.salvageLeft == 0)
    state.phase = Phase::Main;
  return true;
}

bool applyCashBadges(State &state, const Action & /*action*/) {
  Player &me = current(state);
  if (me.badges < state.settings.badgesPerPoint)
    return false;
  me.badges -= state.settings.badgesPerPoint;
  ++me.badgePoints;
  return true;
}

void cashBadgesAtOnce(State &state) {
  if (!state.settings.autoCashBadges)
    return;
  for (Player &player : state.player)
    while (player.badges >= state.settings.badgesPerPoint) {
      player.badges -= state.settings.badgesPerPoint;
      ++player.badgePoints;
    }
}

void cashBadgesActions(const State &state, std::vector<Action> &out) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  if (!state.settings.autoCashBadges && me.badges >= state.settings.badgesPerPoint)
    out.push_back({Kind::CashBadges});
}

} // namespace Internal

} // namespace CosmicHex
