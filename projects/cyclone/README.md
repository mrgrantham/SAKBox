# Cyclone

Cyclone is a rigid-body physics engine by Ian Millington, written to accompany his book *Game Physics Engine Development*. It simulates particles, mass-aggregate systems (particles joined by springs, cables and rods) and full rigid bodies with contact generation and resolution.

It was originally published at [idmillington/cyclone-physics](https://github.com/idmillington/cyclone-physics). That repository stopped changing in 2020, so SAKBox maintains its own copy: it builds with Bazel and strict C++26, has tests, and carries the fixes and additions described below. It's licensed under the MIT licence (see [`LICENSE`](LICENSE)); the original "Icosagon" copyright headers are kept in each file.

```bash
bazel build //projects/cyclone/...
bazel test //projects/cyclone/...
bazel run //projects/cyclone:fracture      # or any demo below
```

## Libraries

| Target | What it is |
| --- | --- |
| `//projects/cyclone:cyclone` | The engine, in double precision. Include `<cyclone/cyclone.h>`. |
| `//projects/cyclone:cyclone_flex` | The same engine with `cyclone::real` as flexmath's `FlexReal`, so the whole engine runs at a precision you can change while it runs (see [flexmath](../flexmath/README.md#using-flexmath-with-cyclone)). |
| `:app`, `:app_flex`, `:timing` | The demo framework (GLFW window, camera, timing, text, sphere and cube drawing), for each engine build. |

The engine libraries are plain C++ with no graphics or platform dependency; only the demo framework links GLFW and OpenGL.

The main pieces of the engine, in `include/cyclone/`:

| Header | Contents |
| --- | --- |
| `core.h` | `Vector3`, `Quaternion`, `Matrix3`, `Matrix4` |
| `precision.h` | The `real` type and math functions (double, or `FlexReal` in the flex build) |
| `particle.h`, `pfgen.h`, `plinks.h`, `pcontacts.h`, `pworld.h` | Particles, their force generators, links (cables, rods), contacts and world |
| `body.h`, `fgen.h`, `joints.h`, `world.h` | Rigid bodies, force generators (gravity, springs, aerodynamics, buoyancy), joints and world (`world.h` isn't in `cyclone.h`; include it directly) |
| `collide_coarse.h`, `collide_fine.h`, `contacts.h` | Broad-phase bounding volumes, primitive collision tests (spheres, boxes, planes), and contact resolution |
| `random.h` | A random number generator for the demos |

## Demos

Each demo is a binary: `bazel run //projects/cyclone:<name>`. They open a GLFW window and draw with legacy (fixed-function) OpenGL and GLU.

| Demo | Shows | Controls |
| --- | --- | --- |
| `ballistic` | Particle projectiles | Click to fire; 1-4 choose pistol, artillery, fireball or laser |
| `bigballistic` | Rigid-body projectiles hitting boxes | Click to fire; 1-4 choose the shot; plus the rigid-body keys |
| `fireworks` | Particle fireworks spawning more fireworks | 1-9 launch different firework types |
| `bridge` | A rope bridge of particles, cables and rods with a moving mass | W/A/S/D move the mass |
| `platform` | A platform of particles and rods with a moving mass | W/A/S/D move the mass |
| `blob` | Particles held together as a jelly-like blob | W/A/S/D push the blob; R resets |
| `explosion` | Boxes and balls tumbling into a heap, then blown apart by an `Explosion`: an implosion, a shock wave (drawn as a red ring) that throws them out spinning, and a chimney of hot air | X sets off the explosion, E edit mode, T up mode, W wakes every body; plus the rigid-body keys |
| `fracture` | A block that shatters when hit | Rigid-body keys |
| `ragdoll` | A ragdoll made of jointed bones | Rigid-body keys |
| `flightsim` | An aircraft with aerodynamic control surfaces | W/S pitch, A/D roll, Q/E yaw, X centres the controls, R resets |
| `flightsim_flex` | The flight sim on `cyclone_flex` | As `flightsim`, plus `[` and `]` step the engine's precision (down to 2 bits), N cycles stochastic, truncating and nearest rounding, F returns to full precision |
| `sailboat` | A boat with a sail and buoyancy | Q/E trim the sail, W centres it |

The rigid-body demos (`bigballistic`, `explosion`, `fracture`, `ragdoll`) start paused and share these keys: P pauses or resumes, Space advances one frame, R resets, C toggles drawing contacts. Drag with the mouse to turn the camera. Pass `--run` to start them running, e.g. `bazel run //projects/cyclone:fracture -- --run`.

Every demo also takes `--keys "SECONDS:INPUT,..."`, which plays input into it so its controls can be tested without touching the keyboard: at each time (seconds after launch) each character of INPUT goes to the demo's key handler, or INPUT `click` is a left click. For example `bazel run //projects/cyclone:bigballistic -- --run --keys "1:2,1.5:click,4:r"` picks artillery, fires it and resets.

## What SAKBox has changed

Compared with the last upstream version:

- **Build:** Bazel targets, strict C++26 (`-std=c++26 -pedantic-errors`) and `-Werror`, LF line endings, and clang-format (repo style). The format-only commit is listed in `.git-blame-ignore-revs` at the repo root.
- **The flex build:** `CYCLONE_FLEX_REAL` in `precision.h` makes `real` flexmath's `FlexReal`. In that build, time steps use `real_duration` (a `double`), and `RigidBody::integrate` and `Particle::integrate` compute the time step and drag factors at full precision inside `CYCLONE_FULL_PRECISION_SCOPE`. Only the bodies' state snaps, because snapping the time step or drag would bias every frame the same way.
- **Bug fixes:**
  - `Matrix3::setOrientation` and `Matrix4::setOrientationAndPos` built the transposed matrix, rotating the opposite way. They now match the rigid-body transform.
  - `CollisionDetector::sphereAndSphere` placed the contact point outside the first sphere instead of midway between the two, so friction spun that sphere the wrong way.
  - `Application`, the demos' base class, lacked a virtual destructor although demos are deleted through it.
  - The ragdoll flew away instead of falling over. It was pushed with forces of 500-1000 N applied for one frame, so the push grew with the frame length: 0.05 s frames launched it at 65 m/s. The push is now a velocity change (a sideways shove and a twist of the chest), the same at any frame rate. Its physics lives in `src/demos/ragdoll/ragdoll_physics.h`, shared with `ragdoll_test`.
  - Buoyancy pushed down on a partly submerged body (`Buoyancy` and `ParticleBuoyancy`): the submerged fraction was `(depth - maxDepth - waterHeight)`, which is negative, and in `Buoyancy` it was also multiplied by `maxDepth` instead of divided. The sailboat sank about 4 m and bobbed underwater indefinitely; it now floats. (The demo's camera follows the boat, which hid this.)
  - Springs only ever pulled (`Spring` and `ParticleSpring` took `real_abs` of the stretch), so a squashed spring pulled tighter instead of pushing back. `ParticleBungee` pushed the particle away when stretched. No demo uses these.
  - `CollisionDetector::boxAndHalfSpace` put each contact point below the box's corner by the full penetration, not halfway to the plane as its comment says. When the contact array filled up it returned without recording the contacts it had written.
  - `IntersectionTests::boxAndBox` said boxes with parallel edges (any two unrotated boxes, for one) never overlapped, because the zero cross product of those edges was tested as a separating axis.
  - The bounding-volume tree (`BVHNode`) didn't compile once used (it treated its volume as a pointer), and `getPotentialContacts` only paired bodies across the root's two halves, missing pairs within either half. No demo uses it.
  - `RigidBody` left `isAwake`, `canSleep` and `motion` uninitialised (it has no constructor), and `setCanSleep` reads `isAwake`. They now start asleep, allowed to sleep, and still. Bodies created with `new RigidBody()` used to be zero-filled, so they could never sleep: fracture's broken pieces and explosion's bodies now fall asleep once they settle (drawn blue), as those demos are written to expect.
  - `Random::seed` multiplied by a signed `long` constant that overflowed (undefined behaviour); it is unsigned now, with the same sequence. `randomReal` built its result by writing one member of a union and reading another (also undefined behaviour); it uses `std::bit_cast`, with the same results.
  - `Random::randomQuaternion` normalised four components drawn from [0, 1), so every rotation was about an axis in one octant, by up to 180 degrees. It now gives every orientation equally often (Shoemake's method). Only the explosion demo uses it, to place its boxes.
  - `Aero` and `AeroControl` added the wind to the body's velocity, instead of subtracting it, so the air seemed to flow the wrong way: a still body was pushed into the wind. The sailboat (the one demo with wind) is now pushed downwind by its sail; the flight sim has no wind, so it is unchanged.
  - `World`, the rigid-body counterpart of `ParticleWorld`, couldn't be used: it kept its bodies and contact generators in private linked lists with no way to add to them, and its force registry was commented out. It now has `getBodies()`, `getContactGenerators()` and `getForceRegistry()`, like `ParticleWorld`, and applies the registered forces each step. No demo uses it.
  - `ForceRegistry::remove` and `clear`, and the same in `ParticleForceRegistry`, were declared but never written, so calling them failed to link. `AngledAero` was declared but never defined, and is removed.
  - `Explosion` was declared (from the book) but never written: its constructor was missing, its rigid-body `updateForce` was empty and its particle one was pure virtual, so it couldn't be created. It now implements the book's three stages (implosion, shock wave, convection chimney; see `fgen.h`), with two of the book's printed formulas corrected to match its text. Time moves on with `advanceTime()`, once per step. The explosion demo sets one off with X; upstream's demo only had an unused `fire()` that pushed one ball.
  - `Matrix3::setBlockInertiaTensor` used 0.3 where the book's formula gives a third, so every box turned 10% too easily.
  - `Random::randomInt(0)` divided by zero. A limit of 0 is now an assertion failure (there's no integer below 0).
  - `ParticleFakeSpring` gave NaN forces for a spring damped too heavily to oscillate (its formula takes the square root of 4 x stiffness - damping^2). It now gives no force there, as its "check they are in bounds" comment intended.
  - **Contacts between boxes are kept from frame to frame** (`BoxContactCache`). `CollisionDetector::boxAndBox` finds one contact, at the deepest point, so a box resting level on another was held up at one corner and rocked: three stacked boxes fell over in 3.5 s. The book (section 13.3.5) meant each frame's contact to be added to those kept for the same pair from earlier frames, remembered by which vertex, face or edge touches and worked out afresh each frame, so a resting box gathers a contact at each corner; Cyclone never had that part. The cache does it: stacks of two and three boxes now stand (tested for 30 s). The explosion and fracture demos use it.
  - **A stack falls asleep as one** (`ContactResolver::sleepInGroups`). Each body used to sleep on its own once it settled, and a sleeping body is woken by an awake one pressing on it (`Contact::matchAwakeState`, as in the book), so the boxes of a stack, settling at slightly different times, kept waking each other and a stack never slept. After resolving contacts, the resolver now joins the bodies they touch into groups (the ground and immovable bodies don't join groups) and puts a group to sleep when every body in it has settled; `RigidBody::integrate` leaves a body in a group to it (the mark lasts one step, so a body that leaves its group sleeps on its own again). Waking is unchanged. Stacks of up to five boxes now fall asleep about 2.5 s after they're built, and then stay put. The book's section 16.2.3 groups contacts the same way, but to speed up resolution rather than for sleep.
  - **`Gravity` woke sleeping bodies:** it adds its force with `addForce`, which wakes the body (as the book's code does), so a body under the gravity force generator could never sleep. The book says forces that are always there, like gravity, shouldn't wake a body. It now leaves sleeping bodies alone. The demos give gravity as an acceleration instead, so none changes.
  - **Particles update their velocity before moving,** as rigid bodies already did (upstream, and the book's section 3.3, moved them with the old velocity first). `ParticleFakeSpring` relies on this order: before, its stiff springs (stiffness 400 and up at 60 steps a second) swung wider and wider; now they hold at every stiffness tried, up to 1,000,000. Particles resting on the ground also sit still now, where before they jittered by a frame of gravity. Every particle demo moves very slightly differently.
- **Demo options:** `--run` starts the rigid-body demos unpaused; `--keys` plays scripted key presses and clicks.
- **GLFW instead of GLUT:** the demos open a GLFW window (GLUT is deprecated on macOS and tied the demos to it). GLUT's sphere, cube and bitmap text are replaced by `src/demos/shapes.cpp` (the same geometry: GLU spheres, GLUT's cube faces and normals) and `stb_easy_font`. They draw at full Retina resolution, with text and line widths scaled to keep their size on screen.
- **Cleanups:** `Quaternion`'s components are plain members rather than an anonymous struct in a union (a GNU extension), `sprintf` became `snprintf`, constructor initialisers follow declaration order, and unused code is gone.

Changes inside the code are marked with `SAKBox:` comments.

## Tests

| Test | Covers |
| --- | --- |
| `core_test` | `Vector3`, `Quaternion`, `Matrix3` and `Matrix4` maths, and a box's inertia tensor; rigid-body and particle integration against their closed forms; damping; local and world space; sphere-sphere and sphere-plane contacts. Written against the original engine before it was reformatted, so it also guards the cleanups. |
| `ragdoll_test` | The ragdoll demo's physics without a window: it collapses in place when not pushed, and a push knocks it over nearby at both 1/60 s and 0.05 s steps |
| `collision_test` | Box against plane, box and sphere (contact count, depth, normal, point); a full contact array; the box contact cache (gathering a contact at each corner as a box rocks, dropping contacts that part or slide off their face, edge contacts, forget and clear); the yes/no intersection tests agree with contact generation; bounding spheres and the bounding-volume tree's candidate pairs |
| `forces_test` | Each force generator against its formula: gravity (which leaves a sleeping body asleep), springs (stretched and squashed), bungees, drag, buoyancy, the fake spring, and aerodynamic surfaces (drag, lift turning with the body, wind, torque from an off-centre surface, control surfaces blending their tensors), and explosions (each stage's reach, strength and timing, the shock wave's fall-off and dependence on an object's speed, spinning rigid bodies, a ring of objects thrown outwards); the force registries' add, remove and clear; plus the sailboat demo's boat floating |
| `contacts_test` | The contact resolver (bounce at restitution x speed, overlap pushed out, head-on elastic collision); joints; particle cables and rods, between particles and to a fixed point; the particle world (a dropped particle, registered forces, a pendulum's length and period); the rigid-body world (a dropped box coming to rest, stacks of two and three boxes standing, sharing out the contact array); sleep (a lone box, stacks falling asleep together, a box dropped on a sleeping stack waking it, knocking out a stack's bottom box, a tall stack set up asleep staying up, a body that can't sleep keeping its group awake, a body that leaves its group sleeping on its own) |
| `random_test` | The random number generator: repeatable from a seed; reals, integers, vectors and orientations spread evenly over their ranges; asking for an integer below 0 is an error |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos draw with legacy OpenGL,** which macOS has deprecated (its warnings are silenced with `GL_SILENCE_DEPRECATION`) but still provides. They are tested on macOS; on Linux they link GL and GLU and on Windows opengl32 and glu32, but neither has been tried.
- **Awake stacks creep, and tall ones fall before they can sleep.**
  - *Creep:* while its boxes are awake, a stack slides sideways a little: three boxes about 3 cm in 30 s, five faster, falling after 15-20 s. Each box also rests a few millimetres into the one below.
  - *Sleep stops it:* a stack of up to five falls asleep within about 2.5 s and then stays put until something knocks it. Eight or more rock and fall within a few seconds of being built, before they settle.
  - *Workaround:* set tall stacks up asleep (`setAwake(false)`), as the book suggests for a level. They then stand until hit.
  - *Cause:* the book explains it (sections 15.5 and 17.3.1). The resolver handles one contact at a time, so the friction at one contact isn't respected while it resolves another. The book's remedies are sleep, which SAKBox now does for stacks, and resolving all the contacts at once (chapter 18, which the book only outlines; see the roadmap).
- **`ParticleFakeSpring` rings longer than a real spring.** Each step puts the particle where the real spring would be, but leaves its velocity a little off (the book calls this the velocity mismatch), so the damping comes out weaker: a soft spring (stiffness 4, damping 2) takes about 15 s to settle where a real one takes 5.
- **The convection chimney doesn't depend on an object's speed.** The book says its peak force is worked out like the shock wave's (less for an object already moving with the air), but the chimney has no air speed to compare with, so it pushes the same on still and rising objects.
- **A sphere whose centre is inside a box gets a contact with no direction** (`boxAndSphere`, as in the book): the closest point on the box is the centre itself. It can only happen after a deep overlap.
- **Buoyancy has no g:** `Buoyancy` and `ParticleBuoyancy` apply `density x volume x fraction` as the force, so a fully submerged 1 m^3 of water lifts 1000 units, not 1000 g newtons. The demos are tuned for this, so it is left as is.
- **The sailboat's sea is drawn at y = 0** while its buoyancy uses a water line of 1.6, so the boat floats a little above the drawn tiles.

## Checked against the book

The engine's formulas were checked against Millington's *Game Physics Engine Development* (2007): integration, the force generators, aerodynamics, collision detection, contact resolution and friction, and the explosion. They match it except where SAKBox has fixed a bug (listed above). Some of those bugs are in the book too: its sphere-sphere contact point, bungee and spring signs, buoyancy fraction and box-plane contact point; its rigid-body integrator applies damping twice and its fake spring has two typos in its acceleration (Cyclone's copies of those two were already right). Notes where the book and the code differ in intent:

- **Aerodynamics:** the book defines the tensor's input as "the velocity of the air", but the demos' tensors (negative for drag) are written for the body's velocity through the air, body velocity minus wind, which is what `Aero` now uses. Upstream's body velocity plus wind matched neither.
- **Stacking and the fake spring:** the book's box-box contacts rely on a contact cache, and its fake spring on velocity being updated before position, but its code had neither. Both are now in place (listed above).
- **Sleep:** the book relies on sleep to keep stacks still, and its gravity shouldn't wake bodies, but its body-by-body sleep never let a stack sleep and its force generators wake whatever they push. Stacks now sleep as a group and `Gravity` leaves sleeping bodies asleep (listed above).

## Roadmap

1. **Stop awake stacks creeping,** by resolving the contacts together: for example sequential impulses, which sweep every contact several times keeping a running total of each contact's push and friction (clamped to the friction cone) and start each frame from the last frame's totals (warm starting, using the contact cache to match contacts up). This replaces the book's velocity resolver, not a tweak to it. A quick trial showed that it needs more than that swap: started cold, it was less stable than the book's resolver, and warm-started it kept stale friction that made a lone box slide. It probably also needs contacts that only push once they touch (the cache keeps contacts up to the collision tolerance apart) and a position step that agrees with it.
