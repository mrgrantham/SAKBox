# Research and design documents

Research and first designs for what's next, to review together before anything is built. Each ends with its questions. Once a design is agreed, it moves into the README of the code that implements it, and its document here is updated or retired.

| Document | What | Depends on |
| --- | --- | --- |
| [Toon shading and outlines](toon-shading.md) | Hard-banded lighting and outlines on the DS (hardware toon and edge marking), the 3DS (vertex-shader bands, or fragment-lighting tables; inverted-hull outlines) and desktops (shader ramps; inverted hull or screen-space edges) | The scene layer |
| [The endless on-rails ship game](rail-ship-game.md) | A ship on an endless, seeded, procedural course, steering in a corridor, Tron-like, on the Mac, 3DS and DS; Cyclone's new heightfield; a moving origin for Q16.16 | Toon shading; Cyclone |
| [A multiplayer hex-tile trading game](hex-trading-game.md) | An original game in the hex resource-trading genre, played with a DS each over local wireless; the host's state is authoritative; the DS toolchain decision (local multiplayer is in BlocksDS's DSWifi, not devkitPro's) | The message bridge |

Related, in code: the message bridge's open questions are in [the network README](../../projects/scratchpadd/network/README.md#questions-still-open).
