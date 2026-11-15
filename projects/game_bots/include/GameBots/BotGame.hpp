// BotGame.hpp - what a game tells the bots, on top of what the action log
// already knows about it (ActionLog::Game: its state, its actions, and how
// an action changes the state).
//
// A bot needs to know whose turn it is, what they may do, what the dealer
// might do (rolls, draws) and how likely each outcome is, how a finished
// game scored, and, for games with hidden information, how to fill in what
// one player can't see with a guess that fits what they can see. That last
// part is what keeps a bot from cheating: it never looks at the real hidden
// tiles or cards, only at guesses drawn from what's still unknown.
#pragma once

#include <ActionLog/ActionLog.hpp>
#include <SeededRandom/Generator.hpp>

#include <concepts>
#include <vector>

namespace GameBots {

// The "player" whose turn it is when the next thing to happen is chance:
// a roll, a tile turned over, a card dealt.
inline constexpr int kDealer = -1;

// A game the bots can play. On top of ActionLog::Game, static members:
//   playerCount(state)          how many players (numbered from 0)
//   currentPlayer(state)        whose turn it is, or kDealer for chance
//   isOver(state)               whether the game has ended
//   score(state, player)        a finished game's result for a player, from
//                               0 (lost) to 1 (won); a draw is between
//   legalActions(state)         what the current player may do (not empty
//                               while the game is on and it's not chance)
//   dealerAction(state, random) a chance outcome, drawn with its real odds
//                               from what's still possible (the tiles left,
//                               the dice)
//   determinize(state, viewer, random)
//                               the state with everything `viewer` can't see
//                               replaced by a guess that fits what they can
//                               (the state itself, for a game with nothing
//                               hidden)
// Actions must be comparable with ==, so the search can tell them apart.
template <typename G>
concept BotGame =
    ActionLog::Game<G> && std::equality_comparable<typename G::Action> &&
    requires(const typename G::State &state, int player, SeededRandom::Generator &random) {
      { G::playerCount(state) } -> std::convertible_to<int>;
      { G::currentPlayer(state) } -> std::convertible_to<int>;
      { G::isOver(state) } -> std::convertible_to<bool>;
      { G::score(state, player) } -> std::convertible_to<double>;
      { G::legalActions(state) } -> std::same_as<std::vector<typename G::Action>>;
      { G::dealerAction(state, random) } -> std::same_as<typename G::Action>;
      { G::determinize(state, player, random) } -> std::same_as<typename G::State>;
    };

// Optional: estimate(state, player), a quick judgement (0 to 1) of an
// unfinished game, used when a simulated game is cut off before its end.
// Without it, a cut-off game counts as a draw (0.5).
template <typename G>
concept HasEstimate = requires(const typename G::State &state, int player) {
  { G::estimate(state, player) } -> std::convertible_to<double>;
};

} // namespace GameBots
