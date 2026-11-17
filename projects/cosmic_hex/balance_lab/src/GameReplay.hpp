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
//                                    (CosmicHex/Describe.hpp)
#pragma once

#include <ActionLog/ActionLog.hpp>
#include <CosmicHex/Describe.hpp>
#include <CosmicHex/Rules.hpp>

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace CosmicHex::Lab {

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

} // namespace CosmicHex::Lab
