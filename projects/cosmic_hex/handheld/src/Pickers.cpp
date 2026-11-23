#include <CosmicHex/Handheld/Pickers.hpp>

#include <algorithm>
#include <numeric>

namespace CosmicHex::Handheld::Pickers {

namespace {
using Kind = Action::Kind;

std::vector<Action> ofKind(const std::vector<Action> &legal, Kind kind) {
  std::vector<Action> out;
  std::copy_if(legal.begin(), legal.end(), std::back_inserter(out),
               [&](const Action &a) { return a.kind == kind; });
  return out;
}

// What the picker says to do when more than one is picked on it
std::string pickSeveralNote(int howMany) {
  return "Tap " + (howMany == 2 ? std::string("two") : std::to_string(howMany)) +
         " (the same twice: tap it again)";
}
} // namespace

std::vector<Action> ResourceChoice::movesFor(const std::vector<Resource> &picked) const {
  if (static_cast<int>(picked.size()) != howMany || picked.empty())
    return {};
  const auto moveNaming = [&](int first, int second) -> std::optional<Action> {
    for (const Action &a : actions)
      if (a.a == first && (second < 0 || a.b == second))
        return a;
    return std::nullopt;
  };
  std::vector<Action> moves;
  if (movePerPick) {
    for (const Resource r : picked)
      if (const auto move = moveNaming(static_cast<int>(r), -1))
        moves.push_back(*move);
      else
        return {};
    return moves;
  }
  const int first = static_cast<int>(picked[0]);
  if (howMany == 1)
    if (const auto move = moveNaming(first, -1))
      return {*move};
  if (howMany == 2) {
    const int second = static_cast<int>(picked[1]);
    if (const auto move = moveNaming(first, second))
      return {*move};
    if (const auto move = moveNaming(second, first))
      return {*move};
  }
  return {};
}

bool ResourceChoice::offers(Resource resource) const {
  const int r = static_cast<int>(resource);
  return std::any_of(actions.begin(), actions.end(), [&](const Action &a) {
    return a.a == r || (howMany == 2 && !movePerPick && a.b == r);
  });
}

std::optional<ResourceChoice> resourceChoiceOf(const State &state,
                                               const std::vector<Action> &legal) {
  if (state.phase == Phase::HomeIncome) {
    auto actions = ofKind(legal, Kind::ChooseHome);
    if (!actions.empty())
      return ResourceChoice{"Your home pays 1", "Choose which resource", 1, std::move(actions),
                            std::max(1, state.homeIncomeLeft)};
  }
  if (state.phase == Phase::Salvage) {
    auto actions = ofKind(legal, Kind::TakeSalvage);
    const int howMany = std::max(1, state.salvageLeft);
    if (!actions.empty())
      return ResourceChoice{.title = "Salvage: any " + std::to_string(howMany),
                            .note = howMany == 1 ? "Take one resource" : pickSeveralNote(howMany),
                            .howMany = howMany,
                            .actions = std::move(actions),
                            .movePerPick = true};
  }
  return std::nullopt;
}

std::optional<ResourceChoice> cardChoiceOf(const std::vector<Action> &legal, Action::Kind card) {
  auto actions = ofKind(legal, card);
  if (actions.empty())
    return std::nullopt;
  if (card == Kind::PlayCartel)
    return ResourceChoice{"Interstellar Cartel", "Name a resource", 1, std::move(actions)};
  if (card == Kind::PlaySupplyDrop)
    return ResourceChoice{"Supply drop: any 2", pickSeveralNote(2), 2, std::move(actions)};
  return std::nullopt;
}

std::optional<PlayerChoice> stealChoiceOf(const State &state, const std::vector<Action> &legal,
                                          int planet) {
  PlayerChoice choice{planet, {}};
  for (const Action &a : legal) {
    if (a.kind != Kind::MoveBlockade || a.a != planet || a.b < 0)
      continue;
    const Resources &held = state.player[static_cast<std::size_t>(a.b)].resources;
    choice.options.push_back({a.b, std::accumulate(held.begin(), held.end(), 0), a});
  }
  if (choice.options.size() < 2)
    return std::nullopt;
  return choice;
}

} // namespace CosmicHex::Handheld::Pickers
