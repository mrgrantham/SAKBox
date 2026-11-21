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
bazel test //projects/animation_lab/...
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
  grid. Compared side by side, each run has a view of its own, both seen
  from the same camera (`Renderer::beginView`); else the run compared is
  on top, in orange.

### Moving the view

As Blender's viewport, by default (`OrbitView`: our own small version of
the ideas in Blender's view navigation code, no dependency):

| Input | What it does |
| --- | --- |
| Middle-drag (or left, or Alt-left) | Turn round (a turntable: the horizon stays level) |
| Shift + drag | Pan: what's under the cursor stays under it |
| Ctrl + drag | Zoom |
| Mouse wheel | Zoom toward the cursor |
| Trackpad two-finger scroll | Turn round; with Shift pan; with Ctrl or Cmd zoom |
| 1, 3, 7 (numpad or number row) | Look from the front, the right, the top; with Ctrl the opposite |
| Home (or the numpad's .) | Frame it all |
| Esc during a drag | Put the view back as it was |
| Alt+← and Alt+→ | Rewind: back to the view before, and forward again |

The view frames what's shown when the lab starts, when a run is chosen or
compared, and on Home; otherwise it stays where you put it.

#### Glides, Rewind and the corner widget

From Autodesk Research's papers on 3D navigation ("Safe 3D Navigation" and
"ViewCube", both 2008), kept in the UX guides folder:

- **Glides:** every change of view the lab makes (the numpad's views, Home,
  Rewind, the corner widget's clicks) moves over 0.4 s, eased, the short
  way round, so you see where you went. A drag, pan or zoom takes over from
  a glide at once (`OrbitView::glideTo`).
- **Rewind** (`ViewHistory`): undo for the view.
  - Each view you stop at for a moment (0.3 s) is a step: after a drag, a
    run of zooming, a numpad view or a glide.
  - A strip along the bottom of the 3D view has a dot per step. Drag along
    it and the view moves smoothly through them; letting go settles on the
    nearest. Its arrows, and Alt+← and Alt+→, step back and forward.
  - Nothing's lost: moving after going back adds a step at the end. The
    newest 50 are kept.
- **The corner widget**, top right, chosen under Making → View (kept in
  `view.txt`):
  - **ViewCube** (`ViewCube`): click a face, an edge or a corner to look
    from there (26 views; the cells of an edge or a corner light together).
    Drag it to turn; letting go within 10° of one of the 26 settles on it.
    Drag the compass ring under it (N away from the front) to turn about the
    world's up only. The house above it frames everything. Faces are tinted
    with their axis's colour. It has no roll arrows: the view is a
    turntable, so it never rolls.
  - **Blender's axes** (`AxisGizmo`): click a ball to look from that end of
    its axis (again, once there, for the opposite end); drag it to turn. X
    red, Y green, Z blue, as Blender's; the lab's world has y up, so Y
    points up where Blender's Z would.
- Not done: the paper's thumbnails of each step on Rewind's strip (it would
  need each view drawn to an off-screen picture, which the renderers don't
  do yet).
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
