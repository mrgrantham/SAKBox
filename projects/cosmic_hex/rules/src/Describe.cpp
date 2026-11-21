#include <CosmicHex/Describe.hpp>

#include <string>

namespace CosmicHex {

// Who takes an action in `before`: "Player 2", or "The dealer"
std::string whoActs(const State &before) {
  const int who = Game::currentPlayer(before);
  return who < 0 ? std::string("The dealer") : "Player " + std::to_string(who + 1);
}

// What an action did, in a sentence: "Player 2 builds a station."
std::string describe(const State &before, const Action &action) {
  using Kind = Action::Kind;
  const auto resource = [](int r) {
    return r >= 0 && r < kResources ? std::string(kResourceShown[static_cast<std::size_t>(r)])
                                    : std::string("?");
  };
  const std::string who = whoActs(before);
  switch (action.kind) {
  case Kind::ChooseHome:
    return who + " takes " + resource(action.a) + " from their home.";
  case Kind::Roll: {
    const int sum = action.a + action.b;
    return "Roll: " + std::to_string(action.a) + " + " + std::to_string(action.b) + " = " +
           std::to_string(sum) + (sum == 7 ? ": the blockade moves." : ".");
  }
  case Kind::MoveBlockade:
    if (action.a < 0)
      return who + " has nowhere to put the blockade.";
    return who + " moves the blockade" +
           (action.b >= 0 ? " and steals from Player " + std::to_string(action.b + 1) + "." : ".");
  case Kind::Steal:
    return "One " + resource(action.a) + " is stolen.";
  case Kind::Trade:
    return who + " trades " + resource(action.a) + " for 1 " + resource(action.b) + ".";
  case Kind::BuildShip:
    return who + " builds a ship.";
  case Kind::Move:
    return who + " moves a ship.";
  case Kind::RevealPlanet: {
    const auto kind = static_cast<std::size_t>(action.a);
    if (kind == static_cast<std::size_t>(PlanetKind::Moon))
      return "A planet turns over: a moon.";
    return "A planet turns over: " +
           std::string(kind < kKindShown.size() ? kKindShown[kind] : "?") + ", on " +
           std::to_string(action.b) + ".";
  }
  case Kind::RevealFind: {
    const auto find = static_cast<std::size_t>(action.a);
    return "An anomaly turns over: " +
           std::string(find < kFindShown.size() ? kFindShown[find] : "?") + ".";
  }
  case Kind::TakeSalvage:
    return who + " takes " + resource(action.a) + " from the salvage.";
  case Kind::DealStartCard:
    return "A starting card is dealt.";
  case Kind::Settle:
    return who + " builds a station.";
  case Kind::BuildColony:
    return who + " makes a station a colony.";
  case Kind::BuyContract:
    return who + " buys a contract.";
  case Kind::DealContracts:
    return "Two contracts are dealt.";
  case Kind::KeepContract:
    return who + " keeps a contract.";
  case Kind::RevealContract: {
    const auto id = static_cast<std::size_t>(action.a);
    const auto &all = contracts();
    return who + " reveals a contract" +
           (id < all.size() ? ": " + std::string(all[id].name) + "." : ".");
  }
  case Kind::PlayEnforcer:
    return who + " plays an Enforcer.";
  case Kind::RollHomeDie:
    return action.a < kResources ? "The home die gives " + resource(action.a) + "."
                                 : "The home die: the player's choice.";
  case Kind::PlayShipyard:
    return who + " plays Shipyard Surplus: two ships for one.";
  case Kind::PlaySupplyDrop:
    return who + " plays a Supply drop: " + resource(action.a) + " and " + resource(action.b) + ".";
  case Kind::PlayCartel:
    return who + " plays an Interstellar Cartel: everyone's " + resource(action.a) + ".";
  case Kind::CashBadges:
    return who + " cashes in badges.";
  case Kind::EndTurn:
    return who + " ends their turn.";
  case Kind::PlaceColony:
    return who + " places their home colony.";
  case Kind::PlaceShip:
    return who + " places their starting ship.";
  case Kind::OfferTrade: {
    const Resources give = resourcesOf(action.a), get = resourcesOf(action.b);
    const auto list = [&](const Resources &r) {
      std::string text;
      for (int i = 0; i < kResources; ++i)
        if (r[static_cast<std::size_t>(i)] > 0)
          text += (text.empty() ? "" : ", ") + std::to_string(r[static_cast<std::size_t>(i)]) +
                  " " + resource(i);
      return text;
    };
    return who + " offers " + list(give) + " for " + list(get) +
           (action.c >= 0 ? " to Player " + std::to_string(action.c + 1) + "." : " to anyone.");
  }
  case Kind::AcceptTrade:
    return who + " accepts the trade.";
  case Kind::DeclineTrade:
    return who + " declines the trade.";
  case Kind::CounterTrade:
    return who + " makes a counter-offer.";
  case Kind::EndTrading:
    return who + " is done trading.";
  case Kind::NumberHome:
    return "An exhausted home world becomes a planet, on " + std::to_string(action.a) + ".";
  }
  return who + " acts.";
}

} // namespace CosmicHex
