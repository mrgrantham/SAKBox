# Cosmic Hex

An original space game of exploring, settling and trading on a hex board, for 2 to 4 players: you against bots on one device first, then friends over Wi-Fi and DS local wireless. It's an original game in the hex resource-trading genre, not a copy of an existing one. The design, with the reasoning behind each decision, is in [design.md](design.md).

It's built on SAKBox's libraries, as its own project: the [hex grid](../hex_grid/README.md), [seeded random numbers](../seeded_random/README.md), the [action log](../action_log/README.md) and [game bots](../game_bots/README.md), and later [ScratchPadd](../scratchpadd/README.md) for its screens and networking.

## What's here so far

| Folder | What it is |
| --- | --- |
| [`rules`](rules) | The rules, as plain C++ with no screens or padds (`CosmicHex/Rules.hpp`), described for the action log and the bots |
| [`simulate`](simulate) | Plays many bot games and reports what to tune |

## The rules

The rules engine's first version is the one in [design.md](design.md#the-rules-engines-first-version-2026-11-15). In short: ships explore a hex board of face-down planets; turning a planet over earns an explorer badge, and settling it makes a station that collects its resource when its number is rolled; stations become colonies; contracts are secret goals; first to 10 points on their own turn wins.

How the code is laid out (`rules/include/CosmicHex/Rules.hpp`):

- **`Settings`:** every number a rule uses, in one table: points to win, ship moves, costs, trade rates, how many of each tile, the dice numbers, the zone's finds. It's part of the state, so an action log replays under the numbers it was played with, and simulations can try other numbers without touching the code.
- **`State`:** the board (planets, the deep-space zone, ships), each player's resources, badges and contracts, what's left to draw, and where in a turn the game is (`Phase`).
- **`Action`:** everything a player or the dealer does: choosing the home's resource, the dealer's roll, moving the blockade, trading, building, moving a ship, the dealer turning a tile over, settling, colonies, contracts, cashing badges, ending the turn.
- **`Game`:** the rules for the [action log](../action_log/README.md) and the [bots](../game_bots/README.md): `apply` (an action, or false if the rules don't allow it), `legalActions`, whose turn it is, the dealer's draws, scoring, and the text a log keeps.
- **`newGame(players, settings)`**, **`points`**, and the **contracts** (`contracts()`: each a goal checked against the board and the player's tokens, and its points).

Two choices make the rules simple to reason about:

- **Face-down tiles aren't laid out at the start.** A tile is drawn from what's left of its pile when a ship turns it over, by the dealer. That's the same as a shuffled face-down tile, but nothing hidden sits on the board, so the state never holds a secret a bot could peek at.
- **The only hidden information is the contracts in each hand.** A bot's guess (`determinize`) deals the other players' contracts again from the ones it hasn't seen.

## Simulating

```sh
bazel run -c opt //projects/cosmic_hex/simulate -- --games 200 --players 4 --iterations 100
```

It reports how often each seat wins (turn order's advantage), how many rounds a game lasts, where the winners' points came from (stations, colonies, Core Control, badges, contracts), and how many games didn't finish. Flags: `--games`, `--players` (2-4), `--iterations` (the bots' thinking: simulated games a move), `--rollout` (how far each of those runs before it's judged), `--max-rounds` (where an unfinished game is stopped), `--first random` (a random player goes first, each keeping their home, to tell turn order and home apart), `--core seven|three` (the core planets: the 7 within 2 of the centre, or the centre and one opposite pair), `--seed`, `--logs FOLDER` to keep every game's action log for replaying or study, and `--csv FILE` for one line a game (its seed, who went first, the winner's seat, rounds, actions, everyone's points, and where the winner's came from), to combine runs and analyse them. Every game has its own seed, made from `--seed` and the game's number (so runs with different `--seed`s never repeat a game); it's in the game's log header and its CSV line.

## Tests

`bazel test //projects/cosmic_hex/...`

`Rules_test` checks:
- the board (19 planets, 4 homes, 7 core planets, the 12 zone tiles, with and without the corners variant) and the start;
- a turn's order;
- production, with colonies paying double and a blockaded planet paying nothing;
- the blockade on a 7, and its theft;
- moving up to 3 hexes, or 5 for a fuel, stopping on a face-down tile, which the dealer turns over, with a badge;
- settling, colonies, and at most two stations a planet;
- Core Control;
- contracts bought, dealt, kept and revealed;
- badges cashed in;
- the bank's 4 for 1;
- the zone's salvage and pirates;
- winning at 10 on your own turn;
- the other players' contracts hidden from a bot;
- the text of the starting state and of every action;
- two bots playing a whole game to a winner, whose action log replays exactly.
