#include "TestTables.hpp"

#include <GameBots/Bots.hpp>
#include <SeededRandom/Generator.hpp>

#include <algorithm>
#include <utility>

namespace CosmicHex::Handheld::TestTables {

namespace {
// A game played on, each player's move chosen by `choose` from their legal
// moves (the dealer's drawn from `seed`)
State played(const std::function<bool(const State &)> &until, std::uint64_t seed, int limit,
             const std::function<Action(const std::vector<Action> &)> &choose) {
  State state = newGame(4, agreedRules(boardR3(), 4));
  SeededRandom::Generator dealer(seed);
  for (int i = 0; i < limit && !until(state) && state.winner < 0; ++i)
    Game::apply(state, Game::currentPlayer(state) == GameBots::kDealer
                           ? Game::dealerAction(state, dealer)
                           : choose(Game::legalActions(state)));
  return state;
}
} // namespace

State playedUntil(const std::function<bool(const State &)> &until, std::uint64_t seed, int limit) {
  return played(until, seed, limit, [](const std::vector<Action> &legal) { return legal.front(); });
}

State playedBuyingUntil(const std::function<bool(const State &)> &until, std::uint64_t seed,
                        int limit) {
  return played(until, seed, limit, [](const std::vector<Action> &legal) {
    const auto buy = std::find_if(legal.begin(), legal.end(), [](const Action &a) {
      return a.kind == Action::Kind::BuyContract;
    });
    return buy != legal.end() ? *buy : legal.front();
  });
}

State atFirstMain(std::uint64_t seed) {
  return playedUntil([](const State &state) { return state.phase == Phase::Main; }, seed);
}

State offeredToYou(std::uint64_t seed) {
  State state = playedUntil(
      [](const State &s) {
        if (s.phase != Phase::Trading || s.turn != s.players - 1)
          return false;
        const Resources &have = s.player[static_cast<std::size_t>(s.turn)].resources;
        return std::any_of(have.begin(), have.end(), [](int n) { return n > 0; });
      },
      seed);
  if (state.phase != Phase::Trading)
    return state; // (never got there: the test says so)
  const Resources &have = state.player[static_cast<std::size_t>(state.turn)].resources;
  const auto held = static_cast<std::size_t>(
      std::find_if(have.begin(), have.end(), [](int n) { return n > 0; }) - have.begin());
  Resources give{}, get{};
  give[held] = 1;
  get[(held + 1) % get.size()] = 1;
  Game::apply(state, {Action::Kind::OfferTrade, bundleOf(give), bundleOf(get), -1});
  return state;
}

TableSettings yourTable(std::uint64_t seed) {
  return {.seats = {Seat::Person, Seat::Bot, Seat::Bot, Seat::Bot},
          .botIterations = 4,
          .seed = seed,
          .physicsDice = true};
}

bool playedOnUntil(TableGame &game, const std::function<bool(const TableGame &)> &until,
                   int limit) {
  for (int i = 0; i < limit && !game.over(); ++i) {
    if (until(game))
      return true;
    if (game.waitingForPerson())
      game.play(game.personMoves().front());
    else if (game.waitingForDice())
      game.rollDice(2, 4);
    else
      game.playOn();
  }
  return until(game);
}

TableSnapshot snapshotWith(State state, std::vector<TableGame::Played> played) {
  return TableSnapshot{.state = std::move(state), .played = std::move(played)};
}

} // namespace CosmicHex::Handheld::TestTables
