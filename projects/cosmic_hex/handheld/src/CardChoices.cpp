#include <CosmicHex/Handheld/CardChoices.hpp>
#include <CosmicHex/Handheld/CardsHeld.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

namespace {
using Kind = Action::Kind;

// What using a card is in the rules, by its drawing
Kind kindOf(Art::Card card) {
  switch (card) {
  case Art::Card::Enforcer:
    return Kind::PlayEnforcer;
  case Art::Card::InterstellarCartel:
    return Kind::PlayCartel;
  case Art::Card::ShipyardSurplus:
    return Kind::PlayShipyard;
  case Art::Card::SupplyDrop:
    return Kind::PlaySupplyDrop;
  default:
    return Kind::RevealContract;
  }
}
} // namespace

std::vector<HeldCard> heldCards(const State &state, const std::vector<Action> &legal, int player,
                                bool yourMove) {
  std::vector<HeldCard> out;
  const Player &me = state.player[static_cast<std::size_t>(player)];
  for (const int id : me.contracts) {
    const std::optional<Art::Card> card = cardOf(id);
    if (!card)
      continue;
    HeldCard held;
    held.contract = id;
    held.card = *card;
    held.kind = kindOf(*card);
    held.charter = held.kind == Kind::RevealContract;
    held.playable = yourMove && std::any_of(legal.begin(), legal.end(), [&](const Action &a) {
                      return a.kind == held.kind && (!held.charter || a.a == id);
                    });
    if (!held.playable)
      held.why = !yourMove      ? "On your turn"
                 : held.charter ? "Not met yet"
                                : "Not now: one card a turn, not the turn it's bought";
    out.push_back(held);
  }
  return out;
}

std::optional<KeepChoice> keepChoiceOf(const State &state, const std::vector<Action> &legal) {
  if (state.phase != Phase::KeepContract)
    return std::nullopt;
  KeepChoice choice;
  for (const Action &a : legal)
    if (a.kind == Kind::KeepContract)
      if (const std::optional<Art::Card> card = cardOf(a.a)) {
        choice.cards.push_back(*card);
        choice.actions.push_back(a);
      }
  if (choice.actions.empty())
    return std::nullopt;
  return choice;
}

} // namespace CosmicHex::Handheld
