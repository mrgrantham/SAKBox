# Decisions

The decisions that shape SAKBox, each with why it was made and what was turned down, newest first. The discussion behind each is on the roadmap page (its rounds of questions and answers), and the work is in git's history; this keeps just the outcome, so it can be read without either.

Add an entry when a real choice is made: something a later reader might otherwise undo or wonder about. Keep it short. When a decision is reversed, add a new entry and mark the old one **superseded**, rather than deleting it.

## Format

**Title** (date)
What was chosen. **Why:** the reasons. **Not:** the alternatives turned down, and why.

## Cosmic Hex: the game

**Animations: Full, Quick or Off, and B skips** (2026-11-20)
B (Back) takes every animation running straight to its end frame (a second B does B's usual job); the Settings' Animations is Full, Quick (twice as fast) or Off (skipped every frame), kept on the card. **Why:** someone playing through quickly shouldn't wait on flourishes (the user's). One clock for the shows (`showClock_`) and the game's clock run at the setting's rate, and skipping moves the shows' clock on past any end, so each show needs no skip code of its own; the dice are stepped on to where they land at once, so the roll is still the physics'. **Not:** a skip method on every show (more code, easy to miss one).

**Who goes first: physics dice, one at a time** (2026-11-20)
Each player's die, in their colour, is thrown into the glass in turn and settles on the number the game rolled; A throws the next at once. Each throw is rehearsed unseen first, and the die drawn with its numbers turned so the side that lands up shows the chosen number. **Why:** the user's: real dice, each settling before the next. **Not:** throwing until the physics happens to give the number (unbounded), or flat pictures of dice (built first, and not what was agreed).

**The home die: a white hexagonal prism** (2026-11-20)
An extruded white hexagon, a resource's colour on each of five sides and the star on the sixth, rolling about its long axis in 3D and landing with the chosen side facing you. **Why:** the user's: six sides for six outcomes, and it reads as a die of its own.

**Finds are called Anomalies** (2026-11-20)
The deep-space zone's face-down tiles (salvage, ruins, alien outposts, the pirate hideout, or nothing) are Anomalies wherever a player reads about them. **Why:** more exciting and space-themed (the user's pick). **Not:** Discoveries (planets are discovered too: the two would blur). In the code they stay `Find` (the rules' enumeration, the art's `find_*` files): a rename there would touch every file for no change a player sees.

**Starting placement: choose your home and your colony's corner** (2026-11-19)
Who goes first is shown as a roll of a die each, highest first. Then, in turn order, each player picks one of the board's homeworlds and a corner of it for their home colony; then the ships, in the reverse order. Placing is by touch: what you touch is shown there, and Place (End Turn's button) puts it down. **Why:** a setup you can see and choose reads as fair (Characteristics of Games, ch. 5); the reverse order for ships is the catch-up for placing last. **Not:** colony and ship in one turn (loses the catch-up), homes fixed by seat (the user wanted the choice). Simulated at 1,800 games: on R3 choosing homes evened the seats (25, 26, 25, 25%; chi-square 1.0) where fixed homes leaned (22 to 27%; 7.9); T4 with three players stays fair (31, 33, 36%; 5.6).

**The splash: words and a small mark** (2026-11-19)
"Vibe Coding by James / presents" with two turquoise hexagons, fading in and out while the start's work runs. **Why:** the start draws every picture before the title (so the title runs smoothly) and takes about 5 s on a DS; something to watch makes it feel shorter. **Not:** words alone.

**The roll waits for you** (2026-11-18)
Your dice are thrown when you press Roll (or A), never for you; a bot's dice wait for its turn banner to finish. **Why:** the roll is the player's moment.

**Other players: a top screen each, with tabs** (2026-11-17)
**Why:** room for everything the table can see. **Not:** one panel with every player's planets, which runs off the screen with three players.

**Badges cash in at once** (2026-11-16)
Three explorer badges become a point as soon as they're earned. **Why:** hoarding added a dull choice and hid a player's real score.

