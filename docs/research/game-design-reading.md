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

## Applied to Infinifly

- **The feelings it's for:** sensation (speed, the Tron look), challenge, and some discovery (a new course each seed). It's a race with one player, made social by ghosts and scores.
- **Difficulty:** step it up as a sawtooth, not a straight line. After each step, give a short breather or a restoring pickup, then the next climb. Each step should add **something new to learn** (a new obstacle or enemy pattern), not only more and faster, since fun is learning a pattern and boredom is having mastered it.
- **Score:** one headline score, so runs and ghosts compare clearly; that's the race's positional heuristic. Show the detailed stats after the run.
- **No hidden rubber-banding.** Restoring pickups as a visible level setting is the right kind of help.
- **The course:** constructive generation with checks built in (always a way through), from a seed, so ghost races are fair.
- **Interest curve per run:** a fast, exciting first ten seconds (the hook), then rising peaks, with enemy waves as the peaks.
- **Triangularity:** near misses and risky pickups pay more.

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
