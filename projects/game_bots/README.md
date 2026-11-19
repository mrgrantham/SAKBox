# Game bots

Computer players for any turn-based game, and the means to play thousands of simulated games with them. Written for Cosmic Hex, where the bots are the single-player mode's opponents and also how the rules get tuned: they play game after game, and the results (who wins, from which seat, how long games run) show what to change. Header-only.

```cpp
#include <GameBots/Bots.hpp>

SeededRandom::Generator random = seed.stream("bots");
GameBots::Ismcts<CosmicHex> bot({.mistakePercent = 10});   // an easier bot
Action move = bot.choose(state, random, {.iterations = 2000});
```

## A game the bots can play

A game first describes itself for the [action log](../action_log/README.md) (`ActionLog::Game`: its state, its actions, and how an action changes the state, with chance outcomes as actions by "the dealer"). For the bots it adds a few more static members, checked by the `GameBots::BotGame` concept (`BotGame.hpp`):

| | |
| --- | --- |
| `playerCount(state)` | How many players, numbered from 0 |
| `currentPlayer(state)` | Whose turn it is, or `GameBots::kDealer` when chance comes next |
| `isOver(state)` | Whether the game has ended |
| `score(state, player)` | A finished game's result for a player, from 0 (lost) to 1 (won) |
| `legalActions(state)` | What the current player may do |
| `dealerAction(state, random)` | A chance outcome, drawn with its real odds from what's still possible |
| `determinize(state, viewer, random)` | The state with everything `viewer` can't see replaced by a guess that fits what they can see |

Optionally, `estimate(state, player)` judges an unfinished game (0 to 1), for simulated games cut off before their end. Without it, a cut-off game counts as a draw.

**`determinize` is what keeps the bots honest.** A bot never looks at the real hidden tiles or cards: before each simulated game it asks for a fresh guess at them, drawn from what's still unknown (tiles not yet turned over, cards not yet seen), exactly as a player would reason. For a game with nothing hidden it returns the state as it is.

## The bots

- **`RandomBot`** plays any allowed move at random. It's the baseline: any other bot must beat it.
- **`Ismcts`**, information-set Monte Carlo tree search (Cowling, Powley and Whitehouse, *Information Set Monte Carlo Tree Search*, IEEE TCIAIG 2012; the method Yannakakis and Togelius's *Artificial Intelligence and Games* recommends for games with dice and hidden information). It needs no strategy written for it. Each simulated game, from the current position:
  1. **Guess** what the bot can't see (`determinize`).
  2. **Select:** go down the tree of moves tried so far, taking the move with the best upper confidence bound (UCB1: how well it has done, plus a bonus for being little tried). In ISMCTS the bonus counts how often a move was *available*, since with hidden information not every move is possible in every guess. The dealer's turns draw an outcome and follow its branch.
  3. **Expand:** add one move not tried yet.
  4. **Roll out:** play the rest of the game at random (up to `rolloutDepth` actions).
  5. **Back up:** each move on the way gets the result for the player who made it.

  It then plays the move tried most often (the most robust choice).

### Settings and difficulty

- `Budget{.iterations, .outOfTime}`: how many simulated games, and an optional check (between games) that stops it early. On the DS this is a check against the timer, so a move takes a second or two.
- `Settings::exploration` (0.7): how much it tries little-known moves against ones that have done well.
- `Settings::rolloutDepth` (400): where simulated games are cut off.
- `Settings::rolloutWeight` (empty): a style: how likely each of the bot's own moves is in its rollouts (0 or more, against the others). Millington's *AI for Games* (3rd ed., 9.5.1) asks for playouts "a little more accurate than choosing moves purely at random"; a bot that imagines itself playing a certain way finds the moves that suit it. Other players' rollout moves stay random, and all-zero weights count as all equal.
- `Settings::mistakePercent` (0): how often, out of 100, it plays its second-best move instead of its best. **Difficulty is thinking time plus mistakes:** more simulated games play better, and easy bots now and then pick the second-best move, the way a person would (Fullerton, *Game Design Workshop*: computer players should play like people, mistakes included).

Everything a bot does comes from the generator it's given, so the same state, generator and budget give the same move on every platform. A time limit can cut a search short at different points on different machines.

### Trading

Negotiation is hard for any search. Cosmic Hex's bots will start with simple trade rules, written in its own project (accept an offer that improves their position by their own judgement; trade with the bank or a station when that helps), and simulations can run with trading on or off to see how much it matters.

## Simulated games

`playMatch(play, players, dealer)` plays a whole game. Each player's turn asks their chooser: any callable taking the state and returning an action, so a bot, a person's input or a replay. The dealer's turns draw from the `dealer` generator. Every action goes into the `ActionLog::Play`'s log, so a simulated game can be saved, replayed and analysed exactly like a real one:

```cpp
ActionLog::Play<CosmicHex> play(startingBoard);
GameBots::playMatch<CosmicHex>(play, {botA, botB, botC, botD}, dealer);
std::string record = ActionLog::write(play.log());
```

## Building

Bazel: depend on `//projects/game_bots` and include `<GameBots/Bots.hpp>`. It depends on [action_log](../action_log/README.md) and [seeded_random](../seeded_random/README.md), and needs C++23.

A DS or 3DS app built with devkitPro's Makefiles: add `"//projects/game_bots:sources": "game_bots"` (and action_log's and seeded_random's) to the app's `extra_srcs`, and their `include` folders to its Makefile's `INCLUDES`. On a console the bot should think on its own padd's thread, so the screen stays responsive while it does.

## Tests

`Bots_test`, with three small games (`tests/TestGames.hpp`), each testing one thing:

| Game | What it checks |
| --- | --- |
| Noughts and crosses (nothing hidden, no chance) | The random bot plays only allowed moves; ISMCTS takes a win, blocks a loss, and never loses to random play over 20 games, from either side; mistakes, when asked for, sometimes miss the win; a time limit stops the search |
| A dice race (chance: the dealer's rolls) | ISMCTS beats random play in at least three games out of four; the same state and generator give the same move; a simulated match is an action log that replays to the same end |
| A hidden number (hidden information) | The bot's guess is the same whatever the secret number is, and it's right about a third of the time, as a fair guesser is: it doesn't peek |

```sh
bazel test //projects/game_bots/...
```

## Memory

`Budget::frameArenaBytes` runs each simulated game in a frame arena (`ScratchPadd/FrameArena.hpp`): its copy of the state and its move lists come from one block, given back after each game, and the search tree grows outside it. It takes effect only with ScratchPadd's counting allocator linked (as Cosmic Hex's console apps do); the moves chosen are the same either way. On a 3DS the search's allocations fell from about 59,000 a second to about 40, and a bot plays about 2.4 times as many games in its 0.8 seconds (Azahar). `lastIterations()` and `lastArenaPeak()` say how many games the last move played and the most one held (about 75 KB in Cosmic Hex).
