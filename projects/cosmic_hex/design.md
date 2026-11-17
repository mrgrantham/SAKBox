# Cosmic Hex: design

Status: research, for review.

**Not a clone:** I won't build a copy of an existing board game: its name, rules as a package, art or look, even renamed. The hex-tile resource-and-trading genre is open to everyone, though: a map of hexes, resources produced and spent, building out from a start, and trading between players. So this is an original game in that genre, with its own theme, rules and look, designed together. Everything below about rules is a first proposal to change.

## Decided so far (from the roadmap page, 2026-11-14)

These replace the first proposal below where they differ. The reasoning behind several of them is in the [reading notes](../../docs/research/game-design-reading.md#applied-to-the-hex-game).

- **Working title:** Cosmic Hex. "Hexoplanet" is taken by an existing game.
- **Theme:** space. **The twist is exploration:** a tile's resource and its dice number are hidden until it's explored. Reveals are public, so a discovery becomes a sought-after destination.
- **Tile ideas:** asteroid belts (minerals), lush planets (food), and more to choose. Alien stations, hidden on the map and found by exploring, offer fixed-rate trades.
- **Production:** two six-sided dice. Victory points are separate from resources and can't be spent. Hidden victory points (secret contracts) are to be worked out together.
- **Building:** stations sit on whole planets, not corners or edges, and there are no lanes (both replaced by the rules under *The rules since round 11* below).
- **The start:** known home systems; placement still open.
- **Trading:** player to player, plus fixed rates at alien stations.
- **Players and length:** 2 to 4, at most an hour with four.
- **Order of work:** rules engine and bots first on the Mac, then Mac and 3DS over Wi-Fi with the bridge, then a DS each over local wireless.
- **Toolchain:** stay on devkitPro, with **no BlocksDS**: its cooperative threads would make the DS port work differently from every other platform. Calico has an unimplemented `WlMgrMode_LocalComms` mode and raw packet send and receive. The plan is our own ARM7 program plus an ARM9 library, offered to devkitPro once it works. This replaces the toolchain table below.
- **Cross-play:** wanted if possible. The Mac can't use DS local wireless, and a DS Lite only joins open or WEP Wi-Fi, so cross-play would go over a dedicated open game network.
- **colonist.io:** a reference for online play only (lobby, bots, turn timer, offering trades), not for rules.
- **Design rule:** approachable for new players, and every rule must work as a physical board game (tiles, tokens, cards, no hidden maths). The screens can help, but the rules mustn't need them.
- **Ships only** (no survey ships). A **blockade** ship (the embargo) moves on a roll of 7: its tile stops producing, and its mover takes a card from a player there.
- **Proposed, awaiting confirmation:**
  - A ship moving onto an unexplored tile flips it and earns an explorer badge. Its owner then chooses to settle there (the ship becomes a station) or fly on. 3 badges trade for 1 victory point.
  - Rare tiles in the base game: alien stations (2-for-1 trades) and ancient ruins (2 badges). Wormholes come later, as a variant.
  - Contracts: a deck of cards; each player is dealt 2 and keeps 1 secretly, and more can be bought.
  - 10 points to win, to be tuned with bots.
- **Decided since:**
  - explorer badges (3 for a point);
  - 10 points to win;
  - ruins and alien stations as the rare tiles, wormholes dropped. Stations are general (3 for 1) or specialist (2 for 1, their own resource); the bank is 4 for 1;
  - contracts all secret, dealt 2 and keep 1.
- **The board (decided 2026-11-14):** about 61 hexes, a hexagon five from centre to edge.
  - **Planets:** 19 planet slots, evenly spaced (every third hex), four of them identical home planets at even spacing on the edge.
  - **Deep-space zone:** the 12 space hexes within two of the centre get face-down tiles, half finds (salvage, comets, a nebula, the pirate hideout) and half nothing. No zone tile touches a home.
  - **Open space:** the rest, with no tiles.
  - **Set-up:** shuffled planet tiles face down on the slots, numbers hidden; shuffled zone tiles face down in the zone; the homes face up.
- **Home planets:** no dice number. Each gives its owner 1 resource of their choice at the start of their turn, a floor so nobody is left with nothing. Identical homes in the base game; a draft of starting spots as a variant.
- **First design settled** (2026-11-14). The numbers (board size, the zone's mix, points to win) get tuned once the rules engine and bots can play it.

### The rules since round 11 (2026-11-14)

- **Resources:** minerals (asteroid belts), food (lush planets), fuel (gas giants), water (ice moons) and crystals (nebulae).
- **Stations sit on a whole planet.** On this board a corner or edge touches at most one planet, so placing on corners would collect from the same single planet with extra rules. Up to **2 stations on a planet**, one per player: each planet tile has two printed docking spots.
- **No lanes.** They did nothing (Schell's elegance lens).
- **Ships stand on hexes** and move to any of the six neighbouring hexes, so a ship can leave a planet in any direction. A new ship is placed on a planet where its owner has a station (the home counts). A ship moves up to 3 hexes a turn for free; 1 fuel boosts it 2 hexes further that turn. Landing to settle or flip a tile ends its move. Ships pass freely through hexes holding other ships; only the blockade ship blocks.
- **The deep-space zone's inner six** move to the board's six corners, to try in play: an early, cheap gamble two hexes from the homes, while the middle stays worth the trip.
- **Points:** a station is 1, a colony (an upgraded station, collecting double) is 2; 3 explorer badges trade for 1; contracts as below; 10 to win. **Core Control:** 2 points to whoever has the most stations on the starred core planets (the slots nearest the centre). It needs at least 2 core stations to claim, a tie keeps it with the holder, and it passes only when someone overtakes (stations are never removed).
- **Costs, a first proposal for the bots to tune:**
  - ship: 1 minerals and 1 fuel;
  - station: settling uses up the ship, plus 1 food and 1 water;
  - colony: 2 food and 2 minerals;
  - contract card: 1 crystal, 1 water and 1 food.
- **Contracts:**
  - Dealt 2 at the start, keep 1 face down. Buying one: draw 2, keep 1. No limit on how many you hold.
  - Each card is one goal and its points (1 to 3, by difficulty). A goal only ever checks what's on the board or in your tokens at that moment, never something that happened earlier, so no one keeps a record at a real table.
  - On your turn you can reveal a contract whose goal is true; it scores at once and stays scored. So a player can hold a met contract and reveal it on the turn it takes them to 10.
  - Kinds for a first deck of 15 to 20: settlement (stations on 3 kinds of planet; 2 gas giants; 2 colonies; a core planet), exploration (5 badges; a kept zone find; stations on opposite sides), neighbours (sharing 2 planets; a station next to an alien station) and economy (6 resources in hand; a colony on a 6 or 8).
  - **Balanced with the bots' numbers:** for each card, how often it's kept when offered, met, and held by the winner. A card that's nearly always kept, or wins far more than others, gets a harder goal or fewer points, one at a time, and each change is reported with the numbers behind it.
- **Debug builds record games:** the starting board and every move, so real games can be analysed like the simulations.
- **Since (2026-11-15), from Fullerton's *Game Design Workshop*** ([reading notes](../../docs/research/game-design-reading.md#applied-to-the-hex-game)):
  - **Turn order:** the bots measure each seat's win rate; if the first seat wins too often, later seats start with an extra resource (like Go's komi).
  - **At most 3 ships in play per player,** like three ship pieces of your colour in the box.
  - **What's left to explore,** three settings, decided by play and simulation: A (the default), each kind of tile's count printed; B, the mix listed in the rules, all of it used; C, more tiles in the box than slots, the extras put back unseen, so the odds are rough and each galaxy differs. The bots show what each does (game length, lead changes, whether early explorers win more); recorded real games show which is fun.

### The rules engine's first version (2026-11-15)

The draft from the roadmap page's round 26, agreed as a good start; every number is in the rules engine's `Settings` for the bots to tune ([rules](rules), [simulating](README.md#simulating)).

- **The board:** a hexagon of radius 4 (61 hexes). The 19 planet slots are every third hex: the centre, and 6 each at distances 2, 3 and 4. The 4 homes are on the outer ring, opposite pairs first (so a two-player game faces off). The core planets are the 7 slots within 2 of the centre (the round-26 draft said 3, but the centre and the six around it are equally near). The zone: 12 face-down tiles, the six at distance 2 and, with the corners variant, six at the board's corners.
- **Planet tiles:** 3 of each kind (asteroid belt: minerals; lush planet: food; gas giant: fuel; ice moon: water; nebula: crystals). **Number tokens:** 2, 3, 4, 4, 5, 5, 6, 6, 8, 8, 9, 9, 10, 11, 12 (no 7). Both are drawn when a ship turns a planet over.
- **Zone tiles:** 6 nothing, 2 salvage (2 resources of your choice), 2 ancient ruins (2 badges), 1 alien station (a trading post, 3 for 1, for a player with a station or ship next to it), 1 pirate hideout (1 random resource from each other player). Salvage, ruins and the hideout count as a kept find. Whether 6 empty tiles is too many is for the simulations to show.
- **The start:** a station at your home (1 point), 1 ship there, and 1 of each resource. Turn order is random.
- **A turn:** your home gives 1 resource of your choice; the dealer rolls two dice; on a 7 you move the blockade ship to a turned-over planet (no production there) and take a random resource from a player with a station on it; otherwise every turned-over planet with the number pays each station on it 1 (a colony 2). Then, in any order: trade, build, move, settle, contracts, badges; then end the turn.
- **Moving:** each ship once a turn, up to 3 hexes, or 5 for 1 fuel; through other ships, but not through a face-down tile or the blockade's planet. Onto a face-down tile: it's turned over, the ship stops, and you earn a badge. A new ship moves from the next turn.
- **Building:** a ship (1 minerals, 1 fuel) on a planet where you have a station, at most 3 in play; a station (the ship, 1 food, 1 water) on a turned-over planet with a free dock, at most 2 a planet and one each; a colony (2 minerals, 2 food); a contract (1 crystal, 1 water, 1 food; draw 2, keep 1).
- **Trading:** the bank 4 for 1; the zone's alien station 3 for 1 nearby. **No trades between players in the first version** (the bots can't negotiate yet): they come later with simple trade rules, and the simulations will compare. The specialist stations (2 for 1) agreed earlier aren't placed anywhere in this draft yet.
- **Points:** stations 1, colonies 2, Core Control 2, every 3 badges cashed 1, revealed contracts their value; 10 on your own turn wins. A first deck of 17 contracts is in the rules engine.
- **Ideas for a second version or another mode** (your round-26 comment, tabled for now): planet clusters of 1, 2 or 3 planets with travel and building along the edges, and dead planets that produce nothing but can still hold a station.

### Corners and edges (roadmap rounds 39 to 50): the rules being built

Agreed with the user, to be built as a setting beside today's rules (stations on planets, ships hex to hex) so both can be compared on the same layouts:

- **Stations and colonies on corners**, where three hexes meet. A station earns from every planet it touches: 1 from each face-up planet whose number is rolled, a colony 2. Moons give nothing. (Round 45.)
- **The distance rule:** no station or colony on a corner next to another (an edge away). (Round 45.)
- **Ships stand on corners and move along edges**, a corner a step: **2 edges a turn, 1 more for a fuel** (rounds 46 and 50; tuned in the runs).
- **Discovery when a move ends:** when a ship ends its move on a corner of a face-down planet or zone tile, it turns over (a badge, as now). Passing by turns nothing over. (Round 47.)
- **Settling:** a ship on a free corner (no station on it or next to it) builds a station there, paying as now; the ship is used up.
- **Ships are built only at colonies**, as Starfarers builds them at spaceports. (Round 46.)
- **The start:** each player's home colony on a corner of their home planet, chosen in snake order (1, 2, 3, 4), then each player's ship placed in reverse order (4, 3, 2, 1) on any corner within 3 edges of their colony, the colony's own included. Placing turns nothing over. (Rounds 47 to 49.)
- **Alien outposts** (rounds 58 to 60; replacing the single alien station): two in the zone's 12 finds (5 nothing, 2 salvage, 2 ruins, 2 outposts, 1 pirate hideout). A ship on one of an outpost's corners trades 3 for 1, a station on one 2 for 1. A station may stand by an outpost though it touches no planet, but becomes a colony only if it touches a planet too (a colony doubles a planet's pay, and it has none).
- **Deep-space relay** (round 63, option A): only a holder of this Charter may build a station touching no planet and no alien outpost, one. The point is the station's: it scores 2, and the card nothing.
- **The home die** (round 66, the hybrid, simulated against the chosen income): below the exhaustion threshold each of the home's resources is rolled on a six-sided die, one face per resource and the sixth "your choice"; at the threshold the home flips to its city side as before. Randomness before the player's choices, a catch-up feature in itself (Characteristics of Games 4.2), and scarcity brought back, which gives trading a reason. Close to Starfarers' reserve-pile draws (2019 rules: 2 cards at 4 to 7 points, 1 at 8 to 9, none after, out of 15).
- **Trading between players** (round 57): after the roll's dealt with (production, or the blockade on a 7) and before anything else in the turn, the player whose turn it is may offer any resources for any resources, to everyone or to one player. Each in turn accepts, declines or counters once; the offerer accepts or declines a counter. No limit on offers. The bots trade by a simple rule rather than by search: each offers what it holds beyond its next build (a colony, a station, a ship, a card, at most two short) for what it's short of, one for one then two for one, each offer once a turn; each accepts what brings its own next build closer and costs only what it holds beyond it, but never from a player within two points of winning (kingmaking: Characteristics of Games). They don't counter. Simulated with trading on and off.
- **The deck** (rounds 59 to 65): 23 cards, close to the genre's 25, every card but the Charters an action. Enforcers ×14 (move the blockade and steal; the most played, 3 or more, wins the Galactic Embargo, 2 points), the Interstellar Cartel ×2 (the user's idea, the genre's monopoly: name a resource and every other player gives you all of it, or in a softer version up to 2 each; simulated both ways), Shipyard Surplus ×2 (the user's idea, the genre's road building: as you build a ship, a second comes free at the same colony; with no room for both under the ship limit you keep the card; flavour text "Under budget, ahead of schedule."), Supply drop ×2 (the genre's year of plenty: any 2 resources from the bank), and three Charters, one of each (round 63: copies only of the cards that act by themselves): stations on 3 kinds of planet (2), Homesteader (a station on another player's exhausted home world, 2), and Deep-space relay (below). One card played a turn, and not one bought that turn.
- **The exhausted home** (rounds 60 to 62, the user's idea; simulated against today's): on a player's first turn with 5 points or more, the resource they choose from their home is its last. The home becomes a planet of that kind, its number drawn from 4, 5, 9 and 10 (useful, never the richest), paying every station on it by the dice, its owner's too; anyone may build by it (the distance rule as everywhere); its guaranteed 1 a turn stops. On a real table (the user, after round 62): home tiles are double-sided, an earthlike blue-and-green world on one side and a city world on the other; an exhausted home is flipped to its city side, with a chip for the one resource it now gives and one for its number, so no spare planet tiles are needed. Whether the kind is chosen or random (an equal chance of each, a die) is for the simulations to show.
- **Finds beside the small systems** (round 58, the rule): each layout's finds go next to its singles and pairs, none touching a three; a layout without that list keeps the usual places until the layout solver places them.
- **The blockade** sits on a planet: stations touching it get nothing from it, and whoever moves it steals from a player with a station touching it. **Core Control** counts stations touching core planets; **the alien station** trades 3 for 1 with a station or ship touching it. (Round 46 plan; not objected to.)
- **The piles of kinds and numbers fit the board** (round 55): a kind and a number for every face-down planet that isn't a moon, the numbers in the genre's mix (2 and 12 half as common as the rest; Catan's own 18 for 18 planets), the kinds even. Before, the piles were made for the first board's 15 planets, so smaller layouts never dealt some, and bigger ones added 10s, 3s, asteroid belts and lush planets over and over.
- **The red-number rule** (round 55): a 6 or 8 isn't dealt next to a face-up 6 or 8 while the pile has another number.
- **Layouts:** the round-45 boards (layouts/layouts.txt), each kind of system the same number of moves from every home; finds beside the small systems as the first way of drawing players outward (round 45), then moons mostly in threes or better numbers on small systems, then a Charter for lone planets.

## Changes since (roadmap rounds 41 to 44)

- **12 points to win** (was 10), agreed in round 41 for testing.
- **Board layouts as a setting:** `Settings::planetHexes` names the planets (besides the homes), so the rules can be played on cluster boards. Round 44 offers 36 layouts on today's 61-hex board (clusters of 3, 2 and 1, never touching each other, the core or a home; homes on the edge or on four corners).
- **Moons** (round 42, named in round 44): `Settings::moons` of the face-down planets turn out moons: turning one over earns a badge as usual, it has no resource and no number, and it can hold a station (a point). The blockade doesn't start on one.
- **Bot styles** (round 41): plain, saver, rusher and explorer, the same search with the bot's own rollout moves leaning one way (`BotStyles.hpp`), to test the rules (the Imperial Tithe especially) against different players.
- Still to build: snake order for a free second station, and corners and edges (building on corners, moving along edges).

## Changes since (roadmap rounds 33 to 37, 2026-11-15)

- **Three and four players** for now; two players becomes a later variant with its own fixes (it's harder to balance: every steal hits your only rival, and turn order counts for more).
- **Charters:** the contracts' name. The deck is **25 cards, as the genre's best-known game has: 11 Charters and 14 Enforcers**, bought as before (1 crystal, 1 water, 1 food; draw 2, keep 1). The two easy Charters were made harder (6 badges, not 5; two finds, not one); the five "stations on 2 of one kind" merged into one, "stations on 2 planets of the same kind"; "all 5 kinds" became "4 kinds" (3 points); "three ships" and "sharing 2 planets" left out.
- **Enforcers:** played on your turn (one a turn, not the turn it's bought) to move the blockade and steal, as on a 7. **The Galactic Embargo:** a bonus like Core Control, 2 points to whoever has played the most Enforcers, at least 3, passing on overtaking. (Names still open to change.)
- **The Imperial Tithe** (being tested): on a 7, anyone holding more than 7 resources gives back half, from what they hold most of.
- **Catching up** (being tested): "below N points", an extra resource from your home while you have fewer than N, as a young home world with plenty untapped; with uneven starts for turn order.

## Why this game suits the DS

- **Each player's hand is private.** With a DS each, cards and resources stay on your own bottom screen while the shared board is on the top, which a single screen can't do.
- **It's turn-based.** A few small messages a turn, so the network's speed doesn't matter, and the DS's slowness doesn't either.
- **No router needed.** DS local wireless links consoles directly, and up to 16 can play (a host and 15 others).

## A first proposal for the game

Working title options: *Lattice*, *Hexfall*, *Outpost Grid*. The theme fits our look: an untextured, glowing hex lattice of sectors in space, each sector a resource.

- **The board:** a hex map, generated from a seed each game (shape, sector types, values), so every game differs.
- **Resources:** four or five kinds (say energy, ore, crystal, data), each from its own kind of sector.
- **Production without dice:** each sector has a visible cycle (it produces every 2nd, 3rd or 4th round), shown as a ring that fills. Production is then something to plan around rather than luck, and it sets us apart from dice-driven games. A random event deck can still bring surprises.
- **Building on sectors, not corners:** players claim **sectors** with outposts, connected to their own network by relays along the hex edges. An outpost collects from its own sector and the ones next to it, and upgrading it collects more.
- **Trading:** open offers to everyone, or private ones to one player, made on your own screen. Every DS shows the offer, and accepting is one touch.
- **Disruption:** an "interference" token any player can move by paying for it, to stop one sector producing.
- **Winning:** the first to a target score, from outposts, upgrades, the longest network, and contracts (secret goals dealt at the start).
- **Players:** 2 to 4. **A game:** 20 to 40 minutes.

The two screens:
- **Top screen:** the board in 3D (the scene layer: hex prisms, outlines, fog at the edges).
- **Bottom screen (touch):** your hand, building, trading and the log of what others did.

## Multiplayer: how it would work

- **The host's DS holds the true game state.** It's authoritative: the others send what they want to do (`BuildOutpost{sector}`, `OfferTrade{...}`), the host checks it's legal, applies it, and tells everyone what happened. Private things, such as cards drawn, go only to the player they belong to.
- That's the message bridge ([network README](../scratchpadd/network/README.md#the-message-bridge)), with one addition already among its open questions: **sending to one device**, not all, and knowing which device a message came from.
- **The transport is the open question.** The bridge runs over the network today (TCP, by IP address), which a DS Lite can't join (WPA2). The DS needs **local wireless** ("NiFi"):
  - **BlocksDS's DSWifi has it**, added in version 1.8.0 (February 2025) ([changelog](https://blocksds.skylyrac.net/docs/changelog/), [local multiplayer guide](https://blocksds.skylyrac.net/docs/dswifi/md_documentation_2local__multiplayer__mode.html), [API](https://blocksds.skylyrac.net/dswifi/group__dswifi9__mp.html)). One DS hosts (as an access point) and up to 15 join. The host sends a frame each round, and each client's reply comes back in the same exchange: `Wifi_MultiplayerHostMode`, `Wifi_MultiplayerClientMode`, `Wifi_MultiplayerHostCmdTxFrame`, and handlers for the packets. Packet sizes are fixed when it starts. It works in DS mode only, not in the DSi's own mode. There are examples (`examples/dswifi/local_multiplayer`, `multiplayer_packets`).
  - **SAKBox builds DS apps with devkitPro** (calico and libnds), and **it has no local multiplayer** (checked 2026-11-14 in the pinned `devkitpro/devkitarm` image). Its `dswifi9.h` has only access-point networking (`Wifi_InitDefault`, `Wifi_AutoConnect`, `Wifi_AssocStatus`, `Wifi_GetIP`), and calico's ARM7 wireless drivers (`ntrwifi.h`, `twlwifi.h`) only scan. There's no host or client mode and no raw frame sending.
  - So the bridge would get a **local wireless transport** beside the network one. Its frames are small and fixed-size, so a bridge message would be split into packets and joined again. The game wouldn't know which transport it's on.
- **Testing without two DS consoles:** melonDS can run several copies on one computer linked by emulated local wireless ([melonDS](https://melonds.kuribo64.net/comments.php?id=216)). It's timing-sensitive, but good enough to develop with. Then your DS Lite, and a second DS or your 3DS running the DS build.
- **The 3DS and the Mac:** a 3DS running the DS build uses DS local wireless, so it can play with DS consoles. The 3DS's own apps and the Mac would play over Wi-Fi with the bridge as it is; they can't join DS local wireless.

## The toolchain decision

This is the biggest question, because SAKBox's DS code relies on calico:

| Option | For | Against |
| --- | --- | --- |
| **A. Move DS builds to BlocksDS** | Local multiplayer as it is, active development, examples | ScratchPadd's DS code uses calico's preemptive threads and priorities (`threadSetPrio`, `threadIrqWait`, `pmMainLoop`); BlocksDS's libnds has its own threads (cooperative, by my reading), so the threading layer needs porting and re-testing. Every DS app is affected |
| **B. Stay on devkitPro; add local multiplayer to it** | Nothing else changes | devkitPro has none of it (above): its wireless is calico's ARM7 driver, which only connects and scans, so it means writing a local multiplayer driver into calico, or replacing calico's ARM7 side with BlocksDS's. A big, risky port |
| **C. Build this game with BlocksDS, with ScratchPadd unthreaded** | ScratchPadd already builds without threads (`SCRATCHPADD_NO_THREADS`: every padd on the main thread, `RunsUnthreaded_test`). A turn-based board game doesn't need threads. Only this game changes toolchain | Two DS toolchains in the repo (a second Docker image for BlocksDS); the scene layer's DS backend would need building against BlocksDS's libnds too |
| **D. One DS, passed around** | No networking at all | Hands aren't private; less fun |

I'd lean to **C**, after a short spike:
1. Build BlocksDS's `local_multiplayer` example in Docker.
2. Run it in two melonDS copies.
3. Then build a small ScratchPadd app unthreaded with BlocksDS, sending bridge messages over it.

That shows what's real before we commit.

## Questions

1. **The game itself:** the theme, the title, how production works (cycles rather than dice?), building on sectors, and how many players.
2. **The toolchain** (above): C after a spike, or another option?
3. **Which devices play together:** DS-only over local wireless, or should 3DS-native and Mac players join over Wi-Fi too? A game hosted on a DS can't reach those.
4. **Hidden information:** private hands, secret goals and private trades all need the bridge's "send to one device". Agree to add it?
5. **What the 3DS shows,** if it plays natively: the board in stereo, and the touch screen as the hand.
6. **Rules engine first:** I'd write the rules as plain C++, tested on the Mac with simulated players, before any networking or screens. Agreed?
