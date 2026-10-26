# Cyclone in padds

Cyclone's ragdoll, with its physics and its window in separate [ScratchPadd](../../scratchpadd/README.md) padds on separate threads, talking only by messages. The physics steps at its own rate in fixed steps and [sub-steps](../../cyclone/README.md#sub-steps), the window draws at the screen's rate, sliders change the steps while it runs, and the bones knock as they hit, played by [AudioPadd](../../scratchpadd/README.md#audio).

```bash
bazel run //projects/examples/cyclone_padds
bazel test //projects/examples/cyclone_padds/...
```

R resets the ragdoll (with a new random shove), P pauses, dragging turns the camera, Esc quits. The panels on the right are each padd's controls.

## How it's built

```
RagdollPhysicsPadd (own thread) ──── RagdollSnapshot, each tick that stepped ────►  RagdollWindowPadd (main thread)
        ▲        │                                                                          │
        │        └── LoadSoundSamples once, PlaySound per knock ──►  AudioPadd (own thread)  │
        │                                                                                   │
        └──── RagdollCommand (R, P), CameraTurned, ControlChange (the panels' edits) ───────┘
```

| Padd | Owns | Receives | Sends |
| --- | --- | --- | --- |
| `RagdollPhysicsPadd` | the ragdoll (`ragdoll_physics.h`, shared with Cyclone's demo), its contact resolver, a `FixedSteps` clock, an `ImpactKnocks` | `RagdollCommand`, `CameraTurned`, control messages | a `RagdollSnapshot` each tick that took a step; `ControlSnapshot`; to `AudioPadd`, the knock (`LoadSoundSamples`) and a `PlaySound` for each hit |
| `RagdollWindowPadd` | the window, drawing, the panels, the camera, a `SnapshotTimeline` | `RagdollSnapshot`, `ControlSnapshot` | `RagdollCommand` for R and P, `CameraTurned` as the camera turns, `ControlRequest`, `ControlChange` to the padd whose panel was edited |
| `AudioPadd` | the sound device (ScratchPadd's audio engine) | `LoadSoundSamples`, `PlaySound` | (answers nobody here listens for) |

**The physics ticks on its own thread.** Every 1/60 s (`kPhysicsTickSeconds`, its `repeatInterval()`), `RagdollPhysicsPadd` adds the real time since its last tick to its `FixedSteps` clock and takes the whole steps that covers, each in its sub-steps, exactly as the rigid-body demos step (`RigidBodyApplication::runStep`). Then it sends one snapshot: each bone's position, orientation and size, the joints (which two bones, where on each, and their slack), and what it measured. The window never touches the physics' state, and the physics never waits for the window.

**Its controls are the sliders.** `Steps a second` (10 to 240, 60 to start), `Sub-steps` (1 to 16, 2 to start, as in the demos), `Resolver` (all contacts together, or the book's one at a time), `Paused` and `Sound`. They take effect from the next step. Raising the steps a second and raising the sub-steps run the same physics (see Cyclone's README); both are here to try.

**The window blends between snapshots.** Snapshots arrive at the physics' rate, not the screen's: at 20 steps a second there are 20 a second, and even at 60 the physics' tick and the screen's refresh drift against each other, so some ticks take no step and others two. Drawing the newest snapshot as it is makes the ragdoll jump. `SnapshotTimeline` draws it a little in the past instead (one tick plus one step: 33 ms at 60 steps a second), blended between the two snapshots either side of that moment: positions in a straight line, orientations along the shortest turn. It never draws ahead of the newest snapshot, and a reset isn't blended with the ragdoll before it. It works out how the physics' clock lines up with the window's from when snapshots arrive, easing out their jitter, and catches up at once after a pause. The window's own `Blend between snapshots` control turns blending off, to compare: at 20 steps a second the difference is plain.

**The knocks come from the physics padd.** It sees every sub-step's contacts, so it hears every hit, however short. After each sub-step finds its contacts, and before they're resolved, it hands them to `ImpactKnocks` (from Cyclone's demos, which use it for their own knocks): pairs of bones, or a bone and the ground, closing at over 1 m/s. Once a tick it takes the knocks for the hits heard (the hardest six, each pair at most every 0.1 s), and sends AudioPadd a `PlaySound` for each: louder the harder the hit, lower for heavier bones, panned to where the hit is on screen (the window sends `CameraTurned` as the camera turns). In `prepare()` it finds AudioPadd by name and gives it the knock, made in code (`MadeSounds::knock`), so there's no sound file. A system without AudioPadd runs silent. The `Sound` control turns the knocks off.

**The window shows** the steps and sub-steps in use, how busy the physics thread is (its share of one core over the last second), the joints' stretch now and the worst since the last reset (each joint is drawn green within its slack, red beyond it), the knocks sent to AudioPadd, and how many snapshots and frames arrive a second.

All the messages are plain structs in `src/RagdollMessages.hpp`, with no Cyclone, GL or ImGui dependency.

## Tests

| Test | Covers |
| --- | --- |
| `SnapshotTimeline_test` | Blending halfway (position and turn); drawing a delay behind, blended between the snapshots either side; holding the oldest or newest outside them; not blending; a reset not blended; arrival jitter eased, and catching up after a pause |
| `RagdollPhysicsPadd_test` | The physics padd in a running system with a stand-in window: whole snapshots arriving, their time keeping up with real time at 60 steps a second in 2 sub-steps; the controls setting 240 steps a second and 4 sub-steps (snapshots then whole numbers of 1/240 s apart); R starting a new generation with its stretch measured afresh; P turning `Paused` on and stopping time; with a stand-in AudioPadd, the knock loaded and the falling ragdoll's hits asked to play (each at a knock's volume, panned no further than 0.8), and none with `Sound` off |

## Not done yet

- **One scene:** only the ragdoll. The other demos' scenes (stacks, the explosion) would each need their physics split from their drawing, as the ragdoll's already is.
