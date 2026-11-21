// Text.cpp - the text the action log keeps: a starting state (its player
// count, settings and board, which readState rebuilds through newGame),
// and each action as a line ("move 2 1,-3"), written and read back.
#include "RulesInternal.hpp"

#include <charconv>
#include <sstream>

namespace CosmicHex {

using namespace Internal;

namespace {

constexpr std::array<std::string_view, kResources> kResourceNames = {"minerals", "food", "fuel",
                                                                     "water", "crystals"};
constexpr std::array<std::string_view, 7> kKindNames = {
    "asteroid_belt", "lush_planet", "gas_giant", "ice_moon", "nebula", "home", "moon"};
constexpr std::array<std::string_view, kFinds> kFindNames = {"nothing", "salvage", "ruins",
                                                             "alien_station", "pirate_hideout"};
// A phase's name in the state's text (a switch, so a new phase without a
// name is a compiler warning rather than a read past a table's end)
std::string_view phaseName(Phase phase) {
  switch (phase) {
  case Phase::HomeIncome:
    return "home_income";
  case Phase::HomeDieDraw:
    return "home_die_draw";
  case Phase::RollDraw:
    return "roll_draw";
  case Phase::Blockade:
    return "blockade";
  case Phase::StealDraw:
    return "steal_draw";
  case Phase::Main:
    return "main";
  case Phase::PlanetDraw:
    return "planet_draw";
  case Phase::ZoneDraw:
    return "zone_draw";
  case Phase::Salvage:
    return "salvage";
  case Phase::ContractDraw:
    return "contract_draw";
  case Phase::KeepContract:
    return "keep";
  case Phase::Over:
    return "over";
  case Phase::PlaceColony:
    return "place_colony";
  case Phase::PlaceShip:
    return "place_ship";
  case Phase::HomeNumberDraw:
    return "home_number_draw";
  case Phase::Trading:
    return "trading";
  case Phase::TradeReply:
    return "trade_reply";
  case Phase::TradeCounter:
    return "trade_counter";
  case Phase::StartCardsDraw:
    return "start_cards_draw";
  }
  return "?";
}

template <std::size_t N>
int indexOf(const std::array<std::string_view, N> &names, std::string_view name) {
  for (std::size_t i = 0; i < N; ++i) {
    if (names[i] == name)
      return static_cast<int>(i);
  }
  return -1;
}

std::string hexText(Hex hex) { return std::to_string(hex.q) + "," + std::to_string(hex.r); }

} // namespace

namespace Internal {

std::optional<int> toInt(std::string_view text) {
  int value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    return std::nullopt;
  return value;
}

std::vector<std::string_view> split(std::string_view text, char by) {
  std::vector<std::string_view> out;
  while (true) {
    const std::size_t at = text.find(by);
    out.push_back(text.substr(0, at));
    if (at == std::string_view::npos)
      return out;
    text.remove_prefix(at + 1);
  }
}

std::optional<std::vector<int>> intList(std::string_view text) {
  std::vector<int> out;
  if (text == "-")
    return out;
  for (std::string_view part : split(text, ',')) {
    const auto value = toInt(part);
    if (!value)
      return std::nullopt;
    out.push_back(*value);
  }
  return out;
}

} // namespace Internal

std::string Game::writeState(const State &state) {
  std::ostringstream out;
  out << "players " << state.players << " turn " << state.turn << " round " << state.round
      << " phase " << phaseName(state.phase) << "\n";
  out << settingsText(state.settings) << "\n";
  for (const Planet &p : state.planets) {
    out << "planet " << hexText(p.hex) << " "
        << (p.flipped ? kKindNames[static_cast<std::size_t>(p.kind)] : "?") << " " << p.number
        << (p.core ? " core" : "") << " home=" << p.homeOf << " former=" << p.formerHomeOf
        << " docks=" << p.docks[0] << "," << p.docks[1] << " colony=" << p.colony[0] << ","
        << p.colony[1] << "\n";
  }
  for (const ZoneSite &z : state.zone)
    out << "zone " << hexText(z.hex) << " "
        << (z.flipped ? kFindNames[static_cast<std::size_t>(z.find)] : "?") << "\n";
  for (const Ship &s : state.ships)
    out << "ship " << s.owner << " "
        << (state.settings.corners ? "corner " + std::to_string(cornerNumber(s.corner))
                                   : hexText(s.at))
        << (s.moved ? " moved" : "") << "\n";
  for (const Station &s : state.stations)
    out << "station " << s.owner << " corner " << cornerNumber(s.at) << (s.colony ? " colony" : "")
        << "\n";
  for (int i = 0; i < state.players; ++i) {
    const Player &p = state.player[static_cast<std::size_t>(i)];
    out << "player " << i << " resources=" << joined(p.resources) << " badges=" << p.badges << ","
        << p.badgesEarned << "," << p.badgePoints << " finds=" << p.findsKept
        << " contracts=" << joined(p.contracts) << " revealed=" << joined(p.revealed)
        << " enforcers=" << p.enforcersPlayed << " new=" << joined(p.boughtThisTurn) << "\n";
  }
  out << "left kinds=" << joined(state.kindsLeft) << " moons=" << state.moonsLeft
      << " numbers=" << joined(state.numbersLeft) << " finds=" << joined(state.findsLeft)
      << " deck=" << joined(state.deck) << "\n";
  out << "blockade=" << state.blockade << " core=" << state.coreHolder
      << " embargo=" << state.embargoHolder << "," << (state.cardPlayed ? 1 : 0)
      << " dominion=" << state.dominionHolder << " winner=" << state.winner
      << " winners=" << joined(state.winners) << " roll=" << state.lastRoll
      << " pending=" << state.pendingPlanet << "," << state.pendingSite
      << " victims=" << joined(state.victims) << " salvage=" << state.salvageLeft
      << " income=" << state.homeIncomeLeft << " offered=" << state.offered[0] << ","
      << state.offered[1] << " first=" << state.first << " setup=" << state.setupStep
      << (state.placingShip ? " placing" : "")
      << (state.startCardsLeft > 0 ? " dealing=" + std::to_string(state.startCardsLeft) : "")
      << "\n";
  out << "final=" << (state.finalRound ? 1 : 0) << " trading=" << (state.tradingDone ? 1 : 0)
      << " offer=" << state.offer.give << "," << state.offer.get << "," << state.offer.to << ","
      << state.offer.replier << "," << state.offer.counterGive << "," << state.offer.counterGet
      << " made=";
  for (const auto &made : state.offersMade)
    out << made[0] << ":" << made[1] << ";";
  out << "\n";
  return out.str();
}

std::optional<State> Game::readState(std::string_view text) {
  // A starting state is newGame's, so it's rebuilt from its player count and
  // settings, and must then write out exactly as the text says
  const std::size_t firstEnd = text.find('\n');
  if (firstEnd == std::string_view::npos)
    return std::nullopt;
  const std::vector<std::string_view> first = split(text.substr(0, firstEnd), ' ');
  if (first.size() < 2 || first[0] != "players")
    return std::nullopt;
  const auto players = toInt(first[1]);
  const auto turn =
      first.size() >= 4 && first[2] == "turn" ? toInt(first[3]) : std::optional<int>(0);
  const std::size_t secondEnd = text.find('\n', firstEnd + 1);
  if (!players || *players < 2 || *players > kMaxPlayers || !turn || *turn < 0 ||
      *turn >= *players || secondEnd == std::string_view::npos)
    return std::nullopt;
  const auto settings = readSettings(text.substr(firstEnd + 1, secondEnd - firstEnd - 1));
  if (!settings)
    return std::nullopt;
  State state = newGame(*players, *settings, *turn);
  if (writeState(state) != text)
    return std::nullopt;
  return state;
}

std::string Game::writeAction(const Action &action) {
  using Kind = Action::Kind;
  const auto res = [](int r) { return std::string(kResourceNames[static_cast<std::size_t>(r)]); };
  const auto n = [](int v) { return std::to_string(v); };
  switch (action.kind) {
  case Kind::ChooseHome:
    return "home " + res(action.a);
  case Kind::Roll:
    return "roll " + n(action.a) + " " + n(action.b);
  case Kind::MoveBlockade:
    return "blockade " + n(action.a) + " " + n(action.b);
  case Kind::Steal:
    return "steal " + res(action.a);
  case Kind::Trade:
    return "trade " + res(action.a) + " " + res(action.b);
  case Kind::BuildShip:
    return "ship " + n(action.a);
  case Kind::Move:
    return "move " + n(action.a) + " " + n(action.b) + "," + n(action.c);
  case Kind::RevealPlanet:
    return "planet " + std::string(kKindNames[static_cast<std::size_t>(action.a)]) + " " +
           n(action.b);
  case Kind::RevealFind:
    return "find " + std::string(kFindNames[static_cast<std::size_t>(action.a)]);
  case Kind::TakeSalvage:
    return "salvage " + res(action.a);
  case Kind::Settle:
    return "settle " + n(action.a);
  case Kind::BuildColony:
    return "colony " + n(action.a);
  case Kind::BuyContract:
    return "buy";
  case Kind::DealContracts:
    return "deal " + n(action.a) + " " + n(action.b);
  case Kind::KeepContract:
    return "keep " + n(action.a);
  case Kind::RevealContract:
    return "reveal " + n(action.a);
  case Kind::PlayEnforcer:
    return "enforcer";
  case Kind::PlayShipyard:
    return "shipyard " + n(action.a);
  case Kind::RollHomeDie:
    return "home_die " + n(action.a);
  case Kind::DealStartCard:
    return "start_card " + res(action.a);
  case Kind::PlaySupplyDrop:
    return "supply " + res(action.a) + " " + res(action.b);
  case Kind::PlayCartel:
    return "cartel " + res(action.a);
  case Kind::CashBadges:
    return "badges";
  case Kind::EndTurn:
    return "end";
  case Kind::PlaceColony:
    return "place_colony " + n(action.a);
  case Kind::PlaceShip:
    return "place_ship " + n(action.a);
  case Kind::OfferTrade:
    return "offer " + n(action.a) + " " + n(action.b) + " " + n(action.c);
  case Kind::AcceptTrade:
    return "accept";
  case Kind::DeclineTrade:
    return "decline";
  case Kind::CounterTrade:
    return "counter " + n(action.a) + " " + n(action.b);
  case Kind::EndTrading:
    return "end_trading";
  case Kind::NumberHome:
    return "number_home " + n(action.a) + " " + n(action.b);
  }
  return "?";
}

std::optional<Action> Game::readAction(std::string_view text) {
  using Kind = Action::Kind;
  const std::vector<std::string_view> w = split(text, ' ');
  const auto res = [](std::string_view name) { return indexOf(kResourceNames, name); };
  const auto num = [](std::string_view v) { return toInt(v); };
  const auto make = [](Kind kind, int a = 0, int b = 0, int c = 0) {
    return std::optional<Action>(Action{kind, a, b, c});
  };
  const std::string_view verb = w[0];
  if (w.size() == 1) {
    if (verb == "buy")
      return make(Kind::BuyContract);
    if (verb == "badges")
      return make(Kind::CashBadges);
    if (verb == "enforcer")
      return make(Kind::PlayEnforcer);
    if (verb == "end")
      return make(Kind::EndTurn);
    if (verb == "accept")
      return make(Kind::AcceptTrade);
    if (verb == "decline")
      return make(Kind::DeclineTrade);
    if (verb == "end_trading")
      return make(Kind::EndTrading);
    return std::nullopt;
  }
  if (w.size() == 2) {
    const int r = res(w[1]);
    if (verb == "home" && r >= 0)
      return make(Kind::ChooseHome, r);
    if (verb == "steal" && r >= 0)
      return make(Kind::Steal, r);
    if (verb == "salvage" && r >= 0)
      return make(Kind::TakeSalvage, r);
    if (verb == "cartel" && r >= 0)
      return make(Kind::PlayCartel, r);
    if (verb == "start_card" && r >= 0)
      return make(Kind::DealStartCard, r);
    if (verb == "find") {
      const int f = indexOf(kFindNames, w[1]);
      return f >= 0 ? make(Kind::RevealFind, f) : std::nullopt;
    }
    const auto v = num(w[1]);
    if (!v)
      return std::nullopt;
    if (verb == "ship")
      return make(Kind::BuildShip, *v);
    if (verb == "shipyard")
      return make(Kind::PlayShipyard, *v);
    if (verb == "home_die")
      return make(Kind::RollHomeDie, *v);
    if (verb == "place_colony")
      return make(Kind::PlaceColony, *v);
    if (verb == "place_ship")
      return make(Kind::PlaceShip, *v);
    if (verb == "settle")
      return make(Kind::Settle, *v);
    if (verb == "colony")
      return make(Kind::BuildColony, *v);
    if (verb == "keep")
      return make(Kind::KeepContract, *v);
    if (verb == "reveal")
      return make(Kind::RevealContract, *v);
    return std::nullopt;
  }
  if (w.size() == 4 && verb == "offer") {
    const auto give = num(w[1]), get = num(w[2]), to = num(w[3]);
    return give && get && to ? make(Kind::OfferTrade, *give, *get, *to) : std::nullopt;
  }
  if (w.size() == 3) {
    if (verb == "supply") {
      const int a = res(w[1]), b = res(w[2]);
      return a >= 0 && b >= 0 ? make(Kind::PlaySupplyDrop, a, b) : std::nullopt;
    }
    if (verb == "trade") {
      const int give = res(w[1]), get = res(w[2]);
      return give >= 0 && get >= 0 ? make(Kind::Trade, give, get) : std::nullopt;
    }
    if (verb == "planet") {
      const int kind = indexOf(kKindNames, w[1]);
      const auto number = num(w[2]);
      const bool known =
          (kind >= 0 && kind < kResources) || kind == static_cast<int>(PlanetKind::Moon);
      return known && number ? make(Kind::RevealPlanet, kind, *number) : std::nullopt;
    }
    if (verb == "move") {
      const auto ship = num(w[1]);
      const std::vector<std::string_view> qr = split(w[2], ',');
      if (!ship || qr.size() != 2)
        return std::nullopt;
      const auto q = num(qr[0]), r = num(qr[1]);
      return q && r ? make(Kind::Move, *ship, *q, *r) : std::nullopt;
    }
    const auto a = num(w[1]), b = num(w[2]);
    if (!a || !b)
      return std::nullopt;
    if (verb == "roll")
      return make(Kind::Roll, *a, *b);
    if (verb == "number_home")
      return make(Kind::NumberHome, *a, *b);
    if (verb == "counter")
      return make(Kind::CounterTrade, *a, *b);
    if (verb == "blockade")
      return make(Kind::MoveBlockade, *a, *b);
    if (verb == "deal")
      return make(Kind::DealContracts, *a, *b);
  }
  return std::nullopt;
}

} // namespace CosmicHex
