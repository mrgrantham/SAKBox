#include <CosmicHex/Handheld/CardsHeld.hpp>

namespace CosmicHex::Handheld {

std::optional<Art::Card> cardOf(int contractId) {
  switch (contractId) {
  case kEnforcerCard:
    return Art::Card::Enforcer;
  case kCartelCard:
    return Art::Card::InterstellarCartel;
  case kShipyardCard:
    return Art::Card::ShipyardSurplus;
  case kSupplyDropCard:
    return Art::Card::SupplyDrop;
  case 21:
    return Art::Card::CharterThreeOfAKind;
  case 19:
    return Art::Card::CharterOccupier;
  case kDeepSpaceRelay:
    return Art::Card::CharterDeepSpaceRelay;
  default:
    return std::nullopt;
  }
}

} // namespace CosmicHex::Handheld
