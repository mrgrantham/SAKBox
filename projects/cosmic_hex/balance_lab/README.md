# Balance lab

Cosmic Hex's balance lab, step 1 of the plan agreed on the roadmap (round 35): choose a layout and the rules' settings, run a batch of bot games, and watch the results fill in as charts.

```bash
bazel run -c opt //projects/cosmic_hex/balance_lab
bazel run -c opt //projects/cosmic_hex/balance_lab -- --layout R2 --edges yes --games 200 --run
```

- **Settings** (left): the layout (from [`../layouts/layouts.txt`](../layouts/layouts.txt)), corners and edges, finds beside the small systems, players, games, how hard the bots think, points to win, a ship's move and fuel boost, the home-world boost, the Imperial Tithe, moons. Run starts a batch; Stop stops handing out games (those being played finish).
- **Charts** (right, [ImPlot](https://github.com/epezent/implot)): wins by turn position and by home, each against a fair share and the band a fair game's result falls in 95% of the time; how many rounds games take; where stations end up (singles, pairs, threes).
- **Saved as it goes:** every game is a line of `balance_lab_runs/<time>-<variant>.csv` in the repository (git-ignored), with the simulator's columns, so the command-line tools read it too.
- **Command line:** `--layout`, `--layouts FILE`, `--edges yes`, `--finds small`, `--games`, `--iterations`, `--players`, and `--run` to start at once.

How it works: the window is a padd on the main thread (`LabWindowPadd`), and eight `SimulationPadd`s each play a game when sent one (`PlayGame`) and reply with what came of it (`GamePlayed`); the window hands each the next game as it reports one done, so the cores stay busy and stopping is simply not handing out more. The games themselves are the simulator's, from [`../simulate/Batch.hpp`](../simulate/Batch.hpp): the same seed and settings give the same game in the simulator and the lab.

Next steps (not built): the board editor, replays of a game's log on the board, and suggestions.

## Tests

| Test | What it checks |
| --- | --- |
| `Tally_test` | Wins counted by turn position (from who went first) and by home, shares, the fair share's margin, rounds and stations by system |
