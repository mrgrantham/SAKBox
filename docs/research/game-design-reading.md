# Game design reading: takeaways for our games

Status: reference. What the game design books in our collection say, gathered for designing [Infinifly](rail-ship-game.md) (the endless on-rails ship game) and the [hex-tile trading game](hex-trading-game.md), and to cite while we work on them. Each takeaway names its source, so we can go back to the chapter.

## What was read

| Book | Read | Mostly about |
| --- | --- | --- |
| Hunicke, LeBlanc and Zubek, *MDA: A Formal Approach to Game Design* (2004) | All | A way to talk about design |
| Elias, Garfield and Gutschera, *Characteristics of Games* (2012) | Ch. 2 (multiplayer), §4.2 (snowball and catch-up), ch. 5 (randomness, luck and skill, hidden information) | Multiplayer and board games |
| Salen and Zimmerman, *Rules of Play* (2003) | Ch. 15 (uncertainty), 17 (information), 18 (feedback loops), 24 (pleasure and flow), in part | Systems |
| Schell, *The Art of Game Design: A Book of Lenses*, 2nd ed. (2015) | Ch. 10 (flow), 13 (balance lenses), 16 (interest curves), in part | Practical lenses |
| Koster, *A Theory of Fun for Game Design* (2nd ed., and the talk slides) | Ch. 3, in part | Fun as learning |
| Yannakakis and Togelius, *Artificial Intelligence and Games*, 2nd ed. (2024) | Ch. 4 (playing games: MCTS, hidden information), ch. 7 (procedural content generation), in part | Bots and generated content |
| Fullerton, *Game Design Workshop: A Playcentric Approach*, 5th ed. (2024) | Ch. 5 (system dynamics: information, control, feedback), 10 (balance), 11 (player choices, fun killers) | Designing, prototyping, playtesting and balancing |

**Not read yet:** Millington's *AI for Games* (the folder copy is a DjVu file, and a preview PDF is there too) and the *Game AI Pro* preview. They're for later, when we build the bots. Millington's physics book is Cyclone's reference, not a design book.

## The ideas that matter most

### Talk about the experience first (MDA)

- **Mechanics** (the rules and code) give rise to **dynamics** (what happens in play), which give rise to **aesthetics** (what the player feels). Players meet the game from the feelings end, and designers build from the mechanics end. So decide the feelings first, then choose the mechanics that produce them.
- MDA's eight kinds of fun: sensation, fantasy, narrative, challenge, fellowship, discovery, expression, submission (a game as a pastime).
- Model the dynamics before building. MDA's own example is the 2d6 bell curve, used to predict how long a game lasts, and a feedback loop where the rich get richer, which makes a game drag on for the players who are behind.

### Fun is learning (Koster; Schell's flow channel; Rules of Play ch. 24)

- A game is fun while the player is still learning its patterns. It becomes boring when the pattern is mastered, or when it looks like noise (too hard, too fast).
- Flow: challenge has to keep pace with skill. Too much and it's anxiety; too little and it's boredom.
- Schell: a steady climb in difficulty is worse than **tense and release**: challenge rises, then a reward or a breather, then it rises again. That oscillation gives variety and anticipation.
- **Interest curves** (Schell ch. 16): open with a hook, then a slight drop, then rising peaks and dips, then a climax. Ask: what are the game's best moments, and in what order do they come?

### Multiplayer (Characteristics of Games, ch. 2)

- **Races and brawls.** In a race, players pursue their own goals and barely affect each other; in a brawl, players attack each other. Most board games are a race with some interaction added.
- **Elimination.** Being knocked out is bad, but being kept in a game you can no longer win (logical elimination) is often worse. The best answer is everyone having a chance until near the end, without the early game ceasing to matter.
- **Politics.** When players can freely choose whom to hurt (or help), the game drifts towards electing a winner: everyone piles on the leader, and skill matters less. The book names **trading** as a political mechanic for this reason. Ways to limit politics:
  - limit whom you can target, by geography (only your neighbours) or by turn order;
  - give in-game reasons to choose a particular target (they hold what you need);
  - reward absolute results, not relative ones.
- **Kingmaking.** A player who can't win decides who does. It needs targeted interaction plus logically eliminated players. Removing either one prevents it.

### Snowballing and catching up (Characteristics of Games §4.2; Rules of Play ch. 18)

- **Snowballing** happens whenever score equals power: money that buys more money. It ends games, which is good, but it's bad when the result is clear long before the end.
- **Fix:** keep victory points separate from what you spend, so a lead can't buy more lead. The cost is more rules (a second kind of counter).
- **Catching up:** randomness is itself a catch-up mechanism, since a random swing helps whoever is behind more. Over-strong catch-up makes early play pointless and gives the trailing player kingmaking power.
- **Positive feedback loops** (resources that build gatherers) bring a game to an end, but they need a damper.
- **Hidden dynamic difficulty adjustment** feels like cheating once players find out. Make help visible, or let players choose it.

