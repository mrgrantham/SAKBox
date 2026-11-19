// CardsHeld.hpp - the cards a player holds as the drawings that show them
// (art/cards): their resources, and their action cards and Charters by the
// rules' card ids
//
//   for (int id : player.contracts) if (auto card = cardOf(id)) ...
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>

namespace CosmicHex::Handheld {

// A resource's card
inline Art::Card cardOf(Resource resource) {
  return static_cast<Art::Card>(static_cast<int>(resource));
}

// An action card's or Charter's drawing, by the rules' id: the Enforcer,
// the Cartel, Shipyard Surplus, the Supply drop, and the three Charters in
// the deck; nothing for an id without one
std::optional<Art::Card> cardOf(int contractId);

} // namespace CosmicHex::Handheld
