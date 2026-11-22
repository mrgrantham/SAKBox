#include <CosmicHex/Handheld/ScriptedPlayer.hpp>

#include <CosmicHex/Handheld/BuildMenu.hpp>

#include <SeededRandom/Seed.hpp>

#include <cmath>
#include <vector>

namespace CosmicHex::Handheld {

namespace {

using Key = BottomScreen::Key;
using Mode = BottomScreen::Mode;

ScriptedInput press(Key key) { return {.kind = ScriptedInput::Kind::Press, .key = key}; }
ScriptedInput tapAt(HexGrid::Point point) {
  return {.kind = ScriptedInput::Kind::Tap,
          .x = static_cast<int>(std::lround(point.x)),
          .y = static_cast<int>(std::lround(point.y))};
}
ScriptedInput tapAt(std::array<int, 2> at) {
  return {.kind = ScriptedInput::Kind::Tap, .x = at[0], .y = at[1]};
}

// A rough fingerprint of what's on the screen: the same for many inputs
// means they aren't getting anywhere
std::uint64_t lookOf(const TableSnapshot &snapshot, const BottomScreen &screen) {
  const State &state = snapshot.state;
  std::uint64_t look = static_cast<std::uint64_t>(state.round) * 1000003u;
  look = look * 31 + static_cast<std::uint64_t>(state.turn);
  look = look * 31 + static_cast<std::uint64_t>(state.phase);
  look = look * 31 + static_cast<std::uint64_t>(screen.mode());
  look = look * 31 + state.ships.size() * 7 + state.stations.size();
  for (const int n : state.player[0].resources)
    look = look * 31 + static_cast<std::uint64_t>(n);
  return look;
}

} // namespace

ScriptedPlayer::ScriptedPlayer(std::uint64_t seed)
    : random_(SeededRandom::Seed::mixedFrom(seed).stream("scripted player")) {}

bool ScriptedPlayer::chance(int percent) { return static_cast<int>(random_.below(100)) < percent; }

ScriptedInput ScriptedPlayer::next(const TableSnapshot &snapshot, const BottomScreen &screen,
                                   const BoardPicture &map, bool yourRollWaits) {
  // (screens to look at closed: this player doesn't look)
  if (screen.mode() == Mode::Cards || screen.mode() == Mode::Others)
    return press(Key::Back);
  if (!snapshot.yourMove && !yourRollWaits)
    return {};
  const State &state = snapshot.state;
  if (state.round != round_ || state.turn != turn_) {
    round_ = state.round;
    turn_ = state.turn;
    tries_ = 0;
  }
  // Getting nowhere: back out, then end the turn
  const std::uint64_t look = lookOf(snapshot, screen);
  sameLook_ = look == lastLook_ ? sameLook_ + 1 : 0;
  lastLook_ = look;
  if (sameLook_ > 12 && sameLook_ % 4 == 1) {
    tries_ = 0; // (and then the board afresh: a ship moved, or the turn ended)
    return press(Key::Back);
  }
  switch (screen.mode()) {
  case Mode::Board:
    return onTheBoard(snapshot, screen, map, yourRollWaits);
  case Mode::Placing:
    return placing(screen, map);
  case Mode::Build:
    return building();
  case Mode::Resources:
    // (the card in focus picked, or the next one along)
    return press(chance(35) ? Key::Right : Key::Confirm);
  case Mode::Players:
    return press(Key::Confirm);
  case Mode::Keep:
    return press(screen.keepChosen() < 0 ? Key::Right : Key::Confirm);
  case Mode::Trade:
    // (an offer to you: accepted if it can be, else declined)
    return press(sameLook_ == 0 ? Key::Confirm : Key::Back);
  case Mode::Cards:
  case Mode::Others:
    break;
  }
  return press(Key::Back);
}

// On the board: the dice thrown when they wait; else something built, a
// ship moved, or (three tries on, or nothing to do) the turn ended
ScriptedInput ScriptedPlayer::onTheBoard(const TableSnapshot &snapshot, const BottomScreen &screen,
                                         const BoardPicture &map, bool yourRollWaits) {
  using Button = BoardButtons::Button;
  if (yourRollWaits)
    if (const auto roll = screen.buttonAt(Button::Roll))
      return tapAt(*roll);
  const State &state = snapshot.state;
  if (tries_ < 3) {
    ++tries_;
    // Something to build: the build screen, and down to its row
    const std::vector<BuildRow> rows = buildRows(state);
    for (int i = static_cast<int>(rows.size()) - 1; i >= 0; --i)
      if (rows[static_cast<std::size_t>(i)].canBuild && chance(80))
        if (const auto build = screen.buttonAt(Button::Build)) {
          buildRow_ = i;
          buildSteps_ = 0;
          return tapAt(*build);
        }
    // A ship that can move: touched (its moves marked)
    std::vector<int> movable;
    for (const Action &move : snapshot.moves)
      if (move.kind == Action::Kind::Move)
        movable.push_back(move.a);
    if (!movable.empty() && chance(70)) {
      const int ship = movable[random_.below(movable.size())];
      return tapAt(map.where(state.ships[static_cast<std::size_t>(ship)].corner));
    }
  }
  if (const auto end = screen.buttonAt(Button::EndTurn))
    return tapAt(*end);
  // (End Turn not usable: a new ship must move first)
  for (const Action &move : snapshot.moves)
    if (move.kind == Action::Kind::Move)
      return tapAt(map.where(state.ships[static_cast<std::size_t>(move.a)].corner));
  return {};
}

// Placing (the start's pieces, a build, a ship's move, the blockade): a
// marked place touched, then Confirm
ScriptedInput ScriptedPlayer::placing(const BottomScreen &screen, const BoardPicture &map) {
  if (screen.chosenPlacement())
    return press(Key::Confirm);
  if (screen.placingBlockade()) {
    const std::vector<Hex> hexes = screen.placingHexes();
    if (!hexes.empty())
      return tapAt(map.where(hexes[random_.below(hexes.size())]));
    return {}; // (nowhere to go: the game goes on by itself)
  }
  const std::vector<Corner> corners = screen.placingCorners();
  if (corners.empty())
    return press(Key::Back);
  return tapAt(map.where(corners[random_.below(corners.size())]));
}

// The build screen: down to the row chosen on the board (the first press
// chooses the top row), then Build
ScriptedInput ScriptedPlayer::building() {
  if (buildSteps_++ <= buildRow_)
    return press(Key::Down);
  return press(Key::Confirm);
}

} // namespace CosmicHex::Handheld
