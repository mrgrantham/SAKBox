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
} // namespace

std::optional<Action> ResourceChoice::actionFor(const std::vector<Resource> &picked) const {
  if (static_cast<int>(picked.size()) != howMany)
    return std::nullopt;
  const int first = static_cast<int>(picked[0]);
  const int second = howMany == 2 ? static_cast<int>(picked[1]) : -1;
  for (const Action &a : actions) {
    if (howMany == 1 && a.a == first)
      return a;
    if (howMany == 2 && ((a.a == first && a.b == second) || (a.a == second && a.b == first)))
      return a;
  }
  return std::nullopt;
}

bool ResourceChoice::offers(Resource resource) const {
  const int r = static_cast<int>(resource);
  return std::any_of(actions.begin(), actions.end(),
                     [&](const Action &a) { return a.a == r || (howMany == 2 && a.b == r); });
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
    if (!actions.empty())
      return ResourceChoice{"Salvage", "Take one resource", 1, std::move(actions)};
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
    return ResourceChoice{"Supply drop: any 2", "Tap two (the same twice: tap it again)", 2,
                          std::move(actions)};
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
