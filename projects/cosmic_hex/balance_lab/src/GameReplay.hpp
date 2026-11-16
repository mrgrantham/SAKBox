// GameReplay.hpp - a played game, step by step, to watch in the balance lab.
//
// A game's action log (its starting state and every action, the dealer's
// rolls and draws included) is played through once, keeping the state after
// every action, so the viewer can jump to any point at once: forwards,
// backwards, a turn at a time. It works for any log the rules can replay:
// the lab's bot games now, a real game played on a device later.
//
//   GameReplay replay = GameReplay::of(result.log);
//   replay.states[0]                 // the start
//   replay.states[i]                 // after i actions
//   describe(replay.states[i - 1], replay.actions[i - 1])  // what action i did
#pragma once

#include <ActionLog/ActionLog.hpp>
#include <CosmicHex/Rules.hpp>

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CosmicHex::Lab {

// Names to show (the rules' own are for log files)
inline constexpr std::array<std::string_view, kResources> kResourceShown = {
    "minerals", "food", "fuel", "water", "crystals"};
inline constexpr std::array<std::string_view, 7> kKindShown = {
    "asteroid belt", "lush planet", "gas giant", "ice moon", "nebula", "home", "moon"};
inline constexpr std::array<std::string_view, kFinds> kFindShown = {
    "nothing", "salvage", "ruins", "an alien station", "a pirate hideout"};

struct GameReplay {
  std::vector<State> states;   // states[i]: after i actions (states[0]: the start)
  std::vector<Action> actions; // actions[i]: the one taking states[i] to states[i + 1]
  std::vector<std::pair<std::string, std::string>> meta; // the log's notes (seed, bots...)
  std::string problem; // why the replay stopped early, if it did (empty: it didn't)
  // Each player's points after each action, for the points chart
  std::array<std::vector<double>, kMaxPlayers> points;

  static GameReplay of(const ActionLog::Log<Game> &log) {
    GameReplay replay;
    replay.meta = log.meta;
    replay.states.push_back(log.start);
    const auto result =
        ActionLog::replay(log, [&](const State &state, const Action &action, std::size_t) {
          replay.states.push_back(state);
          replay.actions.push_back(action);
        });
    if (result.problem)
      replay.problem = result.problem->why;
    for (const State &state : replay.states)
      for (int p = 0; p < state.players; ++p)
        replay.points[static_cast<std::size_t>(p)].push_back(CosmicHex::points(state, p));
    return replay;
  }

  // A log file's text (the simulator's --logs, the lab's own, a real game)
  static std::expected<GameReplay, std::string> fromText(std::string_view text) {
    auto log = ActionLog::read<Game>(text);
    if (!log)
      return std::unexpected("line " + std::to_string(log.error().line) + ": " + log.error().why);
    return of(*log);
  }

  // The last state's index (the end of the game, or where it stopped)
  std::size_t last() const { return states.size() - 1; }
  const State &at(std::size_t step) const { return states[std::min(step, last())]; }

  // Where the next turn starts after `step` (just after the next turn's
  // end, or a setup placement), or the end
  std::size_t nextTurn(std::size_t step) const {
    for (std::size_t i = step + 1; i <= last(); ++i)
      if (startsTurn(i))
        return i;
    return last();
  }
  // Where the turn `step` is in started (or the one before, from its start)
  std::size_t previousTurn(std::size_t step) const {
    for (std::size_t i = step; i-- > 0;)
      if (startsTurn(i))
        return i;
    return 0;
  }

private:
  bool startsTurn(std::size_t step) const {
    if (step == 0)
      return true;
    // Whoever's turn it is changed (setup placements included), or a turn
    // ended (a player can have two in a row at the turn of the setup's snake)
    return states[step].turn != states[step - 1].turn ||
           actions[step - 1].kind == Action::Kind::EndTurn;
  }
};

// Who takes an action in `before`: "Player 2", or "The dealer"
inline std::string whoActs(const State &before) {
  const int who = Game::currentPlayer(before);
  return who < 0 ? std::string("The dealer") : "Player " + std::to_string(who + 1);
}

// What an action did, in a sentence: "Player 2 builds a station."
inline std::string describe(const State &before, const Action &action) {
  using Kind = Action::Kind;
  const auto resource = [](int r) {
    return r >= 0 && r < kResources ? std::string(kResourceShown[static_cast<std::size_t>(r)])
                                    : std::string("?");
  };
  const std::string who = whoActs(before);
  switch (action.kind) {
  case Kind::ChooseHome:
    return who + " takes " + resource(action.a) + " from their home.";
  case Kind::Roll: {
    const int sum = action.a + action.b;
    return "Roll: " + std::to_string(action.a) + " + " + std::to_string(action.b) + " = " +
           std::to_string(sum) + (sum == 7 ? ": the blockade moves." : ".");
  }
  case Kind::MoveBlockade:
    if (action.a < 0)
      return who + " has nowhere to put the blockade.";
    return who + " moves the blockade" +
           (action.b >= 0 ? " and steals from Player " + std::to_string(action.b + 1) + "." : ".");
  case Kind::Steal:
    return "One " + resource(action.a) + " is stolen.";
  case Kind::Trade:
    return who + " trades " + resource(action.a) + " for 1 " + resource(action.b) + ".";
  case Kind::BuildShip:
    return who + " builds a ship.";
  case Kind::Move:
    return who + " moves a ship.";
  case Kind::RevealPlanet: {
    const auto kind = static_cast<std::size_t>(action.a);
    if (kind == static_cast<std::size_t>(PlanetKind::Moon))
      return "A planet turns over: a moon.";
    return "A planet turns over: " +
           std::string(kind < kKindShown.size() ? kKindShown[kind] : "?") + ", on " +
           std::to_string(action.b) + ".";
  }
  case Kind::RevealFind: {
    const auto find = static_cast<std::size_t>(action.a);
    return "A find turns over: " + std::string(find < kFindShown.size() ? kFindShown[find] : "?") +
           ".";
  }
  case Kind::TakeSalvage:
    return who + " takes " + resource(action.a) + " from the salvage.";
  case Kind::Settle:
    return who + " builds a station.";
  case Kind::BuildColony:
    return who + " makes a station a colony.";
  case Kind::BuyContract:
    return who + " buys a contract.";
  case Kind::DealContracts:
    return "Two contracts are dealt.";
  case Kind::KeepContract:
    return who + " keeps a contract.";
  case Kind::RevealContract: {
    const auto id = static_cast<std::size_t>(action.a);
    const auto &all = contracts();
    return who + " reveals a contract" +
           (id < all.size() ? ": " + std::string(all[id].name) + "." : ".");
  }
  case Kind::PlayEnforcer:
    return who + " plays an Enforcer.";
  case Kind::CashBadges:
    return who + " cashes in badges.";
  case Kind::EndTurn:
    return who + " ends their turn.";
  case Kind::PlaceColony:
    return who + " places their home colony.";
  case Kind::PlaceShip:
    return who + " places their starting ship.";
  }
  return who + " acts.";
}

} // namespace CosmicHex::Lab
