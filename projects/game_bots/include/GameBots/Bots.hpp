// Bots.hpp - players for any game that describes itself as a BotGame:
//
//   RandomBot   any allowed move, at random: a baseline every other bot
//               must beat.
//   Ismcts      information-set Monte Carlo tree search (Cowling, Powley and
//               Whitehouse, "Information Set Monte Carlo Tree Search", 2012;
//               the method Yannakakis and Togelius's AI and Games recommends
//               for games with dice and hidden information). It plays many
//               quick simulated games from the current position and picks
//               the move that did best. Before each one it fills in what it
//               can't see with a fresh guess (BotGame::determinize), so it
//               plays well without ever peeking. It needs no strategy written
//               for it; it gets stronger the longer it thinks.
//
//   SeededRandom::Generator random = seed.stream("bots");
//   GameBots::Ismcts<CosmicHex> bot({.mistakePercent = 10});
//   Action move = bot.choose(state, random, {.iterations = 2000});
//
// Everything a bot does comes from the generator it's given, so the same
// state, generator and budget give the same move on every platform (unless
// a time limit cuts the search short, which depends on the machine).
#pragma once

#include <GameBots/BotGame.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace GameBots {

// How long a bot may think: at most `iterations` simulated games, and it
// stops early once `outOfTime()` says so (checked between games; the DS
// passes a check against its timer, so a move takes a second or two).
struct Budget {
  int iterations = 1000;
  std::function<bool()> outOfTime;
};

template <BotGame G> class RandomBot {
public:
  using State = typename G::State;
  using Action = typename G::Action;

  Action choose(const State &state, SeededRandom::Generator &random) const {
    const std::vector<Action> legal = G::legalActions(state);
    assert(!legal.empty());
    return legal[random.below(static_cast<std::uint32_t>(legal.size()))];
  }
};

