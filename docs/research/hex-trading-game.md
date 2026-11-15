# A multiplayer hex-tile trading game on the DS: research and design

Status: research, for review.

**Not a clone:** I won't build a copy of an existing board game: its name, rules as a package, art or look, even renamed. The hex-tile resource-and-trading genre is open to everyone, though: a map of hexes, resources produced and spent, building out from a start, and trading between players. So this is an original game in that genre, with its own theme, rules and look, designed together. Everything below about rules is a first proposal to change.

## Decided so far (from the roadmap page, 2026-11-14)

These replace the first proposal below where they differ. The reasoning behind several of them is in the [reading notes](game-design-reading.md#applied-to-the-hex-game).

- **Working title:** Cosmic Hex. "Hexoplanet" is taken by an existing game.
- **Theme:** space. **The twist is exploration:** a tile's resource and its dice number are hidden until it's explored. Reveals are public, so a discovery becomes a sought-after destination.
- **Tile ideas:** asteroid belts (minerals), lush planets (food), and more to choose. Alien stations, hidden on the map and found by exploring, offer fixed-rate trades.
- **Production:** two six-sided dice. Victory points are separate from resources and can't be spent. Hidden victory points (secret contracts) are to be worked out together.
- **Building:** leaning towards corners and edges (stations at corners, lanes along edges), details open. Exploring: a lane reaching an unknown tile reveals and claims it; survey ships reveal tiles from a distance (publicly), starting a race to claim them.
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
- **Still open, with options on the roadmap page:**
  - the board. My recommendation is planets on even slots with space tiles between, which flip too and sometimes hold small finds: salvage, comets, nebulae, a pirate hideout. About 61 hexes, to tune with bots;
  - the start: identical known home planets with nothing to place, or a draft of starting spots.

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
- That's the message bridge ([network README](../../projects/scratchpadd/network/README.md#the-message-bridge)), with one addition already among its open questions: **sending to one device**, not all, and knowing which device a message came from.
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
