# Decisions

The decisions that shape SAKBox, each with why it was made and what was turned down, newest first. The discussion behind each is on the roadmap page (its rounds of questions and answers), and the work is in git's history; this keeps just the outcome, so it can be read without either.

Add an entry when a real choice is made: something a later reader might otherwise undo or wonder about. Keep it short. When a decision is reversed, add a new entry and mark the old one **superseded**, rather than deleting it.

## Format

**Title** (date)
What was chosen. **Why:** the reasons. **Not:** the alternatives turned down, and why.

## Cosmic Hex: the game

**Card titles take their band's ink; the starting hand dealt as a card show** (2026-11-21)
Every card view writes a title over a card's label band in one colour rule (`Palette::inkOnBand`: dark letters on a light band, light on a dark one), used by the top screen's views (`TopPictures::titleInkOf`) and the bottom screen's card sprites. The starting hand (3 random resources each) is shown by the cards-changing-hands show (`RollPayShow`): each card from the bank (the ROUND hexagon) to its player, face down to the others, the game held; the table sends the deal on its own so the first placing waits for it. **Why:** the owner's: white titles on the pale resource bands were hard to read, and Show resources already wrote them dark; the deal is cards going from the bank to the players, which that show already draws (arcs, a pop, overlap, holding the game, B and the Animations setting). **Not:** a new show of its own for the deal (it would repeat the roll pay's timing and drawing); the bank's cards bowing up along the top as before, where they were lost under the HUD (they now swing down over the board).

**DS memory is checked on the Mac: textures and heap** (2026-11-21)
The DS's texture memory is modelled (`TextureMemory`) and a desktop renderer can be held to it (`Renderer::setTextureBudget`); `GamePictures_test` loads every picture under it. Heap-hungry drawing is held to budgets too (`CloseUpBandsStayWithinTheirMemory`). **Why:** two silent failures on hardware (the title's rocket grey and the cards gone when textures ran out; a crash zoomed in when a picture's band asked for 147 KB in one piece) were found only on the console. **Not:** a log on the card for out-of-memory, as the DS stops before it could be written.

**The board's pointer is general** (2026-11-21)
One chevron over the corner in focus, fading in whenever you choose on the board and away while things move (`CornerPointer`). **Why:** the user's: a selected ship's chevron looked right, but it should say "your turn to choose here" everywhere. **Not:** a chevron only for ships.

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

**The 3DS loads textures last row first** (2026-11-21)
The 3DS backend puts an image's rows into the GPU's tiled order bottom row first (`TextureTiles.hpp`, tested on the Mac), for every texture: the meshes' pictures and the overlay's font atlas. **Why:** the GPU counts a texture's rows from the bottom, so pictures loaded top row first were drawn upside down in Azahar (a salvage tile's wrench mirrored top to bottom against its SVG and the DS's); the overlay alone was right, as its shader turned v into 1 - v. Fixed in one place, the upload, the shaders take texture coordinates as given and the other backends are unchanged. **Not:** turning v into 1 - v in the scene and toon shaders as well (three shaders kept in step, and the upload's order still contrary to what Scene says); flipping the meshes' coordinates in the shared code (every backend would see it).

**Lines of words stacked by their font's line height** (2026-11-21)
`LineStack.hpp` in ScratchPadd's scene: lines stacked down from a top, up to a bottom or around a middle, each as tall as `Overlay::lineHeight` says at its scale, with a gap; it gives each line's box. Cosmic Hex's top-screen views use it where lines of different sizes stack (a discovery, who goes first, choosing a resource), and a discovery's tile takes the room the words leave. **Why:** the owner asked for layout logic that works on every platform: lines placed at offsets scaled by the screen's width overlapped on the 3DS (its screen is shorter for its width, and Orbitron's lines are taller than Silkscreen's), e.g. a discovery's title over its "discovers" heading. **Not:** a layout engine with boxes and constraints (more than stacked lines need); separate offsets for each screen (every new screen or font would need its own); measuring by each text's ink (lines of the same scale would sit unevenly, by which letters they have).

