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

It reports how often each seat wins (turn order's advantage), how many rounds a game lasts, where the winners' points came from (stations, colonies, Core Control, badges, contracts), and how many games didn't finish. Flags: `--games`, `--players` (2-4), `--iterations` (the bots' thinking: simulated games a move), `--rollout` (how far each of those runs before it's judged), `--max-rounds` (where an unfinished game is stopped), `--first random` (a random player goes first, each keeping their home, to tell turn order and home apart), `--seat-bonus a,b,c,d` (extra start resources for the 1st, 2nd, 3rd and 4th to play), `--empty N` (empty tiles of the zone's 12, 5 by default, the rest shared between salvage and ruins after the outposts and the pirate hideout), `--outposts N` (alien outposts among them, 2 by default), `--station-cost` and `--ship-cost` (five numbers, minerals, food, fuel, water, crystals: `--station-cost 0,0,0,0,0` for free stations), `--kinds` (how common each kind of planet is, five weights: `--kinds 1,1,2,1,1` for gas giants twice as common), `--exhaust N` (the exhausted home: at N points a player's home's chosen resource is its last, and it becomes a planet of that kind on 4, 5, 9 or 10, paying everyone by the dice; `--exhaust-kind random` draws its kind, an equal chance of each, instead), `--cartel all|N` (the Interstellar Cartel takes all of the named resource from each player, or up to N), `--home-steps 0,2,5,1` (the home's income in steps by points, from,amount pairs: here 2 a turn, then 1 from 5 points, as Starfarers' reserve pile; it takes the place of `--catch-up-below`), `--home-die yes` (the home's resources rolled on a six-sided die, one face per resource and the sixth the player's choice; round 66's hybrid with `--exhaust 5`), `--trading no` (no trading between players; it's on by default: after the roll the player whose turn it is offers trades to the others, the bots by the trading rule, `CosmicHex/TradingRule.hpp`), `--exhaust N` (the exhausted home: at N points a player's home's chosen resource is its last, and it becomes a planet of that kind on 4, 5, 9 or 10, paying everyone by the dice), `--strong ITERATIONS` (one stronger bot, at a different home each game, to check skill tells), `--variant LABEL` (a label for the CSV), `--catch-up-below N` and `--catch-up-gap N` (a player behind takes an extra resource with their home's income), `--discard-over N` (a hand limit: on a 7, a player holding more than N resources gives back half, from what they hold most of), `--skip-first yes` (the first player goes without their first home income), `--core seven|three` (the core planets: the 7 within 2 of the centre, or the smaller core: the centre and one opposite pair, or for three players the centre and the three a third of the way round), `--planets q,r,q,r...` and `--homes q,r,...` (a layout's planets and homes, such as round 44's cluster boards, instead of a planet every third hex and the usual homes), `--edges yes` (corners and edges, design.md: stations on corners, ships moving along edges, 2 a turn and 1 more for fuel unless `--move N` and `--boost N` say otherwise), `--start deep-space` (with `--edges yes`: each player's starting ship goes on any corner touching no planet, placed in reverse order, turning over the finds it lands by), `--layout NAME` (a board from `layouts/layouts.txt`: the round-45 layouts R1-R7 and S1-S6, each kind of system the same moves from every home, and round 44's E1-E17 and K1-K19; `--layouts FILE` for another file; the finds go beside the layout's singles and pairs, the rule since round 58, unless `--finds usual`), `--zone q,r,...` (where the deep-space finds are, instead of the usual 12 places), `--moons N` (moons among the face-down planets: a badge when turned over, no resource, can hold a station), `--win N` (points to win; the rules' default is 12), `--styles plain,saver,rusher,explorer` (bot styles, `CosmicHex/BotStyles.hpp`: the same search with the bot's own rollout moves leaning to saving, building at once or exploring; player p plays the ((p + game) mod count)th, so each style plays every home and turn position, and the summary gives each style's wins), `--seed`, `--logs FOLDER` to keep every game's action log for replaying or study, and `--csv FILE` for one line a game (its variant, seed, who went first, which seat had the stronger bot, the winner's seat, rounds, actions, everyone's points, where the winner's came from, and what the blockade, catching up and the hand limit did, the players' styles, the stations on singles, pairs and threes at the end, and every badge earned), to combine runs and analyse them; beside it, `FILE.contracts.csv` says how often each contract was dealt, kept, revealed, and revealed by the winner. Every game has its own seed, made from `--seed` and the game's number (so runs with different `--seed`s never repeat a game); it's in the game's log header and its CSV line.

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
