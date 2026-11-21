// Pickers.hpp - the two choosing screens: what's
// being chosen, from the rules' legal moves, and which move a choice makes.
//
// The resource picker: one screen for every "choose a resource" (your
// home's pay, salvage, the Interstellar Cartel's resource, the Supply drop's
// two). The player picker: whom to steal from once the blockade has moved,
// only those the rules allow, with how many cards each holds.
//
//   if (const auto choice = Pickers::resourceChoiceOf(state, legal))
//     ... show it; then send(*choice->actionFor({Fuel}));
#pragma once

#include <CosmicHex/Rules.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace CosmicHex::Handheld::Pickers {

struct ResourceChoice {
  std::string_view title; // "Your home pays 1"
  std::string_view note;  // "Choose which resource"
  int howMany = 1;        // 1, or 2 (the Supply drop)
  std::vector<Action> actions;
  // How many such choices are left, this one included (a home paying 2:
  // two in a row, each its own; the screen says so)
  int left = 1;
  // The same choice, or the next (a new one opens the picker afresh)
  bool sameAs(const ResourceChoice &other) const {
    return title == other.title && left == other.left;
  }
  // The move for the resources picked (one, or two in either order); none
  // if the rules have no such move
  std::optional<Action> actionFor(const std::vector<Resource> &picked) const;
  // Whether `resource` can be picked at all (for the Cartel, say, any)
  bool offers(Resource resource) const;
};

// The choice waiting for the player now, if their legal moves are a choice
// of resources: the home's pay (HomeIncome) or salvage (Salvage)
std::optional<ResourceChoice> resourceChoiceOf(const State &state,
                                               const std::vector<Action> &legal);
// The choice a card brings, from the cards screen: the Interstellar Cartel's
// resource or the Supply drop's two (nothing for other cards)
std::optional<ResourceChoice> cardChoiceOf(const std::vector<Action> &legal, Action::Kind card);

struct PlayerChoice {
  int planet = -1;
  struct Option {
    int player;
    int cards; // how many resource cards they hold (face down on the screen)
    Action action;
  };
  std::vector<Option> options;
};

// Whom the player may steal from with the blockade on `planet`, when the
// rules give more than one (with one, or none, there's nothing to choose)
std::optional<PlayerChoice> stealChoiceOf(const State &state, const std::vector<Action> &legal,
                                          int planet);

} // namespace CosmicHex::Handheld::Pickers