### Randomness, luck and hidden information (Characteristics of Games ch. 5; Rules of Play ch. 15 and 17)

- Luck is uncertainty in the outcome; it isn't the opposite of skill. Poker has plenty of both.
- **What luck gives:**
  - a wider range of players can enjoy a game together (family and friends);
  - an "ego crutch": you can blame the dice for a loss;
  - more varied situations, so more replay value;
  - catch-up, and less calculation;
  - tension at the roll.
- **What luck costs:** expert players dislike it. People also see streaks that aren't there.
- **Kinds of hidden information:**
  - private information (your hand; fog of war that only you can see through);
  - puzzle-like secrets, which don't replay well unless they're regenerated each game, as maps are;
  - randomness, which is information hidden from everyone.
- **What hidden information gives:**
  - discovery and pacing as it's revealed;
  - surprise;
  - less calculation;
  - gameplay in deducing it and in planning around it;
  - bluffing.
- **Fog of war:** strategically unveiling a map adds suspense, and scouting becomes part of play (Rules of Play ch. 17).

### Balance lenses (Schell ch. 13)

- **Meaningful choices:** no dominant strategy.
- **Triangularity:** a safe, small reward against a risky, big one.
- **Skill vs. chance:** are players here to be judged (skill) or to take risks (chance)?
- **Competition vs. cooperation.**
- **Elegance:** each element should serve several purposes.
- **Economy:** how money is earned and spent, and whether those choices matter.
- The final test: does it *feel* right?

### Generated content and bots (AI and Games ch. 4 and 7)

- **Constructive generation** builds content once and must keep it correct as it goes. **Generate-and-test** builds content, checks it (is there a way through? is it fair?), and rebuilds it if it fails. Seeds make content repeatable, and parameters control it.
- **Experience-driven generation** shapes content to what the player experiences; Left 4 Dead's "director" is the example. Be careful with it: the dynamic difficulty adjustment caution above applies.
- **MCTS** suits board games with dice, many possible moves and hidden information. Its variant for hidden information is **ISMCTS** (information set MCTS). Bots also serve as playtesters: simulate thousands of games to tune the game's length and fairness.

### Balancing in practice (Fullerton ch. 5, 10 and 11)

