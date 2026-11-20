# Animation lab

Motions made with physics on a desktop, then baked into something small that
every console can play. Desktop only: the consoles never run it, they play
what it bakes (ScratchPadd's `Motion::BakedMotion`).

## Three ways to make a motion

All three use Cyclone (`projects/cyclone`), and each records where the thing
was at every fixed step (a `Recording`). Each works flat too (`flat`: z stays
0), for motion on a screen rather than over the board.

- **`solveThrow`**: say where it starts, where it lands and how long it
  takes. The throw is worked out (a projectile, Millington's *Game Physics
  Engine Development*, chapter 3), flown in Cyclone, and aimed again by what
  it missed (the integrator's small error) until it lands where asked. A
  natural arc every time.
- **`guideAlong`**: sketch a rough path as keys (where, and when). A spring
  (Cyclone's fake spring: the damped spring's exact motion, stable however
  stiff) pulls the thing after a point moving along the keys, so it lags,
  overshoots and settles: the follow-through of Lasseter's principles, from
  the physics. Stiffness follows closer; less damping overshoots more.
- **`searchThrow`**: for when there's no formula, with air drag or a bounce
  off a floor. Throws are tried around the vacuum's answer, the best fifth
  kept and the next round's guesses narrowed around them (the cross-entropy
  method), then the best polished by small steps along each axis. Seeded, so
  the same search finds the same throw.

## Baking

`bake(recording, pathPoints, timingPoints)` turns a recording into a
`BakedMotion` with its path and its timing kept apart: points evenly spaced
along the way it went (played back on a Catmull-Rom curve through them), and
how far along it is at evenly spaced moments. A path can then be reused with
another timing, or a timing with another path. `bakeError` says how far the
baked motion strays from the recording; 24 points each keep a 4-unit throw
within 3 hundredths of a unit.

## Build and test

```bash
bazel test //projects/animation_lab:Lab_test
```

Still to come: the lab's desktop app (sliders for each way's settings, the
motion drawn as it's made, and the baked one played beside it, saved for the
games).
