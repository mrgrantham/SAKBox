# Cosmic Hex

An original space game of exploring, settling and trading on a hex board, for 2 to 4 players: you against bots on one device first, then friends over Wi-Fi and DS local wireless. It's an original game in the hex resource-trading genre, not a copy of an existing one. The design, with the reasoning behind each decision, is in [design.md](design.md).

It's built on SAKBox's libraries, as its own project: the [hex grid](../hex_grid/README.md), [seeded random numbers](../seeded_random/README.md), the [action log](../action_log/README.md), [game bots](../game_bots/README.md), and [ScratchPadd](../scratchpadd/README.md) for its screens.

## What's here

| Folder | What it is |
| --- | --- |
| [`rules`](rules) | The rules, as plain C++ with no screens or padds (`CosmicHex/Rules.hpp`), described for the action log and the bots |
| [`simulate`](simulate) | Plays many bot games and reports what to tune ([Simulating](#simulating)) |
| [`balance_lab`](balance_lab) | A desktop window for trying rule settings ([its README](balance_lab/README.md)) |
| [`solver`](solver) | Searches for fair boards ([The layout solver](#the-layout-solver)) |
| [`layouts`](layouts) | The boards, as text (`layouts.txt`) |
| [`handheld`](handheld) | What the console app plays and draws, tested on the Mac ([On a handheld](#on-a-handheld)) |
| [`ds`](ds), [`3ds`](3ds) | The console apps' builds |
| [`dice`](dice) | The physics dice ([Dice](#dice)) |
| [`art`](art) | The tiles', cards' and icons' SVG drawings ([Art](#art)) |
| [`models`](models) | The title screen's rocket, from Blender ([Models](#models)) |

## The rules

The rules engine's first version is the one in [design.md](design.md#the-rules-engines-first-version-2026-11-15). In short: ships explore a hex board of face-down planets; turning a planet over earns an explorer badge, and settling it makes a station that collects its resource when its number is rolled; stations become colonies; contracts are secret goals; the first to the target on their own turn wins.

How the code is laid out (`rules/include/CosmicHex/Rules.hpp`):

- **`Settings`:** every number a rule uses, in one table: points to win, ship moves, costs, trade rates, how many of each tile, the dice numbers, the zone's finds. It's part of the state, so an action log replays under the numbers it was played with, and simulations can try other numbers without touching the code.
- **`State`:** the board (planets, the deep-space zone, ships), each player's resources, badges and contracts, what's left to draw, and where in a turn the game is (`Phase`).
- **`Action`:** everything a player or the dealer does: choosing the home's resource, the dealer's roll, moving the blockade, trading, building, moving a ship, the dealer turning a tile over, settling, colonies, contracts, cashing badges, ending the turn.
- **`Game`:** the rules for the [action log](../action_log/README.md) and the [bots](../game_bots/README.md): `apply` (an action, or false if the rules don't allow it), `legalActions`, whose turn it is, the dealer's draws, scoring, and the text a log keeps.
- **`newGame(players, settings)`**, **`points`**, and the **contracts** (`contracts()`: each a goal checked against the board and the player's tokens, and its points).
- **`agreedRules()`:** the rules as agreed, which the app plays and the simulator's defaults are tested to match.

### The rules' source files

The rules are split by part of the turn (`rules/src`), sharing a private header, `RulesInternal.hpp`:

- **`Rules.cpp`:** `Game`: `apply` as a table of phases, each calling the file for its part of the turn; `legalActions`; the dealer's draws.
- **`Setup.cpp`:** the boards, `agreedRules`, `newGame`, the starting hands, and the start's placements.
- **`Production.cpp`:** the home's income, the roll and its payouts, the blockade and stealing.
- **`Trading.cpp`:** the bank, alien outposts, and offers between players.
- **`BuildingAndMoving.cpp`:** ships built and moved, stations and colonies (on corners, or on docks).
- **`Exploring.cpp`:** tiles turned over, the zone's finds, badges.
- **`Contracts.cpp`:** the contract deck: Charters and action cards.
- **`Scoring.cpp`:** points, the titles, and the game's end.
- **`Board.cpp`:** the lookups they share: the hex index, corners and their reach, whose pieces are where.
- **`Text.cpp`**, **`SettingsText.cpp`:** the state, actions and settings as the action log writes them.

Two choices make the rules simple to reason about:

- **Face-down tiles aren't laid out at the start.** A tile is drawn from what's left of its pile when a ship turns it over, by the dealer. That's the same as a shuffled face-down tile, but nothing hidden sits on the board, so the state never holds a secret a bot could peek at.
- **The only hidden information is the contracts in each hand.** A bot's guess (`determinize`) deals the other players' contracts again from the ones it hasn't seen.

The rules are tuned for the bots' search (measured with the `-watch` builds' search breakdown and the Mac's sampler, every simulated game unchanged), making the simulator 2.4 times as fast:

- ships' reach tabled at the start;
- a hex index in `State` for what's on a hex (made by `newGame`);
- the blockade's owners and points in one pass over the stations;
- their small hot helpers marked `[[gnu::hot]]`, which the DS build puts in ITCM.

## Simulating

```sh
bazel run -c opt //projects/cosmic_hex/simulate -- --games 200 --players 4 --iterations 100
```

It reports:

- how often each seat wins (turn order's advantage);
- how many rounds a game lasts;
- where the winners' points came from (stations, colonies, Core Control, badges, contracts);
- how many games didn't finish.

### Its defaults: the agreed rules

Each can be turned off or changed with the flag beside it.

| Default | To change it |
| --- | --- |
| No Core Control | `--core-control yes` |
| No fuel boost in the first round | `--first-boost yes` (as the rules once had) |
| Corners and edges | `--edges no` |
| The deep-space start | `--start home` |
| The home's income on the home die (2 a turn below 5 points; the home becomes a planet at 5) | `--home-die no`, `--catch-up-below 0`, `--exhaust 0` |
| The Imperial Tithe at 7 | `--discard-over 0` |
| Free stations (the ship costing minerals, fuel and water) | `--station-cost`, `--ship-cost` |
| Finishing the round | `--finish-round no` (end at once) |
| Trading between players | `--trading no` |
| Finds beside the small systems | `--finds usual` |

### Flags

Running:

- `--games N`, `--players N` (2-4), `--seed N`
- `--iterations N`: the bots' thinking, in simulated games a move
- `--rollout N`: how far each of those runs before it's judged
- `--max-rounds N`: where an unfinished game is stopped
- `--arena BYTES`: the bots' frame arena (128 KB by default, about 18% faster on a Mac, the same games; `--arena 0` for none)
- `--variant LABEL`: a label for the CSV

Who plays:

- `--first random`: a random player goes first, each keeping their home (to tell turn order and home apart)
- `--strong ITERATIONS`: one stronger bot, at a different home each game (does skill tell?)
- `--strong N --others random`: one bot thinking N, the others choosing any legal move at random (does thinking beat chance?)
- `--styles plain,saver,rusher,explorer`: bot styles (`CosmicHex/BotStyles.hpp`), the same search with the bot's own rollout moves leaning to saving, building at once or exploring. Player p plays the ((p + game) mod count)th, so each style plays every home and turn position, and the summary gives each style's wins

The board:

- `--layout NAME`: a board from `layouts/layouts.txt` (R1-R7 and S1-S6, each kind of system the same moves from every home; E1-E17 and K1-K19); `--layouts FILE` for another file. The finds go beside the layout's singles and pairs, unless `--finds usual`
- `--planets q,r,q,r...` and `--homes q,r,...`: a layout's planets and homes, instead of a planet every third hex and the usual homes
- `--zone q,r,...`: where the deep-space finds are, instead of the usual 12 places
- `--core seven|three`: the core planets (the 7 within 2 of the centre, or the smaller core: the centre and one opposite pair; for three players, the centre and the three a third of the way round)
- `--edges yes`: corners and edges ([design.md](design.md)): stations on corners, ships moving along edges, 2 a turn and 1 more for fuel unless `--move N` and `--boost N` say otherwise
- `--start deep-space` (with `--edges yes`): each player's starting ship on any corner touching no planet, placed in reverse order, turning over the finds it lands by
- `--choose-home no`: each seat's home fixed by the board (by default each player chooses a home at the start, in turn order, putting their home colony on a corner of any home not yet taken)
- `--empty N`: empty tiles of the zone's 12 (5 by default; the rest shared between salvage and ruins, after the outposts and the pirate hideout)
- `--outposts N`: alien outposts among them (2 by default)
- `--moons N`: moons among the face-down planets (a badge when turned over, no resource, can hold a station)
- `--kinds a,b,c,d,e`: how common each kind of planet is (`--kinds 1,1,2,1,1` for gas giants twice as common)

Costs and income:

- `--station-cost` and `--ship-cost`: five numbers, minerals, food, fuel, water, crystals (`--station-cost 0,0,0,0,0` for free stations)
- `--seat-bonus a,b,c,d`: extra start resources for the 1st, 2nd, 3rd and 4th to play
- `--start-cards N`: a starting hand of N resources dealt at random to each player (by the dealer, one at a time round the table from the first player), instead of 1 of each
- `--home-die yes`: the home's resources rolled on a six-sided die, one face per resource and the sixth the player's choice (with `--exhaust 5`, the agreed hybrid)
- `--home-steps 0,2,5,1`: the home's income in steps by points, from,amount pairs (here 2 a turn, then 1 from 5 points, as Starfarers' reserve pile); it takes the place of `--catch-up-below`
- `--catch-up-below N` and `--catch-up-gap N`: a player behind takes an extra resource with their home's income
- `--exhaust N`: at N points a player's home's chosen resource is its last, and the home becomes a planet of that kind on 4, 5, 9 or 10, paying everyone by the dice; `--exhaust-kind random` draws its kind instead, an equal chance of each
- `--skip-first yes`: the first player goes without their first home income

Rules:

- `--win N`: points to win (the rules' default is 12)
- `--finish-round yes`: reaching the points to win plays out the round, so everyone has had as many turns; the most points wins, a tie going to the later in turn order
- `--discard-over N`: a hand limit (on a 7, a player holding more than N resources gives back half, from what they hold most of)
- `--cartel all|N`: the Interstellar Cartel takes all of the named resource from each player, or up to N
- `--trading no`: no trading between players (on by default: after the roll, the player whose turn it is offers trades to the others, the bots by the trading rule, `CosmicHex/TradingRule.hpp`)
- `--charters a,b,c`: the Charters in the deck, by their number in `contracts()` (21,19,20 by default: stations on 3 planets of one kind, Occupier, the relay; 0 is the older 3 kinds of planet)
- `--relay-points N`: what the Deep-space relay Charter itself scores once revealed, on top of its station's 1 (1 by default)
- `--dominion N`: Imperial Dominion's points, to whoever has the most colonies on other players' former homes (0, off, by default)

Output:

- `--logs FOLDER`: every game's action log, for replaying or study
- `--csv FILE`: one line a game, to combine runs and analyse them: its variant, seed, who went first, which seat had the stronger bot, the winner's seat, rounds, actions, everyone's points, where the winner's came from, what the blockade, catching up and the hand limit did, the players' styles, the stations on singles, pairs and threes at the end, and every badge earned. Beside it, `FILE.contracts.csv` says how often each contract was dealt, kept, revealed, and revealed by the winner

Every game has its own seed, made from `--seed` and the game's number (so runs with different `--seed`s never repeat a game); it's in the game's log header and its CSV line.

## On a handheld

One app runs on the 3DS and the DS: `handheld/source/` (`main` in `cosmic_hex_on_a_handheld.cpp`, its parts in [The app's files](#the-apps-files)), the differences in `#if` blocks and in each platform's `PlatformProfile`.

### Build and run

| Command | What it does |
| --- | --- |
| `bazel run //projects/cosmic_hex/3ds:cosmic-hex-3ds` | Builds the 3DS app and opens it in Azahar (Docker must be running: see `tools/devkitpro`); `tools/fbi_install` installs it on a 3DS |
| `bazel run //projects/cosmic_hex/ds:cosmic-hex-ds` | Builds the DS app and opens it in melonDS |
| `cosmic-hex-3ds-watch`, `cosmic-hex-ds-watch` | Start watching four bots by themselves, for unattended runs, with what each frame costs and the memory on the top screen |

The console apps need Blender to build (tagged `blender`: [Models](#models)).

The DS's version differs in:

- its bots thinking less (no floating-point hardware);
- the tiles' faces as 32-pixel textures (its video memory is small), drawn with coarser smoothing (2 x 2 samples), one a frame during the title and setup screens;
- the flat board on the bottom screen as a 16-bit bitmap in VRAM bank C (256 x 192).

### The screens

`ScreenFlow` runs them:

1. **The splash** (ScratchPadd's `StartupSplash`: "Vibe Coding by James / presents") while every picture is drawn from its recipe; then the title fades in.
2. **The title:** the title scene on top (ScratchPadd's `title_scene`: the title in blocky outlined letters, the rocket from `models/` turning on its hexagonal pedestal, the night sky turning slowly), Play, Settings and Quit below.
3. **The setup** (`StartMenu`, drawn by `MenuPicture`): [below](#the-setup).
4. **The game:** who goes first, the start's placing, then turns. START opens the pause menu (Resume, Restart, Title, Quit); paused shows as a still banner in a neutral grey.
5. **The win:** the winner's ships fly round above the board, flames burning, with who won in their colour (with fewer than three ships, the rest fade in at their colonies and take off: `WinCelebration`; A or B skips it). Then the title scene comes back with who won.

Touch or the buttons work everywhere. The buttons are ScratchPadd's `Buttons` panels, and Confirm and Back follow the pad's own convention (`Input::Role`).

### The setup

On the touch screen:

- **Players:** four (you and three bots) or three (you and two).
- **Board:** `boardChoices()` for that many players, the default first (R3 for four, T4 for three); the four-cluster boards FR1, FR3 and FS1 last, the top screen noting their longer games.
- **End:** finish the round, or the first to the target wins at once.
- **Seat bonus:** R3's (off unless chosen).
- **Mode:** you play, or watch the bots.

The top screen shows the chosen board from straight above, lit flat like the map. A new board slides in on the dice's curve (speeding up as the dice's glass does when it's pulled away, then slowing the same way: `slideAcross`, the Motion library's `Pull` curve). L and R change the board whichever row is in focus. Start begins the game, Back returns to the title.

### Starting a game

1. **The board comes down** to the game's view on the dice's curve, while the bottom screen fades from the menu to the map.
2. **A moment to look at it:** 2 seconds (A goes on at once).
3. **Who goes first** fades in over it (`FirstRollShow::appeared`). Each player rolls a die, those tied for the highest rolling again (`TableGame::firstRoll`), thrown with the physics dice (`Dice::DiceInTurn`):
   - each player's die, smaller than the roll's and in their colour, goes down its own lane from the top under their name, walls between the lanes (`DiceLanes`), settling on its number before the next is thrown (A throws the next at once);
   - the number slides up into place under the name and settles (clipped to a window, `Overlay::clipText`);
   - those tied throw again in a fresh glass;
   - then each player hops into their place in the order (a wind-up, a hop, an overshoot, one after another; those already in their place hop where they are, so the order is always shown), 1st to 4th under them.
4. **The start's placing:** in turn order each player chooses a home not yet taken and puts their home colony on one of its corners; then, back again, their ship on a corner in open space. On your turn the places are marked, a touch shows the piece there, and End Turn (or A) places it (B, or touching it again, takes it back).

### Playing a turn

The bottom screen (`BottomScreen`) is the board with its buttons (`BoardButtons`, icons from `art/icons`), each only when it can be used:

| Button | Where | What it does |
| --- | --- | --- |
| Show resources | top left | Your cards on the top screen, sliding in, and back off when closed |
| Other players | top left | A tab for each other player; the top screen shows what everyone can see of them ([below](#the-top-screen)) |
| Build | top right | The build screen: each row buildable, or red with why (`BuildMenu`, from the rules' own legal moves) |
| Trade | top right | The trade room ([below](#trading)) |
| Cards | top right | Your action cards and Charters, what each does and why one can't be used now (`CardChoices`): Play plays one, Reveal shows a Charter |
| Roll | bottom left | Throws the dice |
| End turn | bottom right | Ends the turn |

On the board:

- **Touch a corner** to select it (a small ring on both screens; the hex, outlined, when moving the blockade). A touch picks the corner nearest it.
- **The board's pointer** (`CornerPointer`): a white chevron, turning and bobbing over the corner in focus on the top screen. It fades in gently when the game wants you to choose on the board (placing at the start, where a build goes, a ship's moves) and fades away while pieces move, banners and shows play and turns end; it glides from corner to corner as the focus moves. It points at the ship touched, else what you've touched, else the place nearest the board's middle.
- **Move a ship:** touch it, and the pointer goes over it, and where it can go is marked on both screens, the moves that take a fuel told apart (orange rings on the top screen, outlined squares on the map; the rest highlight-coloured rings and filled dots). Touch a marked corner to move it. After the roll the turn's offers to players are open (the trading phase), and a ship can still be moved: choosing where ends the offers first, as building does. A ship that can't move says why (roll first, or it has moved this turn).
- **Drag** to move the board when zoomed in; **X held with R and L** zooms in and out (L and R alone are left and right on a choosing screen).
- **B** takes whatever is animating straight to its end ([Animation](#animation)).

The choosing screens (`ChoiceScreens`, drawn on a `Canvas`) take the board's place when needed:

- **The resource picker** (`Pickers`): the home's pay and salvage open it by themselves; the Cartel and the Supply drop open it from the cards screen. The card in focus is the pick, and Take always shows (A takes it at once).
  - **Each choice opens afresh:** a home paying 2 is two choices in a row, each opened anew (nothing carried over) and saying how many are left ("(2 to choose)"; `ResourceChoice::left`, `sameAs`).
  - **The top screen:** the CHOOSE A RESOURCE banner first, for each choice; once it's gone, the card in focus big, sliding in from the side you moved to, its name fading in, with what you could build with it, dimmed if even with it you couldn't (`PickShow`, `drawPick`).
- **The player picker:** whom to steal from, when the blockade stands by more than one player.
- **Keeping a card:** after buying, the two cards dealt are thrown onto the top screen and turned over one after the other, their words on them, and the choice opens by itself.
- **Shipyard Surplus** is played by touching a colony.

Debug builds keep the old list of moves behind a button (under Other players), off at first.

### Trading

The trade room (`TradeRoom`):

- a tab for the bank and each player (players while the turn's offers are open);
- your cards to give and get (a touch adds one, a minus takes one back), your own on the lower row, as a hand is held;
- the deal in words, and why not;
- Trade with the bank (ending the turn's offers to players first), or Offer to a player;
- an offer to you, or a counter to yours, opens it by itself, with Accept and Decline, a counter's changes in amber.

Choices are marked in the palette's highlight colour.

### The top screen

The board in 3D (stereo with the 3D slider on a 3DS), with the HUD over it (`Hud.hpp`):

- each player's points as a row of dashes (`PointsBars`: a dash a point in their colour, grey to the target); the turn's row outlined in turquoise, the outline easing from row to row as turns change;
- a point or badge just won pops in, growing past its size in white and settling, several one after another (`ScorePops`);
- the round and your badges in the corners;
- the last move in words;
- who won.

Each tile shows its face, and a turned-over planet its number on a round chip ([Art](#art)). Players have names (`PlayerNames`): yours is your console's owner name ("You" when it has none), the bots' are stars, a different few each game; banners, tabs and the trade room say names. The game's names for its things are in one table (`GameWords`).

**Another player's panel** (`OtherPlayers`): points, cards face down, badges, ships, stations and colonies (drawn as the board's pieces), the planets their stations touch with their numbers, revealed Charters.

- **Opening and closing:** it slides down and back up (0.45 s, the Gentle curve); on a DS the board under it holds frames near 20 a second while it moves, and once it covers the screen the board isn't drawn.
- **Another player:** chosen with the tabs, or left and right (whoever's turn it is: a screen to look at takes the D-pad any time), the last one lifts away and the next comes down (0.3 s; `PanelShow::drawn`).
- **Paused while open:** Show resources and this panel hold the game, and a grey PAUSED tag at the top says so ("close to go on").

### Animation

Each thing shown over the board for a while is a show: its state, timing and whether it's moving in `TopShows.hpp`, tested on its own; its drawing in `TopViews.hpp` (`TopViews_test` draws each through its whole timeline). Their motion follows [`docs/research/animation-whimsy.md`](../../docs/research/animation-whimsy.md): arcs, a wind-up before, a settle after.

On the board (`BoardMotion`: what moved between two moves, part way):

- **Ships** fly from corner to corner like small rockets: a shallow arc, nose first, an engine flame while they speed up, swivelled to steer them round the arc, then coasting in and keeping that heading.
- **The blockade** swoops from planet to planet, up off the board and diving onto the next, its flame burning.
- **A station or colony built** drops in, squashes as it lands and springs back, a ring of dust spreading from it and fading.
- **Planets and anomalies** flip over when explored, over a sparse starfield.
- **A stolen card** flies from the victim to the thief (face down unless it's yours either way); **a trade's cards** cross between the two players, or to and from the bank (off the top of the screen).
- **Resources spent** (building, buying a card, a bank trade) fly to the ROUND hexagon and shrink away behind it.

A snapshot that moves nothing (a hold or a choice) leaves what's moving going.

Over the board:

- **A roll** (`RollDice`: the throw, fixed steps on the game's clock, a watchdog, the pull, skipping; drawn by `drawRoll`): the physics dice ([Dice](#dice)) thrown in front of the board (`DicePieces`: the dice's mesh, and the glass container placed facing the camera). The game waits (`TableGame`'s `physicsDice`) until they settle and plays the faces they show; the roll pops in on a hexagon in the middle of the screen as they settle, and the container is pulled off the screen, the roll going with it. The dice are 1.2 across, grey, their corners rounded (a chamfered cube, 44 triangles and the pips); dice still moving after 8 seconds are thrown again.
- **A home's pay** (with the home die on): its die, an extruded warm-grey hexagon with a resource's colour on each side and the star on the sixth (`homeDie`, `DicePieces`), springs out of the home rolling about its long axis and lands with the chosen side facing you; the card shows big, then flies to the hand. The game and the main dice wait for it.
- **A tile turned over** (`Discovery`): the board's own tile lifts off and flies up to face you, face down, turns over; a planet's number token drops onto it (its number then written sharp over the token's picture, as a small texture's digits blur); its name and what it gives are written beside it; it flies back to lie flat in its place, all on the `Gentle` curve. The game is held meanwhile, and the bottom map shows the tile only after, fading in.
- **A card played, or a Charter revealed** (`CardPlayShow`): it rises from its player, turns over big in the middle to be read, and flies off.
- **Cards gained** show your hand as Show resources does, the new ones dropping into the backs of their piles once nothing that brings them is still showing (`HandShow`).

Words over a card or tile (its name, a token's number, a card's words) wait for it to settle, then fade in (`kSecondsWordsIn`).

Pictures shown big for a moment (a discovered tile, both its back as it flies up face down and its face, the cards dealt, played or paid) are drawn again at `PlatformProfile::closeUpSize` (128 on both consoles) just before they're shown, while nothing moves, into textures kept for them. On a DS they're drawn 16 rows at a time (`closeUpBandRows`), as a picture drawn whole needs more main memory than a game has free.

Moves animate and the dice roll on the game's own clock (ScratchPadd's `GameClock`), which stops while a watched game is paused; the shows run on a clock of their own at the Animations setting's pace. B (Back) takes whatever is animating (the dice, who goes first, pieces moving, the turn's banner, a turned-over tile, a pay, the cards dealt coming in) straight to its end.

### Settings

From the title, kept on the card:

- **Your name** (`name.txt`): your console's owner name at first; Change types another on ScratchPadd's `OnScreenKeyboard`.
- **Animations** (`animations.txt`): Full, Quick (twice as fast) or Off (each straight to its end).

### The bottom map

The bottom screen shows the board flat (`BoardPicture`: the same faces, the pieces as shapes).

- **Drawing only what changed:** each piece is listed with its look and area, and the areas where pieces came, went or changed are drawn again from scratch (the buttons' areas drawn again under them); only the rows that changed are copied to the screen. `BoardPicture_test` checks it matches drawing it all, over whole games.
- **Zooming shows at once:** the whole board's faces, kept, scaled up, sharpening a face a frame as the zoom's own are drawn.
- **Only the faces on screen are drawn sharp,** as many as fit the platform's budget (`PlatformProfile::boardZoomBytes`: 112 KB on a DS, eight faces at zoom 2), the one seen longest ago let go for one coming into view (a cache, least recently used first); the rest show scaled up until there's room. All 19 faces at zoom 2 took 240 KB on a DS, and the game ran out of memory and stopped.
- **Zooming stops** at the closest zoom where the chips and six faces fit (zoom 2 on a DS).
- **Where memory is counted** (a DS), sharp faces are drawn only while they leave `boardZoomReserve` free, and let go if less than half of it is left; the `-watch` builds show the room left.

### Performance on the DS

Measured with the `-watch` builds' frame profile (`FrameProfile`: the bottom screen, its copy to the screen, the dice, the top screen and its wait for the refresh, the board's pieces, the renderer's CPU time and its overlay's, and the bottom map and buttons):

- **A drawing pass:** the text drawn in whole numbers (the overlay's worst frame 40 ms to 8), fewer divides in placing each mesh, the board's places kept, colours word-aligned, and the bottom screen copied a word at a time (its worst frame about 100 ms to 45). Quiet seconds went from 23-33 frames a second to about 55.
- **An unchanged top screen isn't drawn again:** the 3D hardware shows its last frame by itself, and the time goes to the bots (drawing a still board each frame left them 4% of the CPU; in melonDS they went from about 25 games a move to about 130).
- **The dice's physics steps 30 times a second** (60 elsewhere), just as fair (`dice_fairness_flex_point16` over 20,000 throws: faces chi-square 1.63, sums 11.28; `dice_fairness -- THROWS STEPS_A_SECOND`). Rolls run at 22-26 frames a second rather than 8-14.
- **The bots search with a frame arena** of 128 KB (GameBots' `frameArenaBytes`); the `-watch` builds show how many games a move they play.
- **Memory:** with ScratchPadd's counting allocator in the build, the app measures its memory every 2 seconds (the heap, and allocations a second by padd), logged in a debug build and shown in the `-watch` builds.
- **The DS's depth buffer is coarse:** the board's thin layers (the number chips, the chosen hex's ring) sit high enough, the cameras' near plane far enough out, and the grid between the tiles lies only in the gaps. `BoardView_test` checks them against a model of that buffer (the far find tiles had lost triangles to the grid under them).

### The code

What the app plays and draws is in `handheld/`, tested on the Mac:

| Part | What it does |
| --- | --- |
| `TableGame` | Seats for people and bots, the dealer, the action log, each move in words |
| `BoardPieces` | Where hexes and corners are, and the pieces' meshes and colours |
| `BoardMotion` | What moved between two moves, part way |
| `BoardView` | The top screen's cameras |
| `BoardPicture` | The bottom map |
| `BottomScreen`, `BoardButtons`, `ChoiceScreens`, `BuildMenu`, `Pickers`, `CardChoices`, `TradeRoom` | The bottom screen and its choosing screens |
| `Hud`, `RollDice`, `TopShows`, `TopViews` | What's over the board on the top screen |
| `PlatformProfile` | Each console's numbers (texture sizes, memory budgets, the dice's step) |
| `GamePictures` | The pictures the top screen draws with, loaded once at the start; `GamePictures_test` loads them as the DS would, its 256 KB of texture memory emulated, with room left for toon textures |

### The app's files

The app itself is in `handheld/source/`, built by the consoles' Makefiles (every file there). Its parts that need no console (`app_parts`) are also built on the Mac, so clang-tidy checks them.

| File | What it holds |
| --- | --- |
| `cosmic_hex_on_a_handheld.cpp` | `main`: the padds started |
| `TablePadd`, `TableMessages.hpp` | The game on its own thread (TableGame, the bots, the dealer), and what it and the screens say to each other |
| `ScreenPadd.hpp` | The screens' padd, declared; its members defined by role in the files below |
| `ScreenPadd.cpp` | Starting, the frame loop, holding the game, the animations' pace |
| `GameFlow.cpp` | A new state's arrival, starting and stopping games, who goes first, the dice, the win |
| `BoardDrawing.cpp` | The stage's scenes: the title, the setup's board, the game's board and its pieces |
| `ShowsOverBoard.cpp` | What shows over the board: the HUD, banners, close-ups, the shows' updates, the pointer |
| `TouchPanel.cpp` | The touch panel's picture, the buttons and touch, your moves |
| `MenuScreens` | The title, pause, setup and settings menus, your name and the animations setting |
| `ScreenLayout.hpp` | Where each role draws, and its size |
| `AppBasics.hpp`, `MemoryReadout` | The build's switches and timings; memory measured for the `-watch` builds |

### Toward one screen

The app draws by role, not by screen (`ScreenLayout.hpp`): the **stage** (the 3D scene: the title, the boards, the shows over them; the top screen) and the **touch panel** (the menus, the flat map and its buttons; the bottom screen). A one-screen version would place both on its one screen:

- **Unchanged:** `TablePadd` and its messages, `GameFlow.cpp`, `MenuScreens`' choices and what they keep, the shows' timing and holds.
- **The stage** (`BoardDrawing.cpp`, `ShowsOverBoard.cpp`) draws through the renderer at `kStageWidth` x `kStageHeight`: on one screen, into its part of the screen (the renderer would need a viewport, or the stage takes the whole screen with the touch panel over it).
- **The touch panel** (`TouchPanel.cpp`, `MenuScreens`) is a picture drawn by the CPU (`BoardPicture`, `Canvas`, `MenuPicture`) and copied to the bottom screen (`ConsoleScreens::BottomScreen`): on one screen it would go to a texture (`Renderer::replace`, as the close-ups do) drawn over or beside the stage, only when it changes.
- **`ScreenLayout.hpp`** would give each area a place (x, y) as well as a size, and the touch handling (`handleTouch`, `MenuScreens::tap`, `scripted`) would take the touch panel's place off each touch.
- **Holding a still frame** (`PlatformProfile::holdStillFrames`, the DS's top screen) would have to count the touch panel's changes too, as both would be one frame.
- **The menus** on the title, setup and settings screens would cover part of the scene behind them, or take the screen in turn with it.

## The layout solver

```sh
bazel run -c opt //projects/cosmic_hex/solver:solve_layouts -- --threes 2 --away 3 --farther yes
```

It searches for boards that meet the constraints you set and prints the best as lines of a layouts file (add them to `layouts/layouts.txt` and simulate with `--layout NAME`). Every board it makes:

- has clusters that never touch each other, the core or a home;
- is the same seen from opposite homes;
- treats the centre like any other hex (any cluster may cover it, or nothing), unless you ask for a lone planet there.

The constraints:

- **Counts:** `--singles`, `--pairs`, `--threes`, `--fours` (a compact diamond of four) and finds, each a value or a floor-ceiling range (`--threes 2`, `--pairs 1-3`).
- **Homes:** on the edge (as the R layouts) or the corners (`--homes corners`, as the S).
- `--centre lone`: a lone planet at the centre.
- `--away N`: finds never touch a cluster of N or more.
- `--farther yes`: from every home, the nearest three, and four, is farther than the nearest pair and single.
- `--nearest-single`, `--nearest-pair`, `--nearest-three`, `--nearest-four A-B`: from every home, that cluster within A to B ship moves.
- `--reach-moves D --in-reach A-B`: every home has A to B planets within D moves.
- `--same-near D`: every home has the same number of planets within each distance up to D moves (unequal near planets were found to make a board unfair where equal access overall didn't show it).

The search: `--attempts N` (boards tried, 20,000 by default), `--keep N` (the best this many, different from each other, 12 by default), `--seed N`, and `--name PREFIX` (the printed layouts' names, V by default).

Distances are a ship's moves along edges from a home's corners. Results come ranked by how equal every home's access to planets is (planets weighted by 1 / (1 + moves)). The library is `solver/` (`LayoutSolver.hpp`), tested on its own.

## Tests

```sh
bazel test //projects/cosmic_hex/...
```

`Rules_test` checks:

- the board (19 planets, 4 homes, 7 core planets, the 12 zone tiles, with and without the corners variant) and the start;
- a turn's order;
- production, with colonies paying double and a blockaded planet paying nothing;
- the blockade on a 7, and its theft;
- moving, stopping on a face-down tile, which the dealer turns over, with a badge;
- settling, colonies, and at most two stations a planet;
- Core Control;
- contracts bought, dealt, kept and revealed;
- badges cashed in;
- the bank's 4 for 1;
- the zone's salvage and pirates;
- winning on your own turn;
- the other players' contracts hidden from a bot;
- the text of the starting state and of every action;
- two bots playing a whole game to a winner, whose action log replays exactly.

`handheld/`'s tests check what the app plays and draws, each part on its own (`BoardPicture_test`, `BottomScreen_test`, `TopViews_test` and the rest).

## Dice

`dice/` throws the two dice in Cyclone. The top screen is a glass container (a floor, walls at the screen's edges, a lid); two dice are thrown in from the left, and the faces they settle on are the roll. Then the container is pulled off the screen, the dice still simulated and sliding on its floor. A seed decides the throw, so the same seed throws the same on the same machine.

Fairness:

```sh
bazel run -c opt //projects/cosmic_hex/dice:dice_fairness -- 20000
```

It throws them many times and checks each face comes up a sixth of the time and the sums match two fair dice (chi-square). `dice_fairness_float` and `dice_fairness_flex_point16` do the same in the 3DS's float and the DS's 16.16 fixed point.

`DiceInTurn` throws dice one after another to land on numbers already chosen (who goes first):

1. each throw is first rehearsed unseen from its seed, to see which side lands up;
2. then it's thrown for real, with the die's numbers turned so that side shows the chosen number;
3. dice already settled stay where they are, too heavy for the next to move (a massless body sent Cyclone's contacts to NaN).

## Art

`art/` holds SVG drawings the build turns into texture recipes, drawn into textures when the game starts ([ScratchPadd's texture recipes](../scratchpadd/texture_recipe/README.md)). Swap in your own by replacing a file.

| Files | What they are |
| --- | --- |
| `art/*.svg` | The tiles' faces |
| `art/make_number_chips.py` | Writes the number chips' SVGs: a planet's number on a round chip, with a dot for each way two dice make it |
| `art/icons/*.svg` | The board screen's button icons |
| `art/cards/*.svg` | The cards' drawings |
| `art/cards/back_rings.svg`, `back_planets.svg` | The cards' backs: rings of hexagons for action cards, the six kinds of planet on a starfield for resource cards |
| `art/cards/cards.txt` | The cards' words: a line a card, its title, short title and what it does (the build embeds it and `Art::wordsOf` reads it) |

A card's drawing has no words: the game writes the title (centred, up to two lines) and what it does over it, in the areas `Art.hpp` gives (`kCardTitle`, `kCardArt`, `kCardWords`, in the drawing's 63 x 88 units), so a new drawing keeps to them. `Art_test` checks every card's words fit, in the consoles' font.

## Models

`models/title_models.blend` holds the title screen's rocket, a placeholder made by `models/make_title_models.py`:

```sh
blender --background --factory-startup --python make_title_models.py -- title_models.blend
```

To use your own, open the `.blend` in Blender and change or replace the object named `Rocket`. The build exports and cooks it for each console (`tools/assets`), simplifying the DS's version to its budget if it needs to.
