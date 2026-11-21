// Scoring.cpp - points (stations, colonies, titles, badges, revealed
// Charters), the titles that move between players (Core Control, the
// Galactic Embargo, Imperial Dominion), and how the game ends: at once on
// reaching winPoints, or with the round played out (Settings::finishRound).
#include "RulesInternal.hpp"

namespace CosmicHex {

using namespace Internal;

namespace {

// Imperial Dominion's count: colonies on other players' former homes
int coloniesOnFormerHomes(const State &state, int player) {
  int count = 0;
  for (const Station &station : state.stations) {
    if (station.owner != player || !station.colony)
      continue;
    count += std::any_of(state.planets.begin(), state.planets.end(), [&](const Planet &planet) {
      return planet.formerHomeOf >= 0 && planet.formerHomeOf != player &&
             touches(station.at, planet.hex);
    });
  }
  return count;
}

} // namespace

int stationsOf(const State &state, int player) {
  int count = 0;
  if (state.settings.corners) {
    for (const Station &station : state.stations)
      count += station.owner == player;
    return count;
  }
  for (const Planet &planet : state.planets)
    count += dockedAt(planet, player);
  return count;
}

[[gnu::hot]] int points(const State &state, int player) {
  const Player &p = state.player[static_cast<std::size_t>(player)];
  int total = 0;
  if (state.settings.corners) {
    // A station 1, a colony 2, the home colony too: one pass
    for (const Station &station : state.stations)
      if (station.owner == player)
        total += station.colony ? 2 : 1;
  } else {
    total = stationsOf(state, player) + colonies(state, player); // a colony is 2 in all
  }
  if (state.coreHolder == player)
    total += state.settings.coreControlPoints;
  if (state.embargoHolder == player)
    total += state.settings.embargoPoints;
  if (state.dominionHolder == player)
    total += state.settings.dominionPoints;
  total += p.badgePoints;
  for (int id : p.revealed)
    total += charterPoints(state.settings, id);
  return total;
}

double Game::estimate(const State &state, int player) {
  int best = 0;
  for (int p = 0; p < state.players; ++p) {
    if (p != player)
      best = std::max(best, points(state, p));
  }
  const double lead = points(state, player) - best;
  return std::clamp(0.5 + lead / (2.0 * state.settings.winPoints), 0.0, 1.0);
}

namespace Internal {

// Core Control: to whoever has the most stations on core planets, at least
// coreStationsForControl; a tie keeps it with its holder
void updateCoreControl(State &state, int player) {
  if (state.settings.coreControlPoints <= 0)
    return; // (no Core Control in these rules: nobody holds it)
  const int mine = coreStationsOf(state, player);
  if (mine < state.settings.coreStationsForControl || state.coreHolder == player)
    return;
  if (state.coreHolder < 0 || mine > coreStationsOf(state, state.coreHolder))
    state.coreHolder = player;
}

// The Galactic Embargo: to whoever has played the most Enforcers, at least
// embargoEnforcers; a tie keeps it with its holder
void updateEmbargo(State &state, int player) {
  const auto played = [&](int p) {
    return state.player[static_cast<std::size_t>(p)].enforcersPlayed;
  };
  if (played(player) < state.settings.embargoEnforcers || state.embargoHolder == player)
    return;
  if (state.embargoHolder < 0 || played(player) > played(state.embargoHolder))
    state.embargoHolder = player;
}

// Imperial Dominion: to whoever has the most colonies on other players'
// former homes, at least dominionColonies; a tie keeps it with its holder.
// (After a colony is built or a home is exhausted: the only times it
// changes)
void updateDominion(State &state) {
  if (state.settings.dominionPoints <= 0)
    return;
  for (int p = 0; p < state.players; ++p) {
    if (p == state.dominionHolder)
      continue;
    const int mine = coloniesOnFormerHomes(state, p);
    if (mine >= state.settings.dominionColonies &&
        (state.dominionHolder < 0 || mine > coloniesOnFormerHomes(state, state.dominionHolder)))
      state.dominionHolder = p;
  }
}

// The most points wins; a tie, the most stations and colonies on the
// board; still tied, a shared win
void declareWinners(State &state) {
  const auto standing = [&](int p) { return std::pair{points(state, p), stationsOf(state, p)}; };
  std::pair<int, int> best{-1, -1};
  for (int p = 0; p < state.players; ++p)
    best = std::max(best, standing(p));
  for (int step = 0; step < state.players; ++step) {
    const int p = (state.first + step) % state.players;
    if (standing(p) == best)
      state.winners.push_back(p);
  }
  state.winner = state.winners.front();
  state.phase = Phase::Over;
}

// (Not while the home's income is being taken: the turn's yet to start)
void checkForWin(State &state) {
  if (state.phase == Phase::HomeIncome || state.phase == Phase::HomeDieDraw ||
      points(state, state.turn) < state.settings.winPoints)
    return;
  if (state.settings.finishRound) {
    state.finalRound = true; // (the game ends when the round does)
  } else {
    state.winner = state.turn;
    state.phase = Phase::Over;
  }
}

} // namespace Internal

} // namespace CosmicHex
