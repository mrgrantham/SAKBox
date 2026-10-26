# Cyclone in padds

Cyclone's ragdoll, with its physics and its window in separate [ScratchPadd](../../scratchpadd/README.md) padds on separate threads, talking only by messages. The physics steps at its own rate in fixed steps and [sub-steps](../../cyclone/README.md#sub-steps), the window draws at the screen's rate, and sliders change the steps while it runs.

```bash
bazel run //projects/examples/cyclone_padds
bazel test //projects/examples/cyclone_padds/...
```

R resets the ragdoll (with a new random shove), P pauses, dragging turns the camera, Esc quits. The panels on the right are each padd's controls.

## How it's built

```
RagdollPhysicsPadd (own thread) ──── RagdollSnapshot, each tick that stepped ────►  RagdollWindowPadd (main thread)
        ▲                                                                                   │
        └──── RagdollCommand (R, P), ControlChange (the panels' edits) ─────────────────────┘
```

| Padd | Owns | Receives | Sends |
| --- | --- | --- | --- |
| `RagdollPhysicsPadd` | the ragdoll (`ragdoll_physics.h`, shared with Cyclone's demo), its contact resolver, a `FixedSteps` clock | `RagdollCommand`, control messages | a `RagdollSnapshot` each tick that took a step; `ControlSnapshot` |
| `RagdollWindowPadd` | the window, drawing, the panels, the camera, a `SnapshotTimeline` | `RagdollSnapshot`, `ControlSnapshot` | `RagdollCommand` for R and P, `ControlRequest`, `ControlChange` to the padd whose panel was edited |

**The physics ticks on its own thread.** Every 1/60 s (`kPhysicsTickSeconds`, its `repeatInterval()`), `RagdollPhysicsPadd` adds the real time since its last tick to its `FixedSteps` clock and takes the whole steps that covers, each in its sub-steps, exactly as the rigid-body demos step (`RigidBodyApplication::runStep`). Then it sends one snapshot: each bone's position, orientation and size, the joints (which two bones, where on each, and their slack), and what it measured. The window never touches the physics' state, and the physics never waits for the window.

**Its controls are the sliders.** `Steps a second` (10 to 240, 60 to start), `Sub-steps` (1 to 16, 2 to start, as in the demos), `Resolver` (all contacts together, or the book's one at a time) and `Paused`. They take effect from the next step. Raising the steps a second and raising the sub-steps run the same physics (see Cyclone's README); both are here to try.

**The window blends between snapshots.** Snapshots arrive at the physics' rate, not the screen's: at 20 steps a second there are 20 a second, and even at 60 the physics' tick and the screen's refresh drift against each other, so some ticks take no step and others two. Drawing the newest snapshot as it is makes the ragdoll jump. `SnapshotTimeline` draws it a little in the past instead (one tick plus one step: 33 ms at 60 steps a second), blended between the two snapshots either side of that moment: positions in a straight line, orientations along the shortest turn. It never draws ahead of the newest snapshot, and a reset isn't blended with the ragdoll before it. It works out how the physics' clock lines up with the window's from when snapshots arrive, easing out their jitter, and catches up at once after a pause. The window's own `Blend between snapshots` control turns blending off, to compare: at 20 steps a second the difference is plain.

**The window shows** the steps and sub-steps in use, how busy the physics thread is (its share of one core over the last second), the joints' stretch now and the worst since the last reset (each joint is drawn green within its slack, red beyond it), and how many snapshots and frames arrive a second.

All the messages are plain structs in `src/RagdollMessages.hpp`, with no Cyclone, GL or ImGui dependency.

## Tests

| Test | Covers |
| --- | --- |
| `SnapshotTimeline_test` | Blending halfway (position and turn); drawing a delay behind, blended between the snapshots either side; holding the oldest or newest outside them; not blending; a reset not blended; arrival jitter eased, and catching up after a pause |
| `RagdollPhysicsPadd_test` | The physics padd in a running system with a stand-in window: whole snapshots arriving, their time keeping up with real time at 60 steps a second in 2 sub-steps; the controls setting 240 steps a second and 4 sub-steps (snapshots then whole numbers of 1/240 s apart); R starting a new generation with its stretch measured afresh; P turning `Paused` on and stopping time |

## Not done yet

- **Sound:** the knocks of Cyclone's demos (`ImpactSounds`) play through the audio engine directly. Here the physics padd would send the hits it finds to `AudioPadd`.
- **One scene:** only the ragdoll. The other demos' scenes (stacks, the explosion) would each need their physics split from their drawing, as the ragdoll's already is.
