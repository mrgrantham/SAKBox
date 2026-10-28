# Precision Flight

A prototype rail shooter in the spirit of SNES Star Fox, where the numeric precision of the game's subsystems is a resource. Built on [flexmath](../../flexmath/README.md) for the precision and [ScratchPadd](../../scratchpadd/README.md) for its structure.

```bash
bazel run //projects/examples/precision_flight:precision_flight              # fly it yourself
bazel run //projects/examples/precision_flight:precision_flight -- --autopilot
bazel test //projects/examples/precision_flight/...
```

WASD or the arrow keys steer, P pauses, R restarts, T toggles the autopilot, Esc quits.

## The game

Three precision groups decide how finely parts of the game see the world:

| Group | Controls | Default |
| --- | --- | --- |
| Flight | the grid the ship's position snaps to as physics sees it | 1 bit (steps of 0.5) |
| Hull | how finely collisions sample the ship along the rail | 2 bits |
| Render | how finely vertices resolve when drawn | 9 bits |

Powerups change a group's precision for a few seconds:

- **FOCUS** (cyan) raises Flight to 5 bits. Gold gates sit between the default flight grid points, so only FOCUS can thread them.
- **PHASE** (magenta) drops Hull to -1 bits, so collisions sample every 2 units and thin energy walls slip between samples. Render drops to 3 bits, so the world visibly glitches.

The **FlightSim** panel (the precision lab) changes every setting live: each group's bits, the powerup bits, rounding (truncate, nearest or stochastic), lossy velocity and position, the autopilot and the rail speed.

## How it's built

The game runs as five ScratchPadd padds, split for clarity rather than speed: the two game padds share the main thread, so there are no races and messages are handled in a fixed order.

```
FlightDisplayPadd (main thread) ──FrameInput, GameCommand──►  FlightSimPadd (main thread)
        ▲                                                            │
        └──────── WorldSnapshot (one per frame), events ─────────────┤
                  ControlSnapshot (the precision lab)                 │
EventLogPadd (own thread) ◄──────────── events ──────────────────────┤
FlightSoundPadd (own thread) ◄────────── events ──────────────────────┘
        │ LoadSoundSamples, PlaySound
        ▼
AudioPadd (own thread)
```

| Padd | Owns | Receives | Sends |
| --- | --- | --- | --- |
| `FlightSimPadd` | the game: `FlightSim`, and the precision lab as controls | `FrameInput`, `GameCommand`, control messages | events, then one `WorldSnapshot` per frame; `ControlSnapshot` |
| `FlightDisplayPadd` | window, input, rendering (ScratchPadd's [scene layer](../../scratchpadd/README.md#scene-layer); the render precision through its desktop vertex snapping), HUD and control panels ([debug UI](../../scratchpadd/README.md#debug-ui)), and visual-only state (eased ship position, hit flashes, fading messages) | `WorldSnapshot`, events, `ControlSnapshot` | `FrameInput` each frame, `GameCommand` for P/R/T, `ControlRequest`, and `ControlChange` straight to the padd whose controls a panel shows (`sendTo`, to the address its snapshot came from) |
| `EventLogPadd` | terminal output | events | — |
| `FlightSoundPadd` | which sound each event makes (`cueFor`) | events | the sounds, made in code, to AudioPadd once (`LoadSoundSamples`), then a `PlaySound` for each event's cue |
| `AudioPadd` | the sound device (ScratchPadd's [audio](../../scratchpadd/README.md#audio)) | `LoadSoundSamples`, `PlaySound`, its volume controls | — |

**Frames are a chain, not a timer.** The display sends a `FrameInput` (the frame's real time and steering). The simulation advances by that much and answers with what happened (`GatePassed`, `Crashed`, `PowerupCollected`, `WallPhased`, `GameOver`) and a `WorldSnapshot`. Drawing the snapshot waits for vsync and then sends the next `FrameInput`. Because `FlightSimPadd` is listed before `FlightDisplayPadd` in `main.cpp`, each frame draws the snapshot that answers its own input.

**The simulation steps in fixed 1/60 s steps** whatever the frame rate, and the autopilot lives inside it, so an autopilot run is the same however frames slice the time.

**Sound.** A gate chimes, a gold gate higher; hitting an energy wall zaps and clipping a gate's frame clanks, louder the more shield it costs; FOCUS chimes up and PHASE down (precision up, precision down); slipping through a wall whooshes; the game ends with a boom. `FlightSoundPadd` hears the events the simulation already broadcasts, as `EventLogPadd` does, and finds AudioPadd by name, so the simulation knows nothing of sound and a system without AudioPadd runs silent (the tests). The sounds are made in code (`MadeSounds`), so there are no sound files. AudioPadd's panel (master, music and sound volumes, and mute) sits bottom right, below the precision lab.

All message types are plain structs in `src/FlightMessages.hpp`, and the game's rules live in `src/FlightSim.hpp/.cpp` with no graphics or ScratchPadd dependency.

## Tests

| Test | Covers |
| --- | --- |
| `FlightSim_test` | The simulation alone: a 120 s autopilot run must reproduce `tests/data/autopilot_events.txt` exactly (recorded from the original single-file game), as must a run with different lab settings; frame lengths don't change events; powerup overrides; settings; pause and reset |
| `FlightPadds_test` | The padds, driven by a stand-in display: the autopilot run matches the golden log at the driver and in the event log; control changes reach the simulation; the autopilot key updates its control; pause and restart. With a stand-in AudioPadd: each event's cue, and the whole autopilot run asking for each event's sound, in order, after loading the five sounds |