**Buttons are raised, in the library** (2026-11-21)
Every `Buttons` panel draws its buttons raised by default (`Style::bevel`): the edge colour lit a fifth toward white along the top and left, the face at half brightness along the bottom and right, a 2-pixel dark drop under the face; pressed, it sinks a pixel and the edges swap. The drop and the sink stay inside the button's area, so touches, `areas()` and the bottom map's redrawn areas are unchanged. The lit edge comes from the button's edge colour, so a player's tab or the chosen tab keeps its colour's meaning. **Why:** the owner chose this look (B) from the mockups, for every panel at once; in the library, every app's buttons get it. **Not:** a drop outside the area (every caller's redraw areas would grow), or each app drawing its own bevel.

**Cosmic Hex's choosing screens' buttons are a Buttons panel** (2026-11-21)
Back, Decline and the green go button keep their places and words, and their touches are still found from their boxes; the screens stay drawn from what they're given, the press passed in (`ChoiceScreens::Press`) and kept by `BottomScreen`. The go button has the D-pad's focus once it's ready, as A presses it. **Why:** they were drawn by hand, without the press glow and focus the other buttons have. **Not:** making the rows, cards and players buttons: they're things to choose from, chosen with an outline, and a press glow on them would mean nothing more.

**A font for each kind of screen: Silkscreen on the DS, Orbitron elsewhere** (2026-11-21)
The overlay and the Canvas write in `Font::platform()`: Silkscreen, a pixel font at its own 8 pixels drawn at whole multiples, on the DS; Orbitron (SemiBold), a smooth font baked at the 3DS's overlay scales into one alpha atlas, on the 3DS and desktops. One interface (`Font`: measure, lay out into pieces), each font's data its own kind: a pixel font's runs along its rows, filled as rectangles; a smooth font's atlas, drawn as textured quads (the shapes from its solid corner, so the overlay stays one draw). Their data is C++ written by `make_fonts.py` from the TTFs in `third_party/fonts`, committed. **Why:** the owner's choice of nicer letters at little cost. On the DS the text layer is filled by the CPU and its texture memory has about 16 KB spare, so its font needs no texture (2.2 KB of runs in main memory); the 3DS has the texture memory for a 128 KB atlas and smooth letters suit its screens. Orbitron at 9 pixels to the em keeps stb_easy_font's widths and 12-point lines, so layouts carried over. Silkscreen's letters are as wide as stb's at the same scale (its capitals 5 pixels where stb's were 7), and a scale between whole numbers draws the size below it, so no layout comes out wider (rounding 1.5 up put a card's name past its edges); the DS's banners and round number ask for 2. **Not:** one font everywhere (a smooth font on the DS would need texture memory it hasn't, or blending on the CPU in floating point it hasn't); baking the fonts at build time (a Python and Pillow dependency for every build); FreeType in the apps (a new dependency, and rasterising on a DS); Orbitron's regular weight (its strokes thin out at 9 pixels); a 4-bit atlas on the 3DS (half the memory, but more code for 64 KB it doesn't need). The title screen's 3D letters stay stb_easy_font's strokes, as boxes.

**Views in the renderer, on every backend** (2026-11-20)
`Renderer::beginView(camera, viewport)`: a camera drawn into part of the screen, on Metal, OpenGL, Vulkan, the DS and the 3DS, rather than desktop-only or a second renderer. **Why:** the user's pick, for the lab's side-by-side comparison, and one interface that fits every platform (split screen and picture-in-picture later). Views drawn over another share its depth for now.

**The animation lab: three ways, baked to path and timing apart** (2026-11-20)
A desktop-only lab (`projects/animation_lab`) makes motions with Cyclone three ways (a solved throw, a spring guided along keys, a search for when there's no formula) in 3D or flat, and bakes each into ScratchPadd's `BakedMotion`: points evenly spaced along the path, and how far along at evenly spaced moments. **Why:** physics gives motion its weight, but the consoles can't afford to run it for every flourish; a few dozen floats can. Path and timing apart (the user's A) so either can be reused or retimed. **Not:** one curve of position against time (simpler, but a change of pace means a new curve); running the physics on the consoles (the DS can't spare it).

**DS textures in banks B and A** (2026-11-20)
The DS renderer maps VRAM banks B and A as texture slots 0 and 1, 256 KB for textures. **Why:** bank A was mapped by nothing, and with B alone the last pictures loaded (the other players' card backs) silently got no texture. **Not:** paletted textures (a bigger change for the same room). Sharper close-ups (128 pixels on the DS) then fitted in video memory but not in drawing: a picture was drawn whole in main RAM first (about 125 KB of heap free in play). **Then** (2026-11-20): close-ups are drawn a band of rows at a time, each band sent to its texture as it's done (`Renderer::replace` takes a first row, `TextureRecipe::draw` a band, `PictureSlot` the band height from `PlatformProfile`), and blank textures are made with no buffer. One interface everywhere: a desktop or 3DS draws the whole picture as one band, as before (the user's: keep the existing system; elegant interfaces that scale across constraints). The DS writes a band straight into video memory; Metal and OpenGL update the region; Vulkan and the 3DS keep each texture's pixels and make it again. **Not:** a sharper card on the DS's 2D layer (more video memory to share out, and a second way of showing pictures for the games to choose between).

**Checks: the static analyzer, sanitizers, heap budgets** (2026-11-20)
clang-tidy runs the static analyzer (`clang-analyzer-*`) on every project of the repo's own (several, Cosmic Hex among them, weren't in its paths at all), its noisy groups off with reasons in `.clang-tidy`; tests skip the unchecked-optional check (`ASSERT_TRUE` guards them); Waffles waits. `--config=asan-ubsan` runs the tests under both sanitizers. `MemoryStats::HeapBudget` holds a test to a console's memory. **Why:** the user's questions after the zoomed crash, which a heap budget would have caught on the Mac. The first run found real bugs: an empty bounding-volume tree followed a null child, fixed-point division by zero differed by platform (now the largest value of the dividend's sign), a socket used after failing to open.

**The zoomed map keeps only the faces on screen** (2026-11-20)
Zoomed in, the bottom map's sharp faces are a cache of those on screen, capped by the platform's budget (112 KB, eight faces, on a DS), letting go of the one seen longest ago; the rest are the whole board's scaled up. **Why:** every face drawn sharp at zoom 2 took 240 KB, and with the round's new shows the DS ran out of memory and stopped zoomed in. The heap's own "room left" (libnds' heap limit) said about 300 KB were left when it stopped, so the budget is fixed per platform, with the room only a second guard.

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

**One font on every console** (**superseded** by "A font for each kind of screen", 2026-11-21)
stb_easy_font's letters as rectangles, drawn as triangles on desktops and the 3DS, and into the DS's text layer.

**No textures, a Tron look**
Vertex colours, lines and fog. **Except** pictures drawn from SVG recipes in code (tile faces, icons, cards).

**Games are separate projects**
Cosmic Hex and Infinifly are their own projects, using ScratchPadd as a library.

## ThinLTO for the optimised Mac builds (2026-11-21)

- **Decided:** clang's ThinLTO (`-flto=thin`) for `-c opt` on macOS, on Cosmic Hex's rules, the simulator and the balance lab (`tools/lto/lto.bzl`).
- **Why:** splitting the rules into files by part of the turn moved small helpers out of the files that call them, and the simulator ran about 4% slower; ThinLTO inlines across files when linking. Measured on 60 games: about 5–6% faster than the split without it, every game's log identical.
- **Not chosen:** putting the helpers back in one file (the split was for readability); `-flto` everywhere (slower links for every program, for code that isn't hot); Bazel's `thin_lto` toolchain feature (the Mac toolchain doesn't offer it).
- **The DS: left off** (measured on a DS Lite, the -bench build with and without GCC's `-flto`, 2026-11-21): pictures drawn from recipes about 5% quicker (a 64-pixel card 103 ms against 109), a state copy 6% quicker, but the bots' rollout steps about 3% slower (213 µs against 207) and moves listed 2% slower; a simulated game 6–7 ms either way. The bots are what the player waits on, so no gain worth a different build. melonDS had shown the same mix (it doesn't model the caches).
- **The 3DS:** not tried (no bench there yet).

## Cosmic Hex: the discovery's backdrop behind its tile (2026-11-21)

- **Decided:** the dim backdrop is drawn a radius and a half behind the tile, wherever the tile has to be to fit its words (`discoveryDepths`).
- **Why:** at its old fixed distance (4.08 units) it was in front of the 3DS's tile (4.87 units away: Orbitron's taller lines leave it 44% of the screen) and cut through the DS's (3.90) as it turned over, dimming the tile as its name came in.
- **Not chosen:** drawing the tile with the depth test off (its chip and its own faces need depth); no backdrop (the board behind competes with the words).

## Cosmic Hex: where cards fly, in one place (2026-11-21)

- **Decided:** `CardPlaces` says where every flying card starts and lands: your hand, another player's row of the points bars (the row's middle, from `PointsBars::rowOf`, which the bars and their pops use too), the bank behind the ROUND hexagon; a payment to the bank leaves from the payer's home planet (or their exhausted one), projected each frame.
- **Why:** each show worked out a player's place itself; a roll's pay aimed six pixels right of the bars' end (empty space, further out on the 3DS's wider screen), a played card rose from the screen's right edge, and a home's pay flew off the top.
- **Not chosen:** names beside the rows to aim at (the HUD has none, and the rows are 5 pixels apart).

## Cosmic Hex: every build placed on the board (2026-11-21)

- **Decided:** a build chosen on the build screen always goes to the board's placing: its places marked, a single place chosen already with the pointer on it, Confirm or A to build. A card, with no place on the board, is still bought at once.
- **Why:** the owner pressed Build with one possible place and the ship was built with no sign of where.
- **Not chosen:** a confirmation dialog on the build screen (it can't show where).

## Overlay text: tracking for Orbitron's small letters (2026-11-21)

- **Decided:** `Font::tracking` adds a point between Orbitron's letters up to scale 1.5625, tapering to none at 2.5; measuring and layout both add it. The glyph data is unchanged (no regeneration).
- **Why:** the owner found the small text crowded on the 3DS. One function, used by `width` and `layOut`, keeps measured and drawn text in step.
- **Not chosen:** Silkscreen tracking (its letters have their own one-pixel gap; a second made the DS's words a fifth wider); baking wider advances into the font data (one number to tune in code rather than regenerated data).

## Cosmic Hex: explorer badges as the mockup drew them (2026-11-21)

- **Decided:** an earned badge is a yellow star inside a yellow hexagon's outline (`drawBadge`, shared by the HUD and another player's panel); the HUD's hexagons a pixel larger (radius 6) and 13 apart so the star reads on a DS.
- **Why:** the star was in the mockup but never built; both places drew a plain filled hexagon.

## Cosmic Hex: the home die is the home's own tile (2026-11-21)

- **Decided:** a home's pay pulls the home's tile up out of the board as the die (its end the board's own tile mesh with the face texture already loaded, its six sides the resources' colours and the star), its place left a dark pit; it spins about its long axis for each pay without going back between, then flies back over its place, drops in and settles with the build's dust. The show learns that more pays are coming from the game's phase (the dealer's home die roll next) and waits out, letting the game go, until the next roll comes.
- **Why:** the owner's design: the die should be the homeworld, and a home paying two should spin again rather than go in and come out twice.
- **Not chosen:** a new texture for the die's face (the DS has no texture memory to spare; the board's face is the same picture); the die's lower part hidden by drawing only what's above the board (the scene layer can't clip; the pit's walls and floor hide it instead, and read as the tile's empty place); stretching the die out of the board as it rises (a rigid thing would squash its coloured insets; superseded below, "the home die stretched out of its tile", 2026-11-22).

## Cosmic Hex: banners held long enough to read (2026-11-22)

- **Decided:** a banner's words hold 2.6 s in the middle (`Banner::kHold`, was 1.3 s); the band's fade and the words' sweeps in and out are as they were. Whatever waits for a banner asks it how long is left (`remaining`, `showing`), so nothing else needed the old total.
- **Why:** the owner's play-testing: the words went before they were read; the sweeps' speeds were right.

## Cosmic Hex: the flat board's ships face as the 3D board's (2026-11-22)

- **Decided:** the game's idea of a ship's heading is `BoardMotion`'s (the way it flew in, a new ship along +x as its mesh points); the bottom screen's darts, which always pointed up, now take each ship's resting heading (and the blockade's) through `BoardPicture::Marks`, kept as 64 steps a turn in a piece's look so a turn redraws it. A ship keeps its heading when another ship is built (it had snapped back to +x when ships came or went: matched now by owner and corner).
- **Why:** the owner saw the two screens' ships facing different ways.
- **Not chosen:** a heading in the rules' `Ship` (the rules don't care which way a ship faces; it's the animation's); pointing the 3D ships up the screen (they'd stop flying nose first).

## Cosmic Hex: Silkscreen drawn at half sizes (2026-11-22)

- **Decided:** Silkscreen draws a scale at the half pixel at or below it (`Font::pixelHalves`; at 1.5 its pixels alternate one and two screen pixels, `Font::sized`, measured as drawn). This supersedes "a scale between whole numbers draws the size below it" in the Silkscreen and Orbitron entry.
- **Why:** the owner saw the DS's larger words (the top screen's card names, the splash's "Vibe Coding by James", asked for at 1.5) shrink to 1 when Silkscreen came in: stb_easy_font had drawn them at 1.5. Half sizes give back exactly the size the layouts were made for and are never wider than asked. The Orbitron letter spacing (`Font::tracking`) is untouched.
- **Not chosen:** rounding titles up to 2 when they fit (the longer resource names, "Minerals" and "Crystals", don't fit their cards at 2, so card names would have come in two sizes); bigger cards on the DS (a layout change for the sake of one font).

## Cosmic Hex: who goes first, only the highest roll counts (2026-11-22)

- **Decided:** kept as written (`rollForFirst`, the README): those tied for the highest throw again; a tie below it isn't thrown again, since turns go round the table from the first and the other numbers decide nothing. The screen now says so: once all have landed the other numbers dim and "the rest follow round the table" shows.
- **Why:** the owner saw two tie on a 3, nobody throw again, and took it for a bug; the order then reads by seat, which numbers left bright made look like a ranking.
- **Not chosen:** ordering every player by their roll (the rules seat turns round the table; reseating players by roll would change the game, not the screen).

## Cosmic Hex: the home die rises out of space (2026-11-22)

- **Decided:** while the home's tile is out as the die, its place is empty space: the tile mesh in the board's background colour, just under where the die seats (`homeDieHole`), hiding the die's lower part as the pit did; a ripple of space, two hex rings spreading and turning opposite ways (`spaceRipple`), where it leaves and where it seats, in place of the build's dust. The die's motion and timing are unchanged. This supersedes the dark pit in "the home die is the home's own tile".
- **Why:** the owner's: the place should look as though no tile was ever there, the die pulled out of thin air, a wormhole; the opening is one hexagon where the pit was a column and a floor, and the ripple reuses the hex ring mesh, so it's cheaper on the DS.
- **Not chosen:** no opening at all (the die's sides below the board would show through the empty place); a new mesh or texture for the swirl (the DS has no texture memory to spare).

## Cosmic Hex: an offer to you announced, and countering (2026-11-22)

- **Decided:** an offer (or counter) to you starts the Banner in the offering player's colour, and the trade screen slides down the touch panel once the banner's words have settled (0.55 s in, 0.35 s long, eased to a stop), timed on the banner's clock (`TradeArrival`). Both are skippable like the other shows.
- **Why a slide, not a fade:** both were built and recorded on a DS. The slide reads as the screen coming from the banner above and stays sharp throughout; the fade's middle frames showed the map ghosting through the cards, at a DS's frame rate in a few muddy steps, and it has to mix every pixel each frame where the slide is a plain copy of rows.
- **Decided:** countering reuses the trade room's builder (`TradeRoom::startCountering`), started from the offer turned round, with Counter in the offer screen's Back place; a bot counters only a person's offer (`tradingRuleMove(state, mayCounter)`, which `TableGame` sets when the offerer is a person).
- **Why:** the rules already allowed one counter an offer (`CounterTrade`, `TradeCounter`) but no screen made one; bots countering each other would change the measured balance of the simulations.
- **Not chosen:** a separate counter screen (a second builder to keep in step); a key for Counter (the screen's keys are taken: A accepts, the D-pad moves; touch only for now); a counter's headline "COUNTERS YOUR OFFER" (too wide for a DS's banner).
- **Since:** with the shows taking turns (`GameShows`, below), the offer's banner waits its turn as every banner does (after a show that has the stage, or on the band of one sweeping), and the screen comes down after its words whenever they come in (`TradeArrival::after`).

## Cosmic Hex: another player's panel laid out from the screen (2026-11-22)

- **Decided:** `otherPlayerLayout` works out the panel from the screen's width and height and the font's line heights and widths, the same code on every screen: words at the width's scale, shapes and gaps at the smaller of the width's and height's scales, each line as tall as the font draws it. The header, the pieces, the planets and the Charters keep their size; the cards take the room left between the header and the pieces (a DS's 36 wide at most, half their column at most). A short row fans out and closes up as it fills. The planets' label sits beside their first row, saving a line.
- **Why:** every part was placed at a DS's offset scaled by the width, so on the 3DS (400 x 240: a fifth shorter than its width's scale asks for) two rows of cards pushed the planets past the bottom edge and the Charters over them. The owner chose smaller cards on the 3DS so two rows and the planets fit, and asked for one general layout rather than a mode for each console.
- **Not chosen:** a 3DS card size or offsets of its own (another set of numbers per console, and wrong on any other screen); shrinking the words or the planets first (the planets' numbers need to read, and the words match every other view); a third row of planets (the cards would get smaller still for a rare hand; past two rows the last place counts the rest).

## Cosmic Hex: cards drawn in layers, 2:3 (2026-11-22)

- **Decided:** every card, on both screens, is its layers (edge, cream body, title band in its colour from `cards.txt`, dark art band) drawn as tinted rounded rectangles cut from one shared 64 x 64 alpha texture, one mesh of four quads a card, with its icon the only picture (`CardFace`); a card is 2:3 wherever it's shown whole. The cards' SVGs are now just their icons; the card close-up slots and the dealt cards' wait for them are gone. DS texture memory: 240 KB used before, 176 KB after; the 3DS's (RGBA8, before mipmaps): 824 KB before, 936 KB after, its icons drawn at twice the DS's size.
- **Why:** the owner asked for cards built from cheap pieces, true card shapes and sharper icons; a whole card as one picture for each card and size cost the DS most of its card memory and showed only the card's top. Every card already shared one template (frame, bands, art box), so action cards take the same layers, and the drawing lives in one place for both screens.
- **Also:** the DS's renderer now tints an image by its vertices' and material's colours, as `Material` says and the other backends did (it drew every image white-lit, so the layers all showed white); and every backend leaves out a texel more than half see-through on a solid mesh, as the DS's one-bit alpha does (the 3DS and the desktops drew its colour: an icon's see-through ground black, the layers' corners square).
- **Not chosen:** one stretched rounded square for every layer (a long band's corners stretch with it); nine-slicing each layer (nine quads a layer, too many polygons for a DS showing Show resources' piles); a texture per resource for the frame; keeping the action cards as whole pictures (two ways to draw a card, and 2:3 would have cost the DS more); whole 2:3 cards in the trade room and the cards screen (their rows have no room; they show the cards' tops).

## Cosmic Hex: the shows' flow apart from the screens, and scripted play (2026-11-22)

- **Decided:** what ScreenPadd did with each new state and each frame for the shows (starting them, moving them on, the holds, the dice, B's skip, which show is drawn over the board) is `GameShows` in the handheld library, with no console code; ScreenPadd draws what it says and sends the table what it asks. `ScriptedGame` runs it with `BottomScreen` and a table paced as `TablePadd`, player 0 by `ScriptedPlayer`, logging every show (`ShowLog`).
- **Why:** the owner saw shows skip and asked for whole games scripted through the app's own hooks. The shows' flow lived in the console-only padd, so the Mac couldn't run it; moved, the runs exercise the code the consoles run, and found every cause below.
- **Not chosen:** scripting melonDS's input (slow, not repeatable, no view of the shows' state); a copy of the flow for tests (it would drift from the app's).

## Cosmic Hex: the table waits for the stage, one show at a time (2026-11-22)

- **Decided:** the game is held from the moment a move brings a show until it has shown, and while the pieces move or a banner sweeps; shows over the board take turns in the order they came, banners follow each other on one band and wait for a show that has the stage; the game's own clock pauses only when you pause; your move waits for the pieces to land; the table waits a move's time after each state it sends, and the dealer's draws after your move or the dice come with it; B ends only what's animating.
- **Why:** the scripted runs (20 games a setting) found 698 shows cut short or skipped unasked with Full animations: banners covered (373) or cut by the next (160), pieces cut by the next move (132), a home's pay spinning unseen under a discovery and appearing half done (the owner's "the home die just appears"), cards played under a discovery, the dice thrown under a discovery still to come (34, once the log counted the dice as hidden under a show), your cards gained shown once a game, and with B pressed now and then a home's pay or cards still to come skipped with what B was for. The table moved each 0.6 s whatever was showing, each show was timed as it came, and only one is drawn at a time. After: none.
- **Not chosen:** a longer pause between the table's moves (shows vary in length; the board would wait when nothing shows); drawing two shows at once (a DS's words are on one layer over every shape).

## Cosmic Hex: B declines an offer (2026-11-22)

- **Decided:** answering an offer, A accepts and B declines; touching two of your ships on one corner chooses the one that can still move.
- **Why:** the scripted player, on the pad alone, couldn't decline an offer it couldn't accept, and couldn't choose a new ship standing on its colony under one that had moved (End Turn waits for it): both left a game stuck.

## Cosmic Hex: salvage picked on one screen, shown over its find (2026-11-22)

- **Decided:** salvage's resources are all picked on the resource picker at once (the same one twice allowed), each sent as its own `TakeSalvage` in the order picked; the cards come up side by side over the find's tile while the find's words dim, then fly to their player; the find's tile waits facing you until they've landed. A bot's salvage plays the same show, at 1.4 times the pace.
- **Why:** the find said "Take 2 resources" and nothing then said how: the picker opened at once, during the ship's slide, under the find, asking for one resource at a time; its CHOOSE A RESOURCE banner started on the game's clock, which the find's hold stopped, and was drawn only once the find had gone; no show followed `TakeSalvage`, so a bot's salvage, and yours once taken, showed nothing.
- **How it's kept in step:** the picker waits while the find is still turning over (the snapshot that brings it sets the wait before the picker follows the game); the discovery lets the game go from its words on, for the choice to be made, and the salvage show holds it while the cards fly. Every time is on the shows' clock, so B, Quick and Animations Off need nothing of their own. With the shows' flow in `GameShows` (scripted play): a find waiting for its choice lets the table go and doesn't count as a show playing (so the scripted player and the bots can make it); salvage's cards take the stage like the other shows over the board, held by the one hold rule (`updateHolds`) from the last pick taken until they land; a show arriving while another has the stage waits at once, and B treats one still waiting its turn as to come. The cards are drawn as every card is (`cardFace`, 2:3), as large as fit between the find's name and what it gives (`salvageCardWidth`).
- **Not chosen:** one `TakeSalvage` naming both resources (the rules' move stays one resource, as the bots' search sees it); the single-card `PickShow` for salvage (it shows the card in focus, not what's picked, and only one); a new picker screen (the existing one gained `movePerPick` and `movesFor` instead); a separate card-flight drawing for salvage (`flyingCard` is shared with the home's pay).

## Cosmic Hex: one card shape, whole cards on the bottom screen, cards turned over (2026-11-22)

- **Decided:** every card, faces and backs, top and bottom screens, is 2:3, the shape another player's panel draws; `CardFace::kHeightPerWidth` is the one source of it, `heightOf` and `widthToFit` the only ways to a card's height, and Art's card units are checked against it (a `static_assert`). The bottom screen's partial cards (`CardSprites::topHeight`, `CardFace::topHeightOf`) are gone.
- **Why:** the owner asked for one card shape everywhere. Most views were already 2:3, but the trade room and the cards screen cut each card under its art, and the ratio was worked out from Art's units in several places (the shapes texture's card was `kWide * 3 / 2`).
- **Decided:** a resource card's art band goes down to its foot and its icon sits in the card's middle; an action card keeps the band under its title and its words under that. **Why:** the owner's: no empty cream under a resource's icon; resources have no words to put there. The tall band is a fourth shape in the same 64 x 64 texture, so texture memory is unchanged (DS 176 KB, 3DS 936 KB, before and after).
- **Decided:** an action card's words are drawn at the largest scale (from the screen's words' down to the bottom screen's) at which every line fits (`cardWordsFit`), and the cards dealt and played are as big as fit the screen (no longer capped at a DS's 100 and 90). **Why:** a new test showed what it does was cut off on most action cards at the old sizes: on a DS (Silkscreen draws nothing smaller than its own size) four of the seven lost lines when played, and on a 3DS most did at the top screen's scale.
- **Decided:** the trade room and the cards screen show whole cards, laid out from the screen's size and the font's lines (`tradeLayout`, `cardsLayout`), the same code on both consoles, as `otherPlayerLayout`. The trade room keeps its two rows (yours below); a card's count moves onto its title band and its minus and "of 4" onto its foot (the art band under its icon), so no lines are spent under the rows: 36 x 54 on a DS, 44 x 66 on a 3DS. The cards screen takes as many rows as make the cards biggest (56 wide for up to four on a DS, 27 for eight). **Why:** the owner asked for whole cards on both screens from the screen's measurements rather than per-console modes.
- **Decided:** a small card shows no title when any card's short title won't fit its band (one size for all, or none: `CardSprites::titled`); the trade room's cards are identified by their icons and band colours, and the cards screen's chosen card by its words. **Why:** Silkscreen can't draw smaller than its own size, and at 36 wide "Minerals" doesn't fit; a title on some cards and not others reads as a fault.
- **Decided:** resource cards revealed to you come in showing their back and turn over (`CardFace::turnOf`, 0.4 s, `kSecondsTurnOver`, shared with the cards dealt): salvage's and a home's card turn once they're in, before their names fade in, lengthening those shows by the turn; a roll's pay, the starting hand and trades or steals coming to you turn over the first half of their flight (no time added); cards gained turn as they drop into your piles (the drop 0.5 s, from 0.35). One drawing does it on the top screen (`turningCard`), replacing the cards dealt's and a card played's own. **Why:** the owner's reveal; each turn is part of its show's timeline on the shows' clock, so the shows after it wait for it and B, Quick and Animations Off need nothing new.
- **Decided:** another player's panel names each revealed Charter by its title after "Charter: " (`charterNameOf`); the first Charter's title became "Charter: Three of a Kind" (it was "Charter"). Where the line won't fit at the smallest scale (the DS's), it goes onto two lines, each name whole. **Why:** `chartersLine` used the short title, "Charter" for every Charter, so the panel read "Charters: Charter Charter".
- **Not chosen:** a per-console trade room or cards screen; trade rows with the labels in a column at their left (a few pixels more for the cards, losing "touch to add"); a strip under each row for the minus and "of 4" (the cards 32 wide on a DS); turning over cards going to a bot that were face up (the owner asked for the reveal to you); scrolling the cards screen (eight fit whole).

## Cosmic Hex: card icons a little larger (2026-11-22)

- **Decided:** a card's icon is drawn bigger than Art's area for it: an action card's 1.2 times (`CardFace::kIconScale`), a resource's 1.65 times (`kResourceIconScale`), and only its part inside the art band, 2 units in from its edges, is kept (`CardFace::iconOf`, `iconCutOf`, `iconPixels`), each icon's texture that part, padded to the DS's sizes, drawn on its own square. An action card's art band is 34 units tall (from 30), its words 33.5 (from 37.5). The Cartel's and Three of a Kind's drawings are a little smaller in their SVGs so they stay whole in the band.
- **Why:** the owner saw the icons shrink in the layered cards' rework. The drawings were as big for a card's width as before, but the card grew from 63 x 88 to 2:3 and most views fit a card by its height, so an icon was 93% of its size on a card the same height; and several views got smaller cards (the trade room's 48 wide on a DS became 36, a home's card on a 3DS 109 became 68, the picker's card on a 3DS 131 became 86: their cards had shown only their tops). A resource's icon needed 1.61 times to be its old size everywhere; an action card's 1.19, which its two wide drawings can't take inside the band. On a card the same height every icon is now at least its old size (`CardFace_test`).
- **Also:** DS texture memory 176 KB used before, 200 KB after (56 KB left); the 3DS's 936 KB before, 1320 KB after.
- **Not chosen:** letting an icon's square spill past the band (its ground is see-through, but the drawing's own edges would have no margin to the band's, and a resource's texture would be mostly ground); a square texture for a resource's icon cut from the middle (a resource's shown part is 53 x 46 units, not square); keeping action icons at their old size (the cards screen's cards got smaller too); a per-console icon size.

## Cosmic Hex: the home die stretched out of its tile (2026-11-22)

- **Decided:** the home die starts flat in its place, squashed along its long axis to a sliver (`kHomeDieFlat`, a hundredth of its length: the board's tiles have no thickness), so it is the tile and nothing hangs below the board; as it's pulled up, its face rises as before while its far end stays in the place, below the board, so it stretches, eased out (quick from the place, slowing to its length), to its whole length just as that end clears the board (`homeDieStretch`, from `lifted`). Coming back it squashes flat into its place the same way. It stretches along its long axis only, about its face's end, so the face is never distorted and the sides' coloured insets only lengthen. The path, the dip, the spin, the drop, the settle and their timing are unchanged.
- **Why:** the owner's play-testing: the die rose full size from below the home's place, so it read as appearing full-sized under the board and then being pulled out, not as the tile being pulled out. This supersedes "the home die is the home's own tile" (2026-11-21), which kept the die rigid because a stretch would squash its insets: scaling along the long axis only leaves the insets' widths and the face as they are, and the owner prefers the tile visibly drawn out of the board. `docs/research/animation-whimsy.md`'s "rigid things stay rigid" is set aside for this one piece at the owner's word; its easing rule is kept (the stretch eases out, arriving at its length without a jolt).
- **Not chosen:** scaling the whole die up from nothing (it would grow from a point, not from the tile); stretching linearly to its length (a kink in its speed as it went whole); its far end anchored exactly at the board's level (it would show under the board's edge as it stretched; below the place, it's hidden by the opening onto space).

## Cosmic Hex: short scripted games every run, full-length ones by hand (2026-11-22)

- **Decided:** the everyday scripted-play tests play short games: the agreed rules but for two of the rules' own settings, a home exhausted at 4 points and the win at 8 (`TableSettings::winPoints`, `exhaustHomesAt`), 32 seeds at the Full pace (`ScriptedGame_test`) and 5 at each other pace (`ScriptedGamePaces_test`), each game played once for all the checks. Three full-length games are `ScriptedGameLong_test`, tagged `manual`, run before merging.
- **Why:** the owner asked for many short games a run rather than a few long ones, through the game's own settings and its real win. A short game took 1,110 s of play on average against 2,319 s (seeds 1 to 32), so the everyday run plays 52 games in the time the old test's 20 full-length games took; more seeds found shows cut short that the old seeds never met (the star's choice and salvage's second pick meeting a show still to come: now `ShowsOnceCutShortArePlayedWhole`).
- **Decided:** a scripted run repeats from its seed, checked by a test. It already did (every source of chance is seeded and the run has its own clock); the consoles' games don't, by design: they seed from the clock and their bots think for a time.
- **Not chosen:** a special short mode in the game (the rules' own settings do it); a smaller board (not the agreed rules); dropping the long games (the agreed rules' own length still needs a check before a merge).

## Line coverage on a Mac (2026-11-22)

- **Decided:** `bazel coverage` on a Mac uses clang's source-based coverage, read by Xcode's `llvm-cov` and `llvm-profdata` into one LCOV report (`.bazelrc`'s `coverage:macos`), and `tools/coverage/lcov_table.py` prints it per file, worst first.
- **Why:** Bazel's default there (gcov's counters) wrote empty reports: every test passed and no coverage was found.
- **Not chosen:** genhtml's pages (not installed; a table is what a review needs); a coverage tool of the toolchain's own (none is set up for the Mac's toolchain).

## Cosmic Hex: a show keeps the stage through its pauses (2026-11-23)

- **Decided:** a show over the board has the stage from its start until it's done, its pauses for the game included: a home's die waiting for the star's choice or the next roll, a find waiting for salvage's picks, salvage's cards between picks (`GameShows::onStage`). Nothing else starts over the board meanwhile; a banner waits for it, except over a home's die waiting for the game, where banners are drawn (the star's choice, the turn's). The roll's prompt waits for the stage as every banner does.
- **Why:** sweeps of short games found shows cut short at the player's own choices: counted only while it moved, a home's die waiting for the star's choice gave the stage to a tile turned over earlier, which then covered the die and the choice's banner; the next placer's banner started in the pause between salvage's cards, which then flew over it; the roll's prompt started over a tile turned over.
- **Decided:** while the show on the stage waits for a move, what's still to come behind it (a tile, another home's pay, a card played, a banner) doesn't hold the table (`GameShows::stageWaitsForGame`).
- **Why:** held, the move the stage waits for (a bot's choice, the dealer's roll of the home die) is never made, and the game stalls: that's how an earlier attempt, a home's pay waiting for any tile still to come, stalled games.
- **Not chosen:** a home's pay waiting for every tile queued before it (strict order of arrival: the stall above); special cases for the seeds found.

## Cosmic Hex: the next show after B starts afresh, and is logged as its own (2026-11-23)

- **Decided:** B leaves every show not yet seen moving in a frame to start afresh after it (each show's own `waitFor` already tells: nothing once it's started), rather than GameShows deciding which were still to come; once a home's die is back, the next home's die comes out from now, not from the last one's end; in the show log, a show B ended is "skip pressed", and the next of its kind starting from its beginning in the following frame is a show of its own; only one moved on in place, part way (a banner to its waiting prompt), is the skipped one.
- **Why:** B jumps the shows' clock past every end, so a second home's pay timed from the skipped one's end was out long ago and never seen; and a home's pay due the moment it came, B pressed before its first frame (an impatient player, in the sweeps), counted as animating and was jumped past (against "B ends what's animating, not what's to come"); and the log labelled the next card played "skip pressed", as though the player had skipped it too.

## Cosmic Hex rules: rounds from the first player, no trading in the setup (2026-11-23)

- **Decided:** a round is counted from the first player (`State::first`), so the first round, with no fuel boost in the agreed rules, is everyone's first turn; a pirate hideout's steals during the deep-space start go back to the setup, not on to trading.
- **Why:** the written rules. Counted from player 0, with another player first, player 0 could boost in their first turn; and a hideout found by a starting ship sent the setup into trading, marked trading done and never reset it, so the first player had no trading in their first turn.
- **Effect on the simulator:** it shares the rules, so its games change: the bots' search plays through these rules, and every game but one of the first 40 played differently. Over 400 four-player games of the agreed rules (60 iterations) seat wins went from 24/24/24/28% to 26/22/26/24% and the average game from 19.4 rounds to 19.6, within what 400 games can tell; the balance lab's tests pass.
