// Tally.hpp - a batch's results added up as its games come in, for the
// balance lab's charts: wins by turn position and by home (with the margin
// of a fair share), how long games last, and where stations end up.
#pragma once

#include "../../simulate/Batch.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace CosmicHex::Lab {

struct Tally {
  int players = 4;
  int games = 0;                             // played
  int finished = 0;                          // won (the rest were stopped unfinished)
  std::array<int, kMaxPlayers> winsByTurn{}; // 1st to play, 2nd...
  std::array<int, kMaxPlayers> winsByHome{}; // player 1's home, player 2's...
  std::vector<int> gamesByRounds;            // [r]: games won in round r
  std::array<double, 3> stationsBySystem{};  // at the end, in all: singles, pairs, threes
  double badges = 0;                         // earned, in all

  void add(const Batch::GameResult &result) {
    ++games;
    for (std::size_t i = 0; i < 3; ++i)
      stationsBySystem[i] += result.bySystem[i];
    badges += result.badgesAll;
    if (result.winner < 0)
      return;
    ++finished;
    ++winsByHome[static_cast<std::size_t>(result.winner)];
    ++winsByTurn[static_cast<std::size_t>((result.winner - result.first + players) % players)];
    if (gamesByRounds.size() <= static_cast<std::size_t>(result.rounds))
      gamesByRounds.resize(static_cast<std::size_t>(result.rounds) + 1);
    ++gamesByRounds[static_cast<std::size_t>(result.rounds)];
  }

  // A count of finished games as a percentage of them
  double share(int count) const { return finished ? 100.0 * count / finished : 0.0; }
  // A fair share of wins (100 / players), and how far either side of it a
  // fair game's result would fall 95% of the time
  double fairShare() const { return 100.0 / players; }
  double margin() const {
    if (finished == 0)
      return 0;
    const double p = 1.0 / players;
    return 196.0 * std::sqrt(p * (1 - p) / finished);
  }
  double averageRounds() const {
    double rounds = 0;
    for (std::size_t r = 0; r < gamesByRounds.size(); ++r)
      rounds += static_cast<double>(r) * gamesByRounds[r];
    return finished ? rounds / finished : 0.0;
  }
};

} // namespace CosmicHex::Lab
