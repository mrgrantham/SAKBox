# The endless on-rails ship game: research and design

Status: research, for review. The name and the concept are still open. This sets out a design to react to.

## The idea

A ship flies along an endless, procedurally made course: over terrain, between obstacles, through gates. It's on rails: the course carries it forward, and the player steers it within a corridor around the rail, up, down, left and right, to dodge, collect and score. Runs last until a crash. The look is untextured and Tron-like: dark ground, glowing edges, vertex colours, fog hiding the far distance, and toon bands and outlines ([toon-shading.md](toon-shading.md)). It runs on the Mac, the 3DS (in stereo) and the DS, and Cyclone does all of its collision.

The on-rails shooter genre, games where the screen scrolls forward and the player dodges and shoots, is the starting point. The game and its look would be our own.

## What we already have

| Need | We have | Missing |
| --- | --- | --- |
| Rendering on all three | The scene layer: DS, 3DS (stereo), Metal, OpenGL, Vulkan; fog on the DS; culling | Toon bands and outlines (that branch first) |
| Physics and collision | Cyclone: boxes, spheres, capsules, planes; contact resolution; Q16.16 on the DS | **Terrain**: a heightfield shape |
| A frame loop with input | ScratchPadd: GamePadd (the pad and keyboard on every platform), padd priorities, 30 frames on the DS | Nothing |
| Sound | AudioPadd, sounds made in code | The game's own sounds |
| Saving | StoragePadd, `SaveFile` | Nothing (high scores) |
| Tuning on hardware | Panels on every platform, `remote_controls` | Nothing |

## The parts

### The course

- **The rail:** a smooth curve made of segments (Catmull-Rom splines through control points), generated ahead of the ship from a seed and dropped behind it. The same seed gives the same course, which is good for testing, sharing seeds and fair high scores.
- **Chunks:** the course is generated in chunks, say a few seconds of flight each, a few ahead of the ship. A `CoursePadd` that `runs()` `WhenIdle` builds the next chunk while nothing else needs the processor. That suits the DS, where generation must never take time from a frame.
- **Terrain:** a heightfield strip under and around the rail in each chunk. It's a grid of heights from layered noise, flattened near the rail so the ship's path stays clear, with vertex colours by height. The grid is coarse on the DS (polygon budget: 2048 a frame, with fog hiding the far chunks) and finer on desktops.
- **Obstacles and pickups:** pillars, arches, gates, moving blocks, placed by the seed with a difficulty that rises with distance. They're Cyclone shapes: boxes, spheres, capsules.

### Physics: Cyclone

- **The ship** is a rigid body that follows the rail: its target is the rail's point plus the player's offset in the corridor. Springs or a constraint pull it there, so it banks and sways with weight rather than snapping. A crash comes from its contacts.
- **A heightfield shape** is new to Cyclone. Millington's book has planes, spheres and boxes but no terrain, so it would follow Ericson's *Real-Time Collision Detection* (as the plan says: the book first, then Catto and Ericson):
  - the heightfield is treated as two triangles per grid cell;
  - a shape tests only the cells under its bounding sphere;
  - sphere-triangle and box-triangle contacts come from Ericson.

  `world_test` and `stress_test` would cover it like the other shapes.
- **Endless in Q16.16:** the DS's numbers go up to about ±32,767, so 32 km at a metre a unit. An endless course needs a **moving origin**: every so often, say each chunk, everything is shifted back by the ship's progress, so numbers stay small. Every system that keeps positions shifts with it: the physics, the chunks and the camera. Precision stays the same everywhere along the course.
- **Cost on the DS:** the ragdoll's sweep showed about 6 ms per sub-step on the DS. A ship and a few nearby obstacles are far fewer contacts than a ragdoll on the ground, so this should fit at 30 frames a second. The sweep app idea could measure it early.

### The padds

| Padd | Runs | Does |
| --- | --- | --- |
| GamePadd | on the main thread | The pad |
| ScreenPadd | on the main thread | Draws the course, the ship and the HUD; 30 frames a second on the DS, 60 elsewhere |
| FlightPadd | steadily | Steps Cyclone: the ship, contacts, crashes, score |
| CoursePadd | when idle | Generates chunks ahead and drops them behind; the moving origin |
| AudioPadd | urgently | Engine hum, passes, pickups, crashes |
| StoragePadd | when idle | High scores, settings (`SaveFile`) |
| AboutPadd, NetworkPadd + RemoteControlsPadd | | Tuning from the Mac while it runs |

### Each platform

- **DS:**
  - 2048 polygons a frame: a coarse terrain, a few chunks visible, and fog over the rest.
  - The top screen for the view; the bottom screen for the score and a course map, or touch steering.
  - Q16.16 physics with the moving origin.
- **3DS:**
  - Stereo 3D for depth (the slider), with outlines on the key objects ([toon-shading.md](toon-shading.md)).
  - The circle pad for steering.
- **Mac:** the full look and the largest distances; a pad or the keyboard.

## Questions

1. **The core of play:** dodging only, dodging and collecting, or shooting too? Shooting adds targets, aiming, and enemies with their own behaviour.
2. **How much steering:** a corridor around the rail (up, down, left, right), or lanes (left, centre, right), or full freedom in a tunnel? A corridor seems the best fit for analogue steering.
3. **The course's feel:** over terrain (valleys, canyons), through structures (tunnels, cities), or in space (no ground: rings, debris)? Space needs no heightfield, which would make Cyclone's new shape a later step.
4. **Runs:** endless until a crash, with a score by distance? Or lives, checkpoints and stages?
5. **Seeds:** a daily seed or a typed seed, so runs can be compared? Should scores sync between devices (the network), or stay on each one?
6. **The bottom screens** on the DS and 3DS: a map, the score, touch steering, or the controls panel?
7. **Name and theme:** a working title, and what the ship is (a light-cycle-like craft, a glider, a probe).
8. **The order of work:** toon shading first (planned), then Cyclone's heightfield (with tests), then a first playable course on the Mac, then the DS and 3DS?