**The Charters** (2026-11-16)
Stations on 3 planets of one kind, Occupier, and the Deep-space relay (1 point on top of its station's). **Why:** "3 kinds of planet" was met by nearly everyone who kept it; "3 of one kind" by 22 to 38%. Simulated at 1,800 games a board.

**Imperial Dominion: off** (2026-11-16)
**Why:** in simulation it barely triggered.

**Boards are judged at 1,800 games**
**Why:** leaders at 300 and 900 games mostly regressed. Clusters of planets never touch each other or a home; boards are radius 4.

## Platform: ScratchPadd and the consoles

**DS textures in banks B and A** (2026-11-20)
The DS renderer maps VRAM banks B and A as texture slots 0 and 1, 256 KB for textures. **Why:** bank A was mapped by nothing, and with B alone the last pictures loaded (the other players' card backs) silently got no texture. **Not:** paletted textures (a bigger change for the same room). Sharper close-ups (128 pixels on the DS) then fitted in video memory but not in drawing: a picture was drawn whole in main RAM first (about 125 KB of heap free in play). **Then** (2026-11-20): close-ups are drawn a band of rows at a time, each band sent to its texture as it's done (`Renderer::replace` takes a first row, `TextureRecipe::draw` a band, `PictureSlot` the band height from `PlatformProfile`), and blank textures are made with no buffer. One interface everywhere: a desktop or 3DS draws the whole picture as one band, as before (the user's: keep the existing system; elegant interfaces that scale across constraints). The DS writes a band straight into video memory; Metal and OpenGL update the region; Vulkan and the 3DS keep each texture's pixels and make it again. **Not:** a sharper card on the DS's 2D layer (more video memory to share out, and a second way of showing pictures for the games to choose between).

**Words fade in once what they're over has settled** (2026-11-20)
Text over a card or tile waits until it has stopped moving (an overshooting curve's first pass through 1 doesn't count) and fades in over 0.25 s. **Why:** the user's: a number appearing on a still-bouncing token, or text just appearing, looked unfinished. On the DS the text layer can't be see-through, so a fade blends the colour up from what's under it.

**Menu roles and a button library** (2026-11-19)
`Input::Role` (Confirm, Back, Option1, Option2) mapped by each kind of pad's convention, and `Buttons::Panel` (touch, D-pad focus, feedback, drawn on a Canvas); Cosmic Hex's board buttons, menus and setup built on them. **Why:** positions don't say purpose, and Cosmic Hex had three button systems doing the same things. **Not:** leaving roles to each game's own actions; only the shared logic (each game drawing its own).

**Colours named by role** (2026-11-19)
Cosmic Hex's `Palette.hpp`: the accent (turquoise: what's always there and whose turn), the highlight (amber: moments and choices), the words, and the players'. **Why:** the same colour had been written out in a dozen places.

**No "round N" tags in READMEs or comments** (2026-11-19)
Comments keep the reason and the measurement instead. **Why:** a round number means nothing without the roadmap page open. **Not:** keeping the whole conversation in the repository; git history and the roadmap page hold it.

**Cyclone's sweep tolerance** (2026-11-19)
The contact solver can stop its sweeps once nothing changes (off unless asked for; the dice use 0.003). **Why:** Millington's resolver stops at its epsilon (15.3); the dice's physics took 44% less time and stayed fair over 20,000 throws.

**The bottom map draws only what changed** (2026-11-19)
Each piece listed with its look and area; changed areas drawn again from scratch; tested equal to drawing it all. **Why:** a full redraw after each move cost up to 45 ms on a DS. **Not:** 16-bit pictures (every drawing piece would change, for less gain).

**Canvas and ConsoleScreens in ScratchPadd** (2026-11-19)
**Why:** every app with a touch screen needs them; they'd been inside Cosmic Hex.

**Measured performance rules for the DS** (2026-11-18)
Whole numbers or fixed point (no floating-point hardware), the hardware divider, word-aligned pixels, nothing drawn that hasn't changed. Listed with where they live in ScratchPadd's README ("Performance on the consoles"). **Why:** each was measured in melonDS before and after; together they doubled the frame rate.

**Fixed-point maths on the consoles**
flexmath's Q16.16 on the DS, with CORDIC and tables later for identical maths everywhere. **Not:** porting CORE-MATH, which needs 128-bit integers.

**The consoles' Makefiles take their sources from Bazel**
The Makefiles stay (devkitPro's build), but their source lists are generated from the BUILD files. **Why:** one place lists a target's sources.

**One font on every console**
stb_easy_font's letters as rectangles, drawn as triangles on desktops and the 3DS, and into the DS's text layer.

**No textures, a Tron look**
Vertex colours, lines and fog. **Except** pictures drawn from SVG recipes in code (tile faces, icons, cards).

**Games are separate projects**
Cosmic Hex and Infinifly are their own projects, using ScratchPadd as a library.
