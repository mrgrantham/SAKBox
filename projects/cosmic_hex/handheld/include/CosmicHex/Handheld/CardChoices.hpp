// CardChoices.hpp - what the cards screens show, from the rules' legal
// moves: the action cards and Charters you hold, each with whether it can
// be played (or a Charter revealed) now, and why not; and, after buying, the
// two cards dealt to keep one of
//
//   for (const HeldCard &held : heldCards(state, legal, yourMove)) ...
//   if (auto keep = keepChoiceOf(state, legal)) ...   // the screen opens by itself
#pragma once

#include <CosmicHex/Art.hpp>
#include <CosmicHex/Rules.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace CosmicHex::Handheld {

struct HeldCard {
  int contract = -1; // the rules' card id
  Art::Card card = Art::Card::Enforcer;
  bool charter = false;                      // (revealed, rather than played)
  Action::Kind kind = Action::Kind::EndTurn; // what playing or revealing it is
  bool playable = false;                     // the rules allow it now
  std::string_view why;                      // when not, why not (short, for the screen)
};
// `player`'s cards, in the order held; playable only on their move
// (`yourMove`: the legal moves are theirs)
std::vector<HeldCard> heldCards(const State &state, const std::vector<Action> &legal, int player,
                                bool yourMove);

// After buying a card: the two dealt, and the move that keeps each
struct KeepChoice {
  std::vector<Art::Card> cards;
  std::vector<Action> actions; // (in the same order)
};
std::optional<KeepChoice> keepChoiceOf(const State &state, const std::vector<Action> &legal);

} // namespace CosmicHex::Handheld
