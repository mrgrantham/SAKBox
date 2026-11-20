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

- **`tumbleThrow`**: a thing of a real shape (the user's B) thrown spinning
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

On the left: the way to make the motion (Throw, Guide or Search) and its
settings (drag a value and it's made again at once), Flat for 2D, the bake's
path and timing points with how far the baked motion strays and how many
floats it takes, and Save (to `animation_lab_motions/NAME.txt`, read back
with `readText`). On the right: the motion from the side and from above, the
physics as a line and the baked path as dots, a marker on each playing it
back on a loop; and the timing, how far along against time.

Still to come: loading a saved motion in the games (the consoles reading the
text, or the build turning it into a header), and dragging the keys and ends
on the views themselves.
