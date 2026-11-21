// Trading.cpp - trading: with the bank (or, at a better rate, an alien
// outpost a station or ship is by), and between players (offers to one
// player or to anyone, answered yes, no or with a counter-offer:
// Settings::playerTrading). Offers carry their resources as bundles
// (bundleOf).
#include "RulesInternal.hpp"

namespace CosmicHex {

using namespace Internal;

namespace {

// (corners) The best alien rate the player has: alienStationRate with a
// station by an outpost, alienRate with a ship, or 0 with neither
int alienRateOf(const State &state, int player) {
  // (no turned-over outpost, usually: nothing to look for)
  const bool anyOutpost =
      std::any_of(state.zone.begin(), state.zone.end(), [](const ZoneSite &site) {
        return site.flipped && site.find == Find::AlienStation;
      });
  if (!anyOutpost)
    return 0;
  int best = 0;
  for (const Station &station : state.stations)
    if (station.owner == player && byAnOutpost(state, station.at))
      best = state.settings.alienStationRate;
  if (best == 0)
    for (const Ship &ship : state.ships)
      if (ship.owner == player && byAnOutpost(state, ship.corner))
        best = state.settings.alienRate;
  return best;
}

// Whether the zone's alien station is next to one of the player's stations
// or ships, so they can trade there
bool alienStationNear(const State &state, int player) {
  for (const ZoneSite &site : state.zone) {
    if (!site.flipped || site.find != Find::AlienStation)
      continue;
    if (state.settings.corners) {
      // a station or ship on one of its corners
      for (const Station &station : state.stations)
        if (station.owner == player && touches(station.at, site.hex))
          return true;
      for (const Ship &ship : state.ships)
        if (ship.owner == player && touches(ship.corner, site.hex))
          return true;
      continue;
    }
    for (const Planet &planet : state.planets) {
      if (dockedAt(planet, player) && HexGrid::distance(planet.hex, site.hex) <= 1)
        return true;
    }
    for (const Ship &ship : state.ships) {
      if (ship.owner == player && HexGrid::distance(ship.at, site.hex) <= 1)
        return true;
    }
  }
  return false;
}

bool holds(const Resources &have, const Resources &needed) {
  for (std::size_t i = 0; i < have.size(); ++i)
    if (have[i] < needed[i])
      return false;
  return true;
}

bool anything(const Resources &resources) {
  return std::any_of(resources.begin(), resources.end(), [](int n) { return n > 0; });
}

// `from` gives `give` to `to`, and gets `get` back
void swapResources(State &state, int from, int to, const Resources &give, const Resources &get) {
  Resources &a = state.player[static_cast<std::size_t>(from)].resources;
  Resources &b = state.player[static_cast<std::size_t>(to)].resources;
  for (std::size_t i = 0; i < a.size(); ++i) {
    a[i] += get[i] - give[i];
    b[i] += give[i] - get[i];
  }
}

// The next player to answer an offer to anyone after `after`, or -1
int nextReplier(const State &state, int after) {
  if (state.offer.to >= 0)
    return -1;
  for (int step = 1; step < state.players; ++step) {
    const int p = (after + step) % state.players;
    if (p == state.turn)
      return -1; // (round to the offerer: everyone's answered)
    return p;
  }
  return -1;
}

// The offer's answered (taken or not): back to trading
void offerDone(State &state) {
  state.offer = TradeOffer{};
  state.phase = Phase::Trading;
}

} // namespace

int bundleOf(const Resources &resources) {
  int bundle = 0;
  for (std::size_t i = 0; i < resources.size(); ++i)
    bundle |= std::clamp(resources[i], 0, 15) << (4 * i);
  return bundle;
}

Resources resourcesOf(int bundle) {
  Resources out{};
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = (bundle >> (4 * i)) & 15;
  return out;
}

int tradeRateOf(const State &state, int player) { return tradeRate(state, player); }

namespace Internal {

int tradeRate(const State &state, int player) {
  if (state.settings.corners) {
    const int alien = alienRateOf(state, player);
    return alien > 0 ? std::min(alien, state.settings.bankRate) : state.settings.bankRate;
  }
  return alienStationNear(state, player)
             ? std::min(state.settings.alienRate, state.settings.bankRate)
             : state.settings.bankRate;
}

// The player whose turn it is makes an offer, or ends trading for the turn
bool applyTrading(State &state, const Action &action) {
  if (action.kind == Kind::EndTrading) {
    state.tradingDone = true;
    state.phase = Phase::Main;
    return true;
  }
  if (action.kind != Kind::OfferTrade)
    return false;
  const Resources give = resourcesOf(action.a), get = resourcesOf(action.b);
  const bool toSomeone = action.c >= 0 && action.c < state.players && action.c != state.turn;
  if (!anything(give) || !anything(get) || (action.c != -1 && !toSomeone) ||
      !holds(state.player[static_cast<std::size_t>(state.turn)].resources, give))
    return false;
  state.offer = TradeOffer{action.a, action.b, action.c, -1, 0, 0};
  state.offer.replier = toSomeone ? action.c : (state.turn + 1) % state.players;
  state.offersMade.push_back({action.a, action.b});
  state.phase = Phase::TradeReply;
  return true;
}

// The player offered to takes the offer, declines it (on to the next, if
// it was to anyone), or makes a counter-offer
bool applyTradeReply(State &state, const Action &action) {
  const int replier = state.offer.replier;
  const Resources give = resourcesOf(state.offer.give), get = resourcesOf(state.offer.get);
  if (action.kind == Kind::AcceptTrade) {
    if (!holds(state.player[static_cast<std::size_t>(replier)].resources, get))
      return false;
    swapResources(state, state.turn, replier, give, get);
    offerDone(state);
    return true;
  }
  if (action.kind == Kind::DeclineTrade) {
    const int next = nextReplier(state, replier);
    if (next < 0)
      offerDone(state);
    else
      state.offer.replier = next;
    return true;
  }
  if (action.kind == Kind::CounterTrade) {
    const Resources theyGive = resourcesOf(action.a), theyGet = resourcesOf(action.b);
    if (!anything(theyGive) || !anything(theyGet) ||
        !holds(state.player[static_cast<std::size_t>(replier)].resources, theyGive))
      return false;
    state.offer.counterGive = action.a;
    state.offer.counterGet = action.b;
    state.phase = Phase::TradeCounter;
    return true;
  }
  return false;
}

// The offerer takes the counter-offer, or declines it (on to the next
// answerer, as a declined offer)
bool applyTradeCounter(State &state, const Action &action) {
  const int replier = state.offer.replier;
  if (action.kind == Kind::AcceptTrade) {
    const Resources theyGive = resourcesOf(state.offer.counterGive),
                    theyGet = resourcesOf(state.offer.counterGet);
    if (!holds(state.player[static_cast<std::size_t>(state.turn)].resources, theyGet))
      return false;
    swapResources(state, state.turn, replier, theyGet, theyGive);
    offerDone(state);
    return true;
  }
  if (action.kind == Kind::DeclineTrade) {
    state.offer.counterGive = state.offer.counterGet = 0;
    const int next = nextReplier(state, replier);
    if (next < 0) {
      offerDone(state);
    } else {
      state.offer.replier = next;
      state.phase = Phase::TradeReply;
    }
    return true;
  }
  return false;
}

// With the bank (or an outpost): the trade rate of one resource for one other
bool applyBankTrade(State &state, const Action &action) {
  if (action.a < 0 || action.a >= kResources || action.b < 0 || action.b >= kResources ||
      action.a == action.b)
    return false;
  Player &me = current(state);
  const int rate = tradeRate(state, state.turn);
  if (me.resources[static_cast<std::size_t>(action.a)] < rate)
    return false;
  me.resources[static_cast<std::size_t>(action.a)] -= rate;
  ++me.resources[static_cast<std::size_t>(action.b)];
  return true;
}

// Trading between players: the search sees only ending it and answering yes
// or no (the offers and counters, too many to search, come from the trading
// rule: TradingRule.hpp)
void tradingActions(const State &state, std::vector<Action> &out) {
  switch (state.phase) {
  case Phase::Trading:
    out.push_back({Kind::EndTrading});
    break;
  case Phase::TradeReply:
    if (holds(state.player[static_cast<std::size_t>(state.offer.replier)].resources,
              resourcesOf(state.offer.get)))
      out.push_back({Kind::AcceptTrade});
    out.push_back({Kind::DeclineTrade});
    break;
  case Phase::TradeCounter:
    if (holds(state.player[static_cast<std::size_t>(state.turn)].resources,
              resourcesOf(state.offer.counterGet)))
      out.push_back({Kind::AcceptTrade});
    out.push_back({Kind::DeclineTrade});
    break;
  default:
    break;
  }
}

void bankTradeActions(const State &state, std::vector<Action> &out) {
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  const int rate = tradeRate(state, state.turn);
  for (int give = 0; give < kResources; ++give) {
    if (me.resources[static_cast<std::size_t>(give)] < rate)
      continue;
    for (int get = 0; get < kResources; ++get) {
      if (get != give)
        out.push_back({Kind::Trade, give, get});
    }
  }
}

} // namespace Internal

} // namespace CosmicHex
