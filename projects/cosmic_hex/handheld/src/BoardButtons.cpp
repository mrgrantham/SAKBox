#include <CosmicHex/Handheld/BoardButtons.hpp>

#include <algorithm>

namespace CosmicHex::Handheld::BoardButtons {

Situation situationOf(const State &state, const std::vector<Action> &legal, bool yourMove,
                      bool yourRollWaits) {
  using Kind = Action::Kind;
  const auto has = [&](std::initializer_list<Kind> kinds) {
    return std::any_of(legal.begin(), legal.end(), [&](const Action &a) {
      return std::find(kinds.begin(), kinds.end(), a.kind) != kinds.end();
    });
  };
  Situation s;
  s.yourMove = yourMove;
  s.canRoll = yourRollWaits;
  if (!yourMove)
    return s;
  s.canEndTurn = has({Kind::EndTurn, Kind::EndTrading});
  // (the turn could end but for a new ship still on its colony)
  if (!s.canEndTurn && state.phase == Phase::Main && state.settings.newShipsLeave)
    s.newShipMustMove = std::any_of(state.ships.begin(), state.ships.end(), [&](const Ship &ship) {
      return ship.owner == state.turn && !ship.moved &&
             std::any_of(state.stations.begin(), state.stations.end(),
                         [&](const Station &station) { return station.at == ship.corner; });
    });
  // (with the bank, or with players while the turn's offers are open)
  s.canTrade = has({Kind::Trade, Kind::OfferTrade, Kind::EndTrading});
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
  if (s.moveListButton)
    out.push_back({Button::MoveList, edge, edge + 2 * step, size, true});
  // Bottom left: Roll, then End Turn in its place once you've rolled
  if (s.canRoll)
    out.push_back({Button::Roll, edge, bottom, size, true});
  else if (s.canEndTurn || s.newShipMustMove)
    out.push_back({Button::EndTurn, edge, bottom, size, s.canEndTurn});
  // Bottom right, apart from End Turn: Confirm, while placing
  if (s.placing)
    out.push_back({Button::Confirm, right, bottom, size, s.placeChosen});
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
  case Button::MoveList:
    return Art::Icon::MoveList;
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
  case Button::Confirm:
    return Art::Icon::Confirm;
  }
  return Art::Icon::Build;
}

void Fades::update(const std::vector<Place> &places, double now) {
  // (the first layout: there at once; nothing fades in as a screen opens)
  const double came = first_ ? now - kSeconds : now;
  first_ = false;
  for (Tracked &t : tracked_) {
    const auto still = std::find_if(places.begin(), places.end(),
                                    [&](const Place &p) { return p.button == t.place.button; });
    if (still != places.end()) {
      if (t.going) // (back again: fading in from where it is)
        t = {*still, now - kSeconds * opacityOf(t, now), false};
      else
        t.place = *still;
    } else if (!t.going) {
      t = {t.place, now, true};
      t.place.usable = false;
    }
  }
  for (const Place &p : places)
    if (std::none_of(tracked_.begin(), tracked_.end(),
                     [&](const Tracked &t) { return t.place.button == p.button; }))
      tracked_.push_back({p, came, false});
  // (gone and faded: forgotten)
  tracked_.erase(
      std::remove_if(tracked_.begin(), tracked_.end(),
                     [&](const Tracked &t) { return t.going && now - t.since >= kSeconds; }),
      tracked_.end());
}

float Fades::opacityOf(const Tracked &tracked, double now) const {
  const auto along = static_cast<float>(std::clamp((now - tracked.since) / kSeconds, 0.0, 1.0));
  return tracked.going ? 1 - along : along;
}

std::vector<Fades::Shown> Fades::shown(double now) const {
  std::vector<Shown> out;
  out.reserve(tracked_.size());
  for (const Tracked &t : tracked_)
    out.push_back({t.place, opacityOf(t, now)});
  return out;
}

bool Fades::fading(double now) const {
  return std::any_of(tracked_.begin(), tracked_.end(),
                     [&](const Tracked &t) { return now - t.since < kSeconds; });
}

} // namespace CosmicHex::Handheld::BoardButtons
