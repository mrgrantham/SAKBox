#include "BoardProving.hpp"

namespace CosmicHex::Lab {

double fairLine(int players) {
  switch (players) {
  case 2:
    return 3.84;
  case 3:
    return 5.99;
  default:
    return 7.81;
  }
}

bool Fairness::fair(int players) const {
  return games > 0 && turnChi < fairLine(players) && homeChi < fairLine(players);
}

Fairness fairnessOf(std::span<const Batch::GameResult> games, int players) {
  std::vector<int> byTurn(static_cast<std::size_t>(players)),
      byHome(static_cast<std::size_t>(players));
  Fairness out;
  for (const Batch::GameResult &game : games) {
    if (game.winner < 0)
      continue; // (unfinished)
    ++out.games;
    ++byHome[static_cast<std::size_t>(game.winner)];
    ++byTurn[static_cast<std::size_t>((game.winner - game.first + players) % players)];
  }
  if (out.games == 0)
    return out;
  const double expected = static_cast<double>(out.games) / players;
  const auto chi = [&](const std::vector<int> &counts) {
    double sum = 0;
    for (int count : counts)
      sum += (count - expected) * (count - expected) / expected;
    return sum;
  };
  out.turnChi = chi(byTurn);
  out.homeChi = chi(byHome);
  return out;
}

void BoardProving::start(std::size_t boards, int players) {
  players_ = players;
  queue_.clear();
  results_.assign(boards, Result{});
  for (std::size_t b = 0; b < boards; ++b) {
    queue_.push_back({b, kScreenGames, false});
    results_[b].queued = true;
  }
  playing_ = false;
}

std::optional<BoardProving::Run> BoardProving::next() {
  if (playing_ || queue_.empty())
    return std::nullopt;
  const Run run = queue_.front();
  queue_.pop_front();
  playing_ = true;
  return run;
}

void BoardProving::finished(const Run &run, const Fairness &fairness) {
  playing_ = false;
  if (run.board >= results_.size())
    return;
  Result &result = results_[run.board];
  if (run.deep) {
    result.deep = fairness;
    result.queued = false;
    return;
  }
  result.screen = fairness;
  if (fairness.fair(players_)) {
    queue_.push_back({run.board, kDeepGames, true}); // (deep runs after every screen)
  } else {
    result.queued = false;
  }
}

} // namespace CosmicHex::Lab
