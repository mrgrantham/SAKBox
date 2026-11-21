// Contracts.cpp - the contract deck: Charters (secret goals, revealed for
// points once met) and action cards (Enforcer, Cartel, Supply Drop,
// Shipyard Surplus: one a turn, not the turn it was bought). Buying deals
// two; the player keeps one and the other goes back under the deck.
#include "RulesInternal.hpp"

namespace CosmicHex {

using namespace Internal;

namespace {
// The kinds of planet a player has stations on (homes not counted)
std::array<int, kResources> stationKinds(const State &state, int player) {
  std::array<int, kResources> count{};
  for (const Planet &planet : state.planets) {
    if (planet.homeOf < 0 && givesResources(planet.kind) && hasStationOn(state, planet, player))
      ++count[static_cast<std::size_t>(planet.kind)];
  }
  return count;
}
int kindsCovered(const State &state, int player) {
  const auto kinds = stationKinds(state, player);
  return static_cast<int>(std::count_if(kinds.begin(), kinds.end(), [](int n) { return n > 0; }));
}
template <PlanetKind kKind> bool twoOf(const State &state, int player) {
  return stationKinds(state, player)[static_cast<std::size_t>(kKind)] >= 2;
}
} // namespace

bool isCharter(int card) { return card >= 0 && card < static_cast<int>(contracts().size()); }

const std::vector<Contract> &contracts() {
  static const std::vector<Contract> kContracts = {
      {"Stations on 3 kinds of planet", 2,
       [](const State &s, int p) { return kindsCovered(s, p) >= 3; }},
      {"Stations on 2 gas giants", 1, twoOf<PlanetKind::GasGiant>},
      {"Two colonies", 2, [](const State &s, int p) { return colonies(s, p) >= 2; }},
      {"A station on a core planet", 1,
       [](const State &s, int p) { return coreStationsOf(s, p) >= 1; }},
      // (5 badges and one find were scored by about 80% of those who kept
      // them, in 2,400 four-player games: nearly free points)
      {"6 explorer badges earned", 2,
       [](const State &s, int p) {
         return s.player[static_cast<std::size_t>(p)].badgesEarned >= 6;
       }},
      {"Two anomalies kept from the zone", 1,
       [](const State &s, int p) { return s.player[static_cast<std::size_t>(p)].findsKept >= 2; }},
      {"Stations on opposite sides of the board", 3,
       [](const State &s, int p) {
         for (const Planet &a : s.planets) {
           for (const Planet &b : s.planets) {
             if (hasStationOn(s, a, p) && hasStationOn(s, b, p) &&
                 HexGrid::distance(a.hex, b.hex) >= 6)
               return true;
           }
         }
         return false;
       }},
      {"Sharing 2 planets with other players", 2,
       [](const State &s, int p) {
         int shared = 0;
         for (const Planet &planet : s.planets)
           shared += hasStationOn(s, planet, p) && ownerCount(s, planet) >= 2;
         return shared >= 2;
       }},
      {"A station next to the alien station", 1,
       [](const State &s, int p) {
         for (const ZoneSite &site : s.zone) {
           if (!site.flipped || site.find != Find::AlienStation)
             continue;
           if (s.settings.corners) {
             for (const Station &station : s.stations)
               if (station.owner == p && touches(station.at, site.hex))
                 return true;
             continue;
           }
           for (const Planet &planet : s.planets) {
             if (dockedAt(planet, p) && HexGrid::distance(planet.hex, site.hex) <= 1)
               return true;
           }
         }
         return false;
       }},
      {"6 resources in hand", 1,
       [](const State &s, int p) {
         return total(s.player[static_cast<std::size_t>(p)].resources) >= 6;
       }},
      {"A colony on a planet numbered 6 or 8", 2,
       [](const State &s, int p) {
         for (const Planet &planet : s.planets) {
           if (planet.homeOf < 0 && (planet.number == 6 || planet.number == 8) &&
               hasColonyOn(s, planet, p))
             return true;
         }
         return false;
       }},
      {"Stations on 2 ice moons", 1, twoOf<PlanetKind::IceMoon>},
      {"Stations on 2 nebulae", 1, twoOf<PlanetKind::Nebula>},
      {"Three ships in play", 1, [](const State &s, int p) { return shipsOf(s, p) >= 3; }},
      {"Stations on all 5 kinds of planet", 3,
       [](const State &s, int p) { return kindsCovered(s, p) >= 5; }},
      {"Stations on 2 asteroid belts", 1, twoOf<PlanetKind::AsteroidBelt>},
      {"Stations on 2 lush planets", 1, twoOf<PlanetKind::LushPlanet>},
      // The five above merged into one, and 5 kinds made 4
      {"Stations on 2 planets of the same kind", 1,
       [](const State &s, int p) {
         const auto kinds = stationKinds(s, p);
         return std::ranges::any_of(kinds, [](int n) { return n >= 2; });
       }},
      {"Stations on 4 kinds of planet", 3,
       [](const State &s, int p) { return kindsCovered(s, p) >= 4; }},
      // (19) A station or colony by another player's exhausted
      // home world (Settings::exhaustHomesAt)
      {"Occupier: a station on another player's exhausted home world", 2,
       [](const State &s, int p) {
         for (const Planet &planet : s.planets)
           if (planet.formerHomeOf >= 0 && planet.formerHomeOf != p && hasStationOn(s, planet, p))
             return true;
         return false;
       }},
      // (20) A station touching no planet and no outpost, which
      // only a holder of this Charter may build (one). The station scores 1
      // as any; the card nothing
      {"Deep-space relay: a station touching no planet and no alien outpost", 0,
       [](const State &s, int p) {
         return std::any_of(s.stations.begin(), s.stations.end(), [&](const Station &st) {
           return st.owner == p && isRelaySpot(s, st.at);
         });
       }},
      // (21) In place of "3 kinds" (met by 88 to 91% of those who
      // kept it, being what everyone does anyway), its opposite: lean on one
      {"Stations on 3 planets of one kind", 2,
       [](const State &s, int p) {
         const auto kinds = stationKinds(s, p);
         return *std::max_element(kinds.begin(), kinds.end()) >= 3;
       }},
  };
  return kContracts;
}

int charterPoints(const Settings &settings, int id) {
  return id == kDeepSpaceRelay ? settings.relayPoints
                               : contracts()[static_cast<std::size_t>(id)].points;
}

namespace Internal {

int colonies(const State &state, int player) {
  int count = 0;
  if (state.settings.corners) {
    const int home = homeColonyOf(state, player); // (once, not a search a station)
    for (std::size_t i = 0; i < state.stations.size(); ++i)
      count += state.stations[i].owner == player && state.stations[i].colony &&
               static_cast<int>(i) != home;
    return count;
  }
  for (const Planet &planet : state.planets) {
    for (int d = 0; d < kDocks; ++d)
      count += planet.docks[static_cast<std::size_t>(d)] == player &&
               planet.colony[static_cast<std::size_t>(d)];
  }
  return count;
}

// One card a turn, in the main part, and not one bought this turn
bool canPlayCard(const State &state, int card) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const auto held = std::count(me.contracts.begin(), me.contracts.end(), card);
  const auto bought = std::count(me.boughtThisTurn.begin(), me.boughtThisTurn.end(), card);
  return state.phase == Phase::Main && !state.cardPlayed && held > bought;
}

void playCard(State &state, int card) {
  Player &me = state.player[static_cast<std::size_t>(state.turn)];
  me.contracts.erase(std::find(me.contracts.begin(), me.contracts.end(), card));
  state.cardPlayed = true;
}

bool applyBuyContract(State &state, const Action & /*action*/) {
  Player &me = current(state);
  if (state.deck.empty() || !canAfford(me.resources, state.settings.contractCost))
    return false;
  pay(me.resources, state.settings.contractCost);
  state.phase = Phase::ContractDraw;
  return true;
}

// The dealer deals two cards (two the same only if the deck holds two; one
// only when one is left)
bool applyContractDraw(State &state, const Action &action) {
  if (action.kind != Kind::DealContracts)
    return false;
  std::vector<int> deck = state.deck;
  if (!removeOne(deck, action.a))
    return false;
  if (action.b < 0 ? !deck.empty() : !removeOne(deck, action.b))
    return false; // two must be dealt while two are left
  state.deck = std::move(deck);
  state.offered = {action.a, action.b};
  state.phase = Phase::KeepContract;
  return true;
}

// One of the two dealt kept; the other back under the deck
bool applyKeepContract(State &state, const Action &action) {
  if (action.kind != Kind::KeepContract || action.a < 0 ||
      (action.a != state.offered[0] && action.a != state.offered[1]))
    return false;
  Player &me = current(state);
  me.contracts.push_back(action.a);
  if (!isCharter(action.a))
    me.boughtThisTurn.push_back(action.a); // (an action card: playable from next turn)
  const int other = action.a == state.offered[0] ? state.offered[1] : state.offered[0];
  if (other >= 0)
    state.deck.push_back(other);
  state.offered = {-1, -1};
  state.phase = Phase::Main;
  return true;
}

// A Charter, not one revealed already, met now
bool applyRevealContract(State &state, const Action &action) {
  Player &me = current(state);
  const auto held = std::find(me.contracts.begin(), me.contracts.end(), action.a);
  if (held == me.contracts.end() || !isCharter(action.a) ||
      std::find(me.revealed.begin(), me.revealed.end(), action.a) != me.revealed.end() ||
      !contracts()[static_cast<std::size_t>(action.a)].met(state, state.turn))
    return false;
  me.contracts.erase(held);
  me.revealed.push_back(action.a);
  return true;
}

// An Enforcer: the blockade moved, and a steal, as on a 7
bool applyEnforcer(State &state, const Action & /*action*/) {
  if (!canPlayCard(state, kEnforcerCard))
    return false;
  playCard(state, kEnforcerCard);
  ++current(state).enforcersPlayed;
  updateEmbargo(state, state.turn);
  state.phase = Phase::Blockade;
  return true;
}

// A Supply Drop: two resources of the player's choice from the bank
bool applySupplyDrop(State &state, const Action &action) {
  if (!canPlayCard(state, kSupplyDropCard) || action.a < 0 || action.a >= kResources ||
      action.b < 0 || action.b >= kResources)
    return false;
  playCard(state, kSupplyDropCard);
  Player &me = current(state);
  ++me.resources[static_cast<std::size_t>(action.a)];
  ++me.resources[static_cast<std::size_t>(action.b)];
  return true;
}

// A Cartel: one resource taken from every other player (all of it, or up
// to cartelTake each)
bool applyCartel(State &state, const Action &action) {
  if (!canPlayCard(state, kCartelCard) || action.a < 0 || action.a >= kResources)
    return false;
  playCard(state, kCartelCard);
  Player &me = current(state);
  const int take = state.settings.cartelTake;
  const auto r = static_cast<std::size_t>(action.a);
  for (int p = 0; p < state.players; ++p) {
    if (p == state.turn)
      continue;
    int &theirs = state.player[static_cast<std::size_t>(p)].resources[r];
    const int taken = take > 0 ? std::min(theirs, take) : theirs;
    theirs -= taken;
    me.resources[r] += taken;
  }
  return true;
}

void keepContractActions(const State &state, std::vector<Action> &out) {
  for (int id : state.offered) {
    if (id >= 0)
      out.push_back({Kind::KeepContract, id});
  }
}

// Buying a card, and revealing each Charter met (once each)
void contractActions(const State &state, std::vector<Action> &out) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  if (!state.deck.empty() && canAfford(me.resources, state.settings.contractCost))
    out.push_back({Kind::BuyContract});
  for (int id : me.contracts) {
    const bool revealed =
        std::find(me.revealed.begin(), me.revealed.end(), id) != me.revealed.end();
    const bool offered = std::any_of(out.begin(), out.end(), [&](const Action &a) {
      return a.kind == Kind::RevealContract && a.a == id;
    });
    if (isCharter(id) && !revealed && !offered &&
        contracts()[static_cast<std::size_t>(id)].met(state, state.turn))
      out.push_back({Kind::RevealContract, id});
  }
}

// The action cards playable now (Shipyard Surplus is listed with the
// ships: BuildingAndMoving.cpp)
void cardActions(const State &state, std::vector<Action> &out) {
  if (canPlayCard(state, kCartelCard))
    for (int r = 0; r < kResources; ++r)
      out.push_back({Kind::PlayCartel, r});
  if (canPlayCard(state, kSupplyDropCard))
    for (int a = 0; a < kResources; ++a)
      for (int b = a; b < kResources; ++b)
        out.push_back({Kind::PlaySupplyDrop, a, b});
  if (canPlayCard(state, kEnforcerCard))
    out.push_back({Kind::PlayEnforcer});
}

} // namespace Internal

} // namespace CosmicHex
