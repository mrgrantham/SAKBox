// BoardProving.hpp - the lab proving the layout solver's boards by playing
// them (round 87, the user's: equal access isn't enough, only games tell a
// fair board). Each board queued plays kScreenGames; one fair by both turn
// position and home (chi-square under the 5% line) then plays kDeepGames
// more, on fresh seeds, since a few hundred games flatter a board (round 84).
//
//   BoardProving proving;
//   proving.start(6);                         // the solver's first six
//   while (auto run = proving.next()) {       // a board and how many games
//     ... play run->games on board run->board, then
//     proving.finished(*run, fairnessOf(results, players));
//   }
#pragma once

#include "../../simulate/Batch.hpp"

#include <cstddef>
#include <deque>
#include <optional>
#include <span>
#include <vector>

namespace CosmicHex::Lab {

// How evenly a batch's wins fell: by turn position and by home, each a
// chi-square against equal shares
struct Fairness {
  int games = 0; // finished
  double turnChi = 0, homeChi = 0;
  // Fair by both at the 5% line for this many players
  bool fair(int players) const;
};

// The 5% line for (players - 1) degrees of freedom: 3.84, 5.99, 7.81
double fairLine(int players);

Fairness fairnessOf(std::span<const Batch::GameResult> games, int players);

class BoardProving {
public:
  static constexpr int kScreenGames = 300;
  static constexpr int kDeepGames = 1800;

  struct Run {
    std::size_t board;
    int games;
    bool deep; // the second, longer run
  };
  struct Result {
    std::optional<Fairness> screen, deep;
    bool queued = false;
  };

  // Queues the first `boards` boards for screening (forgetting any before)
  void start(std::size_t boards, int players);
  // What to play next, or nothing when every queued board is done
  std::optional<Run> next();
  // A run played: its fairness kept, and a fair screened board queued deep
  void finished(const Run &run, const Fairness &fairness);
  void stop() { queue_.clear(); }

  bool busy() const { return !queue_.empty() || playing_; }
  const std::vector<Result> &results() const { return results_; }
  int players() const { return players_; }

private:
  std::deque<Run> queue_;
  std::vector<Result> results_;
  bool playing_ = false;
  int players_ = 4;
};

} // namespace CosmicHex::Lab
