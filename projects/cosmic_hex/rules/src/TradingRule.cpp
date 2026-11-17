#include <CosmicHex/TradingRule.hpp>

#include <algorithm>
#include <array>
#include <numeric>

namespace CosmicHex {

namespace {

using Kind = Action::Kind;

int total(const Resources &r) { return std::accumulate(r.begin(), r.end(), 0); }

Resources shortOf(const Resources &have, const Resources &cost) {
  Resources out{};
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = std::max(0, cost[i] - have[i]);
  return out;
}

bool anyStationToUpgrade(const State &state, int player) {
  return std::any_of(state.stations.begin(), state.stations.end(),
                     [&](const Station &s) { return s.owner == player && !s.colony; });
}

bool anyShip(const State &state, int player) {
  return std::any_of(state.ships.begin(), state.ships.end(),
                     [&](const Ship &s) { return s.owner == player; });
}

int shipsOwned(const State &state, int player) {
  return static_cast<int>(std::count_if(state.ships.begin(), state.ships.end(),
                                        [&](const Ship &s) { return s.owner == player; }));
}

// Would `player` take `gets` for `gives`? It must cost only what they hold
// beyond their next build, and bring that build closer
bool worthIt(const State &state, int player, const Resources &gets, const Resources &gives) {
  const TradeNeeds needs = tradeNeedsOf(state, player);
  if (!needs.wantsSomething)
    return false;
  int helps = 0;
  for (std::size_t i = 0; i < gets.size(); ++i) {
    if (gives[i] > needs.spare[i])
      return false;
    helps += std::min(gets[i], needs.shortfall[i]);
  }
  return helps > 0;
}

// Whether someone's within two points of winning (no help for them)
bool nearlyWon(const State &state, int player) {
  return points(state, player) >= state.settings.winPoints - 2;
}

bool offeredAlready(const State &state, int give, int get) {
  return std::any_of(
      state.offersMade.begin(), state.offersMade.end(),
      [&](const std::array<int, 2> &made) { return made[0] == give && made[1] == get; });
}

} // namespace

TradeNeeds tradeNeedsOf(const State &state, int player) {
  const Settings &settings = state.settings;
  const Resources &have = state.player[static_cast<std::size_t>(player)].resources;
  // The builds they could use, in order: a colony, a station, a ship, a card
  std::array<std::pair<bool, Resources>, 4> builds = {{
      {anyStationToUpgrade(state, player), settings.colonyCost},
      {anyShip(state, player), settings.stationCost},
      {shipsOwned(state, player) < settings.maxShips, settings.shipCost},
      {!state.deck.empty(), settings.contractCost},
  }};
  TradeNeeds needs;
  for (const auto &[usable, cost] : builds) {
    if (!usable)
      continue;
    const Resources shortfall = shortOf(have, cost);
    if (total(shortfall) == 0)
      return needs; // (they can build something now: no need to trade)
    if (total(shortfall) <= 2) {
      needs.shortfall = shortfall;
      for (std::size_t i = 0; i < have.size(); ++i)
        needs.spare[i] = std::max(0, have[i] - cost[i]);
      needs.wantsSomething = true;
      return needs;
    }
  }
  return needs;
}

Action tradingRuleMove(const State &state) {
  const int me = Game::currentPlayer(state);
  switch (state.phase) {
  case Phase::Trading: {
    const TradeNeeds needs = tradeNeedsOf(state, me);
    if (!needs.wantsSomething)
      return {Kind::EndTrading};
    // What they're shortest of, and what they hold most beyond the build
    const auto most = [](const Resources &r) {
      return static_cast<int>(std::max_element(r.begin(), r.end()) - r.begin());
    };
    const int want = most(needs.shortfall), spare = most(needs.spare);
    for (int giving = 1; giving <= 2; ++giving) {
      if (needs.spare[static_cast<std::size_t>(spare)] < giving)
        break;
      Resources give{}, get{};
      give[static_cast<std::size_t>(spare)] = giving;
      get[static_cast<std::size_t>(want)] = 1;
      const int g = bundleOf(give), w = bundleOf(get);
      if (!offeredAlready(state, g, w))
        return {Kind::OfferTrade, g, w, -1};
    }
    return {Kind::EndTrading};
  }
  case Phase::TradeReply: {
    // They'd get the offerer's give, and give the offerer's get
    const bool take =
        !nearlyWon(state, state.turn) &&
        worthIt(state, me, resourcesOf(state.offer.give), resourcesOf(state.offer.get));
    return {take ? Kind::AcceptTrade : Kind::DeclineTrade};
  }
  case Phase::TradeCounter: {
    // (the offerer, answering a person's counter: they'd get its give)
    const bool take = !nearlyWon(state, state.offer.replier) &&
                      worthIt(state, me, resourcesOf(state.offer.counterGive),
                              resourcesOf(state.offer.counterGet));
    return {take ? Kind::AcceptTrade : Kind::DeclineTrade};
  }
  default:
    return {Kind::EndTrading};
  }
}

} // namespace CosmicHex
