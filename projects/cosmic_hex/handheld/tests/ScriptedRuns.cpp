#include "ScriptedRuns.hpp"

#include <CosmicHex/Rules.hpp>

#include <cmath>

namespace CosmicHex::Handheld::Testing {

using Cut = ShowLog::Cut;

std::unique_ptr<ScriptedGame> played(const ScriptedGameSettings &settings) {
  auto game = std::make_unique<ScriptedGame>(settings);
  game->run();
  return game;
}

ScriptedGameSettings shortGame(ScriptedGameSettings settings) {
  settings.winPoints = kShortWinPoints;
  settings.exhaustHomesAt = kShortExhaustAt;
  return settings;
}

std::vector<ScriptedGameSettings> shortGames(std::uint64_t first, int count,
                                             const ScriptedGameSettings &settings) {
  std::vector<ScriptedGameSettings> games;
  for (int g = 0; g < count; ++g) {
    ScriptedGameSettings game = shortGame(settings);
    game.seed = first + static_cast<std::uint64_t>(g);
    games.push_back(game);
  }
  return games;
}

std::vector<PlayedGame> playAll(const std::vector<ScriptedGameSettings> &games) {
  std::vector<PlayedGame> runs;
  runs.reserve(games.size());
  for (const ScriptedGameSettings &settings : games)
    runs.push_back({settings, played(settings)});
  return runs;
}

std::string nameOf(const ScriptedGameSettings &settings) {
  std::string name = "seed " + std::to_string(settings.seed);
  if (settings.animations == Animations::Quick)
    name += ", Quick";
  if (settings.frameSeconds > 1.0 / 60 + 1e-9)
    name += ", " + std::to_string(std::lround(1 / settings.frameSeconds)) + " frames a second";
  if (!settings.person)
    name += ", watched";
  if (settings.impatience > 0)
    name += ", impatient";
  if (settings.winPoints)
    name += ", win at " + std::to_string(*settings.winPoints);
  return name;
}

std::string unasked(const ScriptedGame &game, std::optional<ShowKind> kind) {
  std::string found;
  for (const ShowLog::Entry &entry : game.log().entries())
    if ((!kind || entry.kind == *kind) && entry.cut() != Cut::None && !ShowLog::asked(entry.cut()))
      found += std::string(nameOf(entry.kind)) + " " + std::string(ShowLog::nameOf(entry.cut())) +
               " at " + std::to_string(entry.startedAt) + " (" + entry.note + ")\n";
  return found;
}

std::string endedByTheWin(const ScriptedGame &game) {
  const State &state = game.table().state();
  if (!game.table().over())
    return "unfinished after " + std::to_string(game.seconds()) + " s";
  const int winning = points(state, state.winner);
  if (winning < state.settings.winPoints)
    return "won with " + std::to_string(winning) + " of " +
           std::to_string(state.settings.winPoints) + " points";
  bool celebrated = false;
  for (const ShowLog::Entry &entry : game.log().entries())
    celebrated = celebrated || entry.kind == ShowKind::Celebration;
  if (!celebrated)
    return "the win not celebrated";
  return "";
}

} // namespace CosmicHex::Handheld::Testing
