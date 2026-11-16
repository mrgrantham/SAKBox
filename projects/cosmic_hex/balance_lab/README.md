# Balance lab

Cosmic Hex's balance lab, step 1 of the plan agreed on the roadmap (round 35): choose a layout and the rules' settings, run a batch of bot games, and watch the results fill in as charts.

```bash
bazel run -c opt //projects/cosmic_hex/balance_lab
bazel run -c opt //projects/cosmic_hex/balance_lab -- --layout R2 --games 200 --run
```

- **Settings** (left): the layout as a thumbnail with its empty-space corners (where a deep-space start may put a ship), **Choose a layout...** for every layout as thumbnails (the repo's [`../layouts/layouts.txt`](../layouts/layouts.txt) and the lab's own saved ones), corners and edges (on by default), the deep-space start, finds beside the small systems, players, games, how hard the bots think, points to win, a ship's move and fuel boost (each showing the number the games use: 2 and 1 along edges, the rules' hex moves without; ticking corners and edges switches them unless you changed them), the home-world boost, the Imperial Tithe, moons. Run starts a batch; Stop stops handing out games (those being played finish).
- **The board** (middle): the chosen layout, large: H homes, P planets, the core's star, F the finds' places, and with the deep-space start the empty-space corners. Click a corner to see how far a ship gets from there in a turn (filled) and with fuel (rings), by the move and boost the settings give (along edges with corners, hex to hex without); it starts from the first home. **Edit planets and finds**: a click turns a hex from empty to a planet, a planet to a find, a find to empty (homes stay). **Save as a new layout** writes it to `projects/cosmic_hex/layouts/saved_layouts.txt` (commit it to keep it), where Choose finds it; a batch can also run an edited layout unsaved.
- **Charts** (right, [ImPlot](https://github.com/epezent/implot)): wins by turn position and by home, each against a fair share and the band a fair game's result falls in 95% of the time; how many rounds games take; where stations end up (singles, pairs, threes).
- **Saved as it goes:** every game is a line of `balance_lab_runs/<time>-<variant>.csv` in the repository (git-ignored), with the simulator's columns, so the command-line tools read it too.
- **Command line:** `--layout`, `--layouts FILE`, `--edges no` (the older hex-to-hex rules), `--finds small`, `--games`, `--iterations`, `--players`, `--run` to start at once, and `--choose` to open the layouts' thumbnails.

How it works: the window is a padd on the main thread (`LabWindowPadd`), and eight `SimulationPadd`s each play a game when sent one (`PlayGame`) and reply with what came of it (`GamePlayed`); the window hands each the next game as it reports one done, so the cores stay busy and stopping is simply not handing out more. The games themselves are the simulator's, from [`../simulate/Batch.hpp`](../simulate/Batch.hpp): the same seed and settings give the same game in the simulator and the lab.

Next steps (not built): replays of a game's log on the board (simulated games, and real ones once the game is playable: both are action logs), and suggestions.

## Tests

| Test | What it checks |
| --- | --- |
| `LabLayouts_test` | The repo's layouts read; a hex cycled (empty, planet, find, empty) with homes left alone; the usual finds' places; empty-space corners; a layout written, read back, and read by the batch |
| `Tally_test` | Wins counted by turn position (from who went first) and by home, shares, the fair share's margin, rounds and stations by system |
