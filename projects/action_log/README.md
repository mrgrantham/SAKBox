# Action log

A game kept as its starting state and the list of what happened, written as text. Saves, replays, recorded games to analyse, network sync and bots all build on it. Header-only, with no dependencies, and it knows nothing about padds: a game's rules run on their own, for bots and tests.

## The idea

A game's rules are a function from a state and an action to the next state. **Every chance outcome is an action too**, made by "the dealer": a roll, a tile flipped, a card dealt. So the log holds everything that happened:

- **A replay needs no random numbers.** It doesn't depend on the generator, and old logs keep replaying as long as the rules are the same.
- **The log reads like a record of the game,** which is what we want for analysing real games. A test scene can be written by hand, with exactly the rolls it needs.
- **Nobody can see ahead.** The seed makes the draws while playing, and only the live game, or a network game's host, has it. The host sends each outcome as it happens, private ones (a card dealt to you) only to their owner. Bots don't get the seed either: they guess what's hidden from what's still unknown, as a player would.

## A game

A game describes itself with a struct of static members, checked by the `ActionLog::Game` concept:

| | |
| --- | --- |
| `kName` | Its name in logs, with no spaces (`"cosmic_hex"`) |
| `kRulesVersion` | Bumped whenever a rule changes what an action does |
| `State`, `Action` | Its state, and its actions (the dealer's included) |
| `apply(state, action)` | Plays an action, or returns false and changes nothing if the rules don't allow it |
| `writeState`, `readState` | The state as text, any number of lines |
| `writeAction`, `readAction` | An action as text, one line |

The read functions return nothing for text that isn't one.

## Playing, writing, reading, replaying

```cpp
ActionLog::Play<CosmicHex> play(startingBoard);   // a fingerprint every 10 actions
play.addMeta("players", "Ann, Bot 1, Bot 2");
play.play(Roll{3, 5});                             // false if the rules refuse it
std::string text = ActionLog::write(play.log());

auto log = ActionLog::read<CosmicHex>(text);       // a Log, or a Problem (line, why)
auto again = ActionLog::replay(*log);              // again.state, again.played, again.problem
ActionLog::replay(*log, [&](const State &state, const Action &action, std::size_t index) {
  // watch the game go by, for analysis
});
```

`replay` checks as it goes: the rules version, that every action is allowed, and each fingerprint. It stops at the first problem, with the state as it was then and why.

## The text

```
action_log 1
game cosmic_hex 3
meta players Ann, Bot 1, Bot 2
start 2
<the game's own lines for its starting state>
<...>
a roll 3 5
a move 2 3,-1
f 2 9c1f0e2a7b33d401
```

- A header: the format's version, then the game's name and rules version.
- `meta` lines: notes, such as the players' names.
- `start N`: the starting state, in the game's own N lines.
- `a`: an action, one a line.
- `f`: a **fingerprint** (FNV-1a of the state's text) after that many actions, every 10 by default. A replay that comes out different, because a rule changed or two devices are out of step, is caught where it first differs rather than quietly becoming another game.
- Lines starting with `#` are comments, for notes made when analysing.

Logs made under other rules say so when replayed. Converting old logs is left for when it's needed. Saving and loading the text is the caller's job (ScratchPadd's StoragePadd, say).

## Building

Bazel: depend on `//projects/action_log` and include `<ActionLog/ActionLog.hpp>`. It needs C++23 (`std::expected`).

A DS or 3DS app built with devkitPro's Makefiles: add `"//projects/action_log:sources": "action_log"` to the app's `extra_srcs`, and `action_log/include` to its Makefile's `INCLUDES`.

## Tests

`ActionLog_test` uses a small dice game (roll into a pot, hold or keep rolling, a 1 loses the pot; first to 20), with the dealer's rolls drawn from a seeded stream:
- only allowed actions are recorded;
- a recorded game replays exactly, from the log alone;
- the text reads back as the same log and replays the same;
- the text has the format above;
- comments are skipped;
- an edited roll is caught by the fingerprints;
- other rules versions say so;
- what isn't a log is refused, with the line;
- a replay can be watched as it goes.

```sh
bazel test //projects/action_log/...
```