- **Four things to balance:** the *variables* (costs, payouts, board size, lives), the *dynamics* (what happens when the rules run: loops, dominant objects, dominant strategies), the *starting positions* (fairness, including who goes first), and *skill* (matching challenge to the player).
- **Reinforcing loops:** an early lead that buys more lead ends games early. Remedies: a small, temporary bonus rather than a lasting one, a price for holding a strong position, chance, or letting trailing players gang up. But near the end, *let the scales tip*: a sweeping finish feels good to the winner and is quick for the losers. Don't drag out an ending.
- **Dominant objects and strategies:** no option should be best everywhere; give each a strength and a matching weakness. A *favourite* strategy that experts like but that can be countered isn't dominant.
- **Who goes first:** make the first move weak (chess), compensate later players (Go's komi), let the second player swap (the pie rule), make the game long enough that the first move hardly matters, or add chance.
- **Method:** keep subsystems separate, so a change shows its effect; give each part one purpose (strip parts out until the game breaks, to find the ones that do nothing); change **one thing at a time**; keep every variable in a table the game reads, so variants can be swapped while testing.
- **Kinds of decision:** avoid *hollow* (no consequence), *obvious* (no real choice) and *uninformed* (arbitrary) ones; aim for *informed*, *weighted* (a cost either way), *immediate* and *long-term* ones. Dilemmas with no optimal answer (the prisoner's dilemma: temptation > reward > punishment > sucker) make the best moments. Let the importance of decisions rise towards the end.
- **Rewards:** useful ones (towards winning) count most. Unpredictable rewards (a random schedule) hold attention for repetitive actions; for big choices, let players see and anticipate the consequences. Recognition by other players is a strong reward: show achievements as they happen.
- **Information:** open (calculation), hidden (bluff and reading people), mixed, or changing during play (fog of war). Which one decides what kind of thinking a game asks for.
- **Fun killers:** micromanagement (many small decisions that add up to one: merge them, or automate them); stagnation (repetition, or a balance of power where everyone always stops the leader); insurmountable obstacles (watch new players get stuck, and help just enough); arbitrary events (disasters with no defence feel like cheating); a predetermined path.
- **Skill:** set a high-water mark with expert players and a low-water mark with novices, and tune to the middle; or adjust as the player plays (Tetris speeds up with the score). A tuning variable for an opponent: Asteroids' small saucer fires at random inside a window a few degrees either side of the player, and the window narrows as the score rises.
- **Tuning for drama** (Stone Librande's *Us vs. It* exercise): tune an opponent so it is beaten at the most dramatic moment, by iterating, never by playing badly on purpose.
- **Playtesting:** fresh testers for usability, data gathering, and rapid iterative testing (fix and retest between sessions).

## Applied to Infinifly

- **The feelings it's for:** sensation (speed, the Tron look), challenge, and some discovery (a new course each seed). It's a race with one player, made social by ghosts and scores.
- **Difficulty:** step it up as a sawtooth, not a straight line. After each step, give a short breather or a restoring pickup, then the next climb. Each step should add **something new to learn** (a new obstacle or enemy pattern), not only more and faster, since fun is learning a pattern and boredom is having mastered it.
- **Score:** one headline score, so runs and ghosts compare clearly; that's the race's positional heuristic. Show the detailed stats after the run.
- **No hidden rubber-banding.** Restoring pickups as a visible level setting is the right kind of help.
- **The course:** constructive generation with checks built in (always a way through), from a seed, so ghost races are fair.
- **Interest curve per run:** a fast, exciting first ten seconds (the hook), then rising peaks, with enemy waves as the peaks.
- **Triangularity:** near misses and risky pickups pay more.
- **From Fullerton:**
  - The technique meter is a *weighted decision*: spend it on a dodge now or keep the score multiplier.
  - **Enemy aim as a tuning variable:** enemies fire inside a cone around the ship that narrows with each difficulty step (Asteroids' saucer). One number per step, easy to tune.
  - **Tune to the middle:** set the difficulty curve between an expert's run (the high-water mark) and a newcomer's (the low-water mark).
  - **No insurmountable obstacles:** the course generator checks that every chunk can be passed at the ship's speed, for every seed, in the tests.
  - **Avoid stagnation:** each stage changes the kind of action, not only its speed, and shows progress (distance, the next difficulty step).
  - **Bosses tuned for drama:** iterate until a good run beats a boss with little health to spare.
  - **Recognition:** near misses and technique called out on screen as they happen; ghosts and seeded races for comparing with friends.

## Applied to the hex game

- **The feelings it's for:** discovery (exploration is its identity), fellowship (playing with friends), and challenge, with the luck of 2d6 keeping a wide range of players in it.
- **Snowballing:** build-more-to-produce-more snowballs. Keep **victory points separate from resources**, and let exploration bring catch-up: a trailing player can still find a rich world.
- **Politics:** trading and blocking production are targeted interactions.
  - Keep fixed-rate trades with the bank or stations as a floor, so no one is at the table's mercy.
  - Limit blocking to tiles your own lanes reach (targeting limited by geography), not anyone anywhere.
  - Watch for kingmaking near the end. Hidden victory points (for example, secret contracts) blur who's leading, which reduces piling on.
- **Hidden information:** unexplored tiles are hidden from everyone, a regenerated puzzle that replays well because the map is new each game. Revealing a tile publicly makes exploring a race, and the explorer needs a reward for paying to explore, or players will wait for someone else to do it.
- **Length:** use the 2d6 model and bots playing thousands of games to tune it to under an hour for four players.
- **Bots:** ISMCTS, since the game has dice and hidden tiles.
- **The map:** generate-and-test, checking that each starting system's expected production is equal.
- **From Fullerton:**
  - **Reinforcing loop:** stations give resources that buy stations. The balancing pieces: the blockade ship on a 7, hidden contracts (a lead isn't visible), and Core Control passing to whoever overtakes. At the end, contracts revealed together let the scales tip quickly instead of dragging out.
  - **Who goes first:** identical homes make positions symmetrical except for turn order, which matters for the race to the core and the corner finds. The bots measure each seat's win rate; if the first seat wins too often, later seats start with an extra resource (like Go's komi).
  - **Informed, not uninformed, exploring:** the counts of each kind of tile (and the zone's half finds, half nothing) are printed, so flipping a tile is a gamble with known odds rather than an arbitrary pick.
  - **Weighted decisions:** settle (the ship is used up) or fly on to find more; contracts (draw 2, keep 1) pull each player somewhere different.
  - **Micromanagement:** moving many ships each turn on a small screen becomes a chore; a limit on ships each player has in play would keep turns short.
  - **Tuning:** every cost and payout in one rules table that the game and the bots read; variants swapped without code changes; one change at a time, each reported with the numbers behind it.
  - **Arbitrary events:** no random disasters without a defence; the blockade is moved by a player, on a roll everyone can see coming.
