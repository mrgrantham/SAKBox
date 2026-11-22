#include <CosmicHex/Handheld/TableSnapshot.hpp>

#include <CosmicHex/Describe.hpp>

#include <algorithm>
#include <utility>

namespace CosmicHex::Handheld {

TableSnapshot snapshotOf(const TableGame &game, std::size_t playedFrom, bool paused,
                         std::uint64_t seed) {
  const auto &history = game.history();
  const std::size_t shown = std::min<std::size_t>(history.size(), 8);
  const auto &played = game.played();
  const std::size_t from = std::min(playedFrom, played.size());
  const bool dice = game.waitingForDice(), person = game.waitingForPerson();
  std::vector<CosmicHex::Action> moves;
  std::vector<std::string> moveText;
  if (person) {
    moves = game.personMoves();
    for (const CosmicHex::Action &move : moves)
      moveText.push_back(CosmicHex::describe(game.state(), move));
  }
  return TableSnapshot{.state = game.state(),
                       .lastMoves = std::vector<std::string>(
                           history.end() - static_cast<long>(shown), history.end()),
                       .lastThinkSeconds = game.lastThinkSeconds(),
                       .paused = paused,
                       .yourMove = person,
                       .moves = std::move(moves),
                       .moveText = std::move(moveText),
                       .waitingForDice = dice,
                       .diceSeed = dice ? seed * 1000003u + history.size() : 0,
                       .firstRoll = game.firstRoll(),
                       .played = std::vector<TableGame::Played>(
                           played.begin() + static_cast<long>(from), played.end()),
                       .botGames = game.lastBotIterations(),
                       .botSearch = game.lastSearch()};
}

} // namespace CosmicHex::Handheld
