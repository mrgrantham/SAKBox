# Animation lab

Motions made with physics on a desktop, then baked into something small that
every console can play. Desktop only: the consoles never run it, they play
what it bakes (ScratchPadd's `Motion::BakedMotion`).

## Four ways to make a motion

All four use Cyclone (`projects/cyclone`), and each records where the thing
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

- **`tumbleThrow`**: a thing of a real shape thrown spinning
  onto the floor, bouncing and rolling to rest as that shape would. Its
  shape is its hull's corners (`hullCorners`: a model's points furthest out
  in 26 directions), which Cyclone's `CollisionHull` sets on the floor as
  the book sets a box. Its turns are recorded with its positions.

## Baking

`bake(recording, pathPoints, timingPoints)` turns a recording into a
`BakedMotion` with its path and its timing kept apart: points evenly spaced
along the way it went (played back on a Catmull-Rom curve through them), and
how far along it is at evenly spaced moments; and for a thing that
tumbles, how it's turned at those moments (four numbers a moment: the only
extra a real shape costs the bake). A path can then be reused with
another timing, or a timing with another path. `bakeError` says how far the
baked motion strays from the recording; 24 points each keep a 4-unit throw
within 3 hundredths of a unit.

## Build and test

```bash
bazel test //projects/animation_lab:Lab_test
```

## The window

```bash
bazel run -c opt //projects/animation_lab:animation_lab
```

Four parts:

- **History** (left): every run, its settings saved once they've been still
  a moment (so a drag is one run, not hundreds). Click one to go back to
  it; **vs** plays it beside this one (or on top), in sync, in the 3D view
  and the plots: the comparison. Runs can be named and deleted.
- **Making**: the way (Throw, Guide, Search or Tumble) and its settings
  (drag a value and it's made again at once), Flat for 2D, the shape and
  its size, the bake's path and timing points with how far the bake strays
  and how many floats it takes, and Save.
- **3D** (top right): the shape flying the baked motion as a game would
  play it (or the physics, to compare), its path as a line, over a floor
  grid. Drag to turn the view, scroll to zoom.
- **Plots** (bottom right): from the side, from above, and the timing (how
  far along against time), the physics as lines and the bake as dots.

The shapes (`app/LabShapes.hpp`) are the games' own: Cosmic Hex's die, a
card with its SVG picture, the title rocket, and the example ship and rock
from Blender. Each is centred and 1 across, and its hull is what Tumble
lands on. `Settings` (`include/AnimationLab/Settings.hpp`) is everything
that makes one motion; it's what the history keeps.

Files go in the app's folder (`Storage::Folders`; on a Mac
`~/Library/Application Support/animation_lab/`): `history.txt`, and saved
motions as `motions/NAME.txt` (read back with `readText`).

Still to come: loading a saved motion in the games (the consoles reading the
text, or the build turning it into a header), and dragging the keys and ends
on the views themselves.
