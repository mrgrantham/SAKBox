#include <CosmicHex/Handheld/BoardButtons.hpp>

#include <algorithm>

namespace CosmicHex::Handheld::BoardButtons {

Situation situationOf(const State &state, const std::vector<Action> &legal, bool yourMove) {
  using Kind = Action::Kind;
  const auto has = [&](std::initializer_list<Kind> kinds) {
    return std::any_of(legal.begin(), legal.end(), [&](const Action &a) {
      return std::find(kinds.begin(), kinds.end(), a.kind) != kinds.end();
    });
  };
  Situation s;
  s.yourMove = yourMove;
  if (!yourMove)
    return s;
  // (the roll is the dealer's in the rules: the app throws the dice when
  // the player's turn reaches it)
  s.canRoll = state.phase == Phase::RollDraw;
  s.canEndTurn = has({Kind::EndTurn, Kind::EndTrading});
  s.canTrade = has({Kind::Trade, Kind::OfferTrade});
  const Player &me = state.player[static_cast<std::size_t>(state.turn)];
  s.holdsCards = !me.contracts.empty();
  return s;
}

std::vector<Place> layout(int width, int height, const Situation &s) {
  // 22 pixels on a 256-wide screen, in proportion on wider ones
  const float size = 22.0f * static_cast<float>(width) / 256.0f, edge = 3;
  const float right = static_cast<float>(width) - edge - size;
  const float bottom = static_cast<float>(height) - edge - size;
  const float step = size + 3;
  std::vector<Place> out{
      {Button::ShowResources, edge, edge, size, true},
      {Button::OtherPlayers, edge, edge + step, size, true},
      {Button::Build, right, edge, size, s.yourMove},
      {Button::Trade, right, edge + step, size, s.canTrade},
      {Button::Cards, right, edge + 2 * step, size, s.holdsCards},
  };
  if (s.canRoll)
    out.push_back({Button::Roll, edge, bottom, size, true});
  if (s.canEndTurn)
    out.push_back({Button::EndTurn, right, bottom, size, true});
  return out;
}

std::optional<Button> at(const std::vector<Place> &places, int x, int y) {
  for (const Place &p : places)
    if (p.usable && static_cast<float>(x) >= p.x && static_cast<float>(x) < p.x + p.size &&
        static_cast<float>(y) >= p.y && static_cast<float>(y) < p.y + p.size)
      return p.button;
  return std::nullopt;
}

Art::Icon iconOf(Button button) {
  switch (button) {
  case Button::ShowResources:
    return Art::Icon::ShowResources;
  case Button::OtherPlayers:
    return Art::Icon::OtherPlayers;
  case Button::Build:
    return Art::Icon::Build;
  case Button::Trade:
    return Art::Icon::Trade;
  case Button::Cards:
    return Art::Icon::Cards;
  case Button::Roll:
    return Art::Icon::Roll;
  case Button::EndTurn:
    return Art::Icon::EndTurn;
  }
  return Art::Icon::Build;
}

} // namespace CosmicHex::Handheld::BoardButtons