template <BotGame G> class Ismcts {
public:
  using State = typename G::State;
  using Action = typename G::Action;

  struct Settings {
    // How much it tries moves it knows little about against moves that have
    // done well (the UCB1 constant; 0.7 is a common start for results in
    // 0 to 1)
    double exploration = 0.7;
    // Simulated games are cut off after this many actions, and judged by
    // G::estimate if the game has one (a draw if not)
    int rolloutDepth = 400;
    // Out of 100: how often it plays its second-best move instead of its
    // best, so an easy bot makes mistakes the way people do
    unsigned mistakePercent = 0;
    // A style: how likely each of the bot's own moves is in its rollouts,
    // weighed against the others (0 or more; empty: all equally likely).
    // Millington's AI for Games (3rd ed., 9.5.1) asks for playouts "a
    // little more accurate than choosing moves purely at random"; a bot
    // that imagines itself saving, rushing or exploring finds the moves
    // that suit doing so, and plays more that way. The other players'
    // rollout moves stay random.
    std::function<double(const State &, const Action &)> rolloutWeight;
  };

  Ismcts() = default;
  explicit Ismcts(Settings settings) : settings_(std::move(settings)) {}

  // The move it chooses for whoever's turn it is in `state` (not the
  // dealer's).
  Action choose(const State &state, SeededRandom::Generator &random, const Budget &budget) const {
    const int me = G::currentPlayer(state);
    assert(me != kDealer && !G::isOver(state));
    const std::vector<Action> legal = G::legalActions(state);
    assert(!legal.empty());
    if (legal.size() == 1)
      return legal.front();

    // The root: no move led to it, so its action is a placeholder that's
    // never read (only children's actions are)
    std::vector<Node> tree{Node{legal.front(), kDealer, -1, {}, 0, 0, 1}};
    for (int done = 0; done < budget.iterations; ++done) {
      if (done > 0 && budget.outOfTime && budget.outOfTime())
        break;
      simulate(tree, G::determinize(state, me, random), random, me);
    }

    // The most tried of the root's moves is the best (the most robust
    // choice); the second most tried is the "mistake"
    std::vector<int> ranked = tree[0].children;
    std::stable_sort(ranked.begin(), ranked.end(),
                     [&](int a, int b) { return tree[a].visits > tree[b].visits; });
    assert(!ranked.empty());
    const bool mistake = ranked.size() > 1 && settings_.mistakePercent > 0 &&
                         random.chance(settings_.mistakePercent, 100);
    return tree[static_cast<std::size_t>(ranked[mistake ? 1 : 0])].action;
  }

private:
  struct Node {
    Action action;       // the move that led here (a placeholder at the root)
    int mover = kDealer; // who made it
    int parent = -1;
    std::vector<int> children;
    double reward = 0;    // the total result, for `mover`, of games through here
    int visits = 0;       // games through here
    int availability = 1; // games in which this move was allowed (ISMCTS's
                          // replacement for the parent's visits in UCB1)
  };

  // The result of a (possibly cut-off) game for a player
  static double resultFor(const State &state, int player) {
    if (G::isOver(state))
      return G::score(state, player);
    if constexpr (HasEstimate<G>)
      return G::estimate(state, player);
    else
      return 0.5;
  }

  static int childFor(const std::vector<Node> &tree, int node, const Action &action) {
    for (int child : tree[static_cast<std::size_t>(node)].children) {
      if (tree[static_cast<std::size_t>(child)].action == action)
        return child;
    }
    return -1;
  }

  static int addChild(std::vector<Node> &tree, int node, const Action &action, int mover) {
    tree.push_back(Node{action, mover, node, {}, 0, 0, 1});
    const int child = static_cast<int>(tree.size()) - 1;
    tree[static_cast<std::size_t>(node)].children.push_back(child);
    return child;
  }

  // One simulated game from a guess at the hidden information: down the
  // tree by the best-looking moves (selection), one new move (expansion),
  // random moves to the end (the rollout), then the result back up the
  // moves taken (backpropagation).
  void simulate(std::vector<Node> &tree, State state, SeededRandom::Generator &random,
                int me) const {
    int node = 0;
    bool expanded = false;
    while (!expanded && !G::isOver(state)) {
      const int player = G::currentPlayer(state);
      if (player == kDealer) {
        // Chance: draw an outcome, and follow (or start) its branch
        const Action outcome = G::dealerAction(state, random);
        int child = childFor(tree, node, outcome);
        if (child < 0) {
          child = addChild(tree, node, outcome, kDealer);
          expanded = true;
        }
        G::apply(state, outcome);
        node = child;
        continue;
      }
      const std::vector<Action> legal = G::legalActions(state);
      // Moves allowed in this guess: count them as available, and find
      // the ones not tried yet
      std::vector<Action> untried;
      for (const Action &action : legal) {
        const int child = childFor(tree, node, action);
        if (child < 0)
          untried.push_back(action);
        else
          ++tree[static_cast<std::size_t>(child)].availability;
      }
      if (!untried.empty()) {
        const Action &action = untried[random.below(static_cast<std::uint32_t>(untried.size()))];
        const int child = addChild(tree, node, action, player);
        G::apply(state, action);
        node = child;
        expanded = true;
        continue;
      }
      // All tried: the one with the best upper confidence bound
      int best = -1;
      double bestScore = -std::numeric_limits<double>::infinity();
      for (const Action &action : legal) {
        const int child = childFor(tree, node, action);
        const Node &c = tree[static_cast<std::size_t>(child)];
        const double score =
            c.reward / c.visits +
            settings_.exploration * std::sqrt(std::log(static_cast<double>(c.availability)) /
                                              static_cast<double>(c.visits));
        if (score > bestScore) {
          bestScore = score;
          best = child;
        }
      }
      G::apply(state, tree[static_cast<std::size_t>(best)].action);
      node = best;
    }

    // The rest of the game, at random (the bot's own moves in its style,
    // if it has one)
    for (int depth = 0; depth < settings_.rolloutDepth && !G::isOver(state); ++depth) {
      const int player = G::currentPlayer(state);
      if (player == kDealer) {
        G::apply(state, G::dealerAction(state, random));
      } else {
        const std::vector<Action> legal = G::legalActions(state);
        const bool styled = player == me && settings_.rolloutWeight;
        G::apply(state, styled ? weightedPick(state, legal, random)
                               : legal[random.below(static_cast<std::uint32_t>(legal.size()))]);
      }
    }

    // Each move gets the result for the player who made it
    for (int at = node; at >= 0; at = tree[static_cast<std::size_t>(at)].parent) {
      Node &n = tree[static_cast<std::size_t>(at)];
      ++n.visits;
      if (n.mover != kDealer)
        n.reward += resultFor(state, n.mover);
    }
  }

  // One of `legal`, each as likely as its rolloutWeight (all equally if
  // they're all 0)
  Action weightedPick(const State &state, const std::vector<Action> &legal,
                      SeededRandom::Generator &random) const {
    std::vector<double> weights;
    weights.reserve(legal.size());
    double total = 0;
    for (const Action &action : legal) {
      const double weight = std::max(0.0, settings_.rolloutWeight(state, action));
      weights.push_back(weight);
      total += weight;
    }
    if (total <= 0)
      return legal[random.below(static_cast<std::uint32_t>(legal.size()))];
    double pick = random.unitDouble() * total;
    for (std::size_t i = 0; i < legal.size(); ++i) {
      if (pick < weights[i])
        return legal[i];
      pick -= weights[i];
    }
    return legal.back();
  }

  Settings settings_;
};

// Plays a whole game: each player's turn asks their chooser (any callable
// taking the state and returning an action: a bot, a person's input, a
// replay), and the dealer's turns draw from `dealer`. Every action goes into
// `play`'s log, so a simulated game can be saved, replayed and analysed like
// a real one. Stops after `maxActions` (a safety net for a game that
// doesn't end).
template <BotGame G>
void playMatch(
    ActionLog::Play<G> &play,
    const std::vector<std::function<typename G::Action(const typename G::State &)>> &players,
    SeededRandom::Generator &dealer, int maxActions = 100000) {
  for (int done = 0; done < maxActions && !G::isOver(play.state()); ++done) {
    const int player = G::currentPlayer(play.state());
    const typename G::Action action = player == kDealer
                                          ? G::dealerAction(play.state(), dealer)
                                          : players[static_cast<std::size_t>(player)](play.state());
    const bool allowed = play.play(action);
    assert(allowed && "a player chose a move the rules don't allow");
    (void)allowed;
  }
}

} // namespace GameBots
