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
| `stack` | A tower of boxes that are never allowed to sleep, held up by resolving the contacts together; M switches to the book's resolver, and the tower creeps and falls | 1-9 and 0 rebuild it with that many boxes (0 is ten); plus the rigid-body keys |
| `flightsim` | An aircraft with aerodynamic control surfaces | W/S pitch, A/D roll, Q/E yaw, X centres the controls, R resets |
| `flightsim_flex` | The flight sim on `cyclone_flex` | As `flightsim`, plus `[` and `]` step the engine's precision (down to 2 bits), N cycles stochastic, truncating and nearest rounding, F returns to full precision |
| `sailboat` | A boat with a sail and buoyancy | Q/E trim the sail, W centres it |

The rigid-body demos (`bigballistic`, `explosion`, `fracture`, `ragdoll`, `stack`) start paused and share these keys: P pauses or resumes, Space advances one frame, R resets, C toggles drawing contacts, M switches the contact resolver (see [Resolving contacts together](#resolving-contacts-together); the one in use is shown bottom right). Drag with the mouse to turn the camera. Pass `--run` to start them running, e.g. `bazel run //projects/cyclone:fracture -- --run`.

Every demo also takes `--keys "SECONDS:INPUT,..."`, which plays input into it so its controls can be tested without touching the keyboard: at each time (seconds after launch) each character of INPUT goes to the demo's key handler, or INPUT `click` is a left click. For example `bazel run //projects/cyclone:bigballistic -- --run --keys "1:2,1.5:click,4:r"` picks artillery, fires it and resets.

## Resolving contacts together

`ContactResolver` can resolve contacts two ways, chosen with `setMethod` (`World::getResolver()` gives a world's resolver):

```cpp
resolver.setMethod(cyclone::ContactResolver::Method::sequentialImpulses);
```

- **`Method::oneContactAtATime`** (the default) is the book's resolver: it removes overlap, then fixes velocities, one contact at a time, the worst first. It's fast and handles impacts well, but while it resolves one contact it ignores friction at the others, so an awake stack creeps sideways and a tall one falls (see [Known limitations](#known-limitations)).
- **`Method::sequentialImpulses`** resolves all the contacts together: sequential impulses (Catto, *Iterative Dynamics with Temporal Coherence*, GDC 2005), which the book outlines in chapter 18. Awake stacks of ten boxes stand still. One call:
  1. **Wakes** sleeping bodies an awake one is pushing, as the book's resolver does.
  2. **Warm starts** each contact from the impulse it applied last step, matched by where it is on the first body (within 5 cm). Without this, three boxes drift 17 cm and five fall.
  3. **Velocity sweeps** (40 by default) visit every contact in turn, keeping a running total of its push and friction: the push may only push, and friction stays within friction x push (the friction cone), so every contact on a body is satisfied together. Contacts still apart may close the gap this step but no further, so they only push once they touch; touching ones mustn't close, or bounce by the book's rule.
  4. **Move:** `integrate()` has already moved each body by the velocities it started the step with. The resolver moves it as if it had had the resolved velocities instead, which is semi-implicit Euler, as `integrate()` uses. This cancels the step's fall into whatever a body rests on, and because the resolved velocities respect friction, so does the move.
  5. **Settle sweeps** (10) leave the contacts the move has just closed still (or bouncing) rather than closing.
  6. **Position sweeps** (10) remove what overlap is left beyond `positionEpsilon` (80% of it each step), on velocities of their own that only move the bodies (split impulses), so pushing bodies apart doesn't set them moving.

  `setSweeps(velocity, settle, position)` changes the counts. Each sweep visits each contact once, so a step costs (40 + 10 + 10) visits per contact. The book's resolver instead takes up to `setIterations` resolutions of one contact (which `World` sets to 4 x the number of contacts), each scanning every contact.

It took some trying to get here. Each of these was tried and dropped:
- **Removing overlap with a push in the velocity sweeps** (Baumgarte), or pushing and then relaxing (Box2D v3's soft step): the stack swayed like an inverted pendulum. The push acts as a spring.
- **Letting resting contacts sink a few millimetres** (a slop) before they push back: the same sway.
- **Removing overlap only by moving bodies along the contact normals**, after the book's position step (split impulses without the move in step 4): the bodies sank a frame's fall into each other every step, and pushing them back out along slightly tilted normals walked the stack sideways about 3 mm a second. Friction can't stop a push that changes no velocity.

Moving the bodies by the change in velocity (step 4) is what stopped both the sway and the creep.

What it needs:
- **Call it straight after integrating the bodies,** as Cyclone's game loop does (integrate, generate contacts, resolve), with the same duration. Steps 3 and 4 assume `integrate()` has just moved the bodies by their velocities; a contact set up by hand between bodies that haven't moved is resolved as if they had.
- **Call `forgetWarmStart()`** after teleporting bodies or deleting them, so a body allocated at a freed address doesn't start from a deleted body's impulses.
- **Joints work, but they're the book's single contact** at the midpoint between the two anchors, and the ragdoll takes a little longer to come to rest than with the book's resolver (`ragdoll_test`'s "at rest" check fails narrowly). That's why the book's resolver is still the default.

The `stack` demo compares the two methods, and every rigid-body demo switches between them with M.

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
  - The generator interfaces (`ForceGenerator`, `ParticleForceGenerator`, `ContactGenerator`, `ParticleContactGenerator`) had virtual functions but no virtual destructor, so a generator deleted through one of them wasn't destroyed in full. They have one now, which the repo's `-Wnon-virtual-dtor` requires.
  - `ParticleFakeSpring` gave NaN forces for a spring damped too heavily to oscillate (its formula takes the square root of 4 x stiffness - damping^2). It now gives no force there, as its "check they are in bounds" comment intended.
  - **Contacts between boxes are kept from frame to frame** (`BoxContactCache`). `CollisionDetector::boxAndBox` finds one contact, at the deepest point, so a box resting level on another was held up at one corner and rocked: three stacked boxes fell over in 3.5 s. The book (section 13.3.5) meant each frame's contact to be added to those kept for the same pair from earlier frames, remembered by which vertex, face or edge touches and worked out afresh each frame, so a resting box gathers a contact at each corner; Cyclone never had that part. The cache does it: stacks of two and three boxes now stand (tested for 30 s). The explosion and fracture demos use it.
  - **A stack falls asleep as one** (`ContactResolver::sleepInGroups`). Each body used to sleep on its own once it settled, and a sleeping body is woken by an awake one pressing on it (`Contact::matchAwakeState`, as in the book), so the boxes of a stack, settling at slightly different times, kept waking each other and a stack never slept. After resolving contacts, the resolver now joins the bodies they touch into groups (the ground and immovable bodies don't join groups) and puts a group to sleep when every body in it has settled; `RigidBody::integrate` leaves a body in a group to it (the mark lasts one step, so a body that leaves its group sleeps on its own again). Waking is unchanged. Stacks of up to five boxes now fall asleep about 2.5 s after they're built, and then stay put. The book's section 16.2.3 groups contacts the same way, but to speed up resolution rather than for sleep.
  - **`Gravity` woke sleeping bodies:** it adds its force with `addForce`, which wakes the body (as the book's code does), so a body under the gravity force generator could never sleep. The book says forces that are always there, like gravity, shouldn't wake a body. It now leaves sleeping bodies alone. The demos give gravity as an acceleration instead, so none changes.
  - **Particles update their velocity before moving,** as rigid bodies already did (upstream, and the book's section 3.3, moved them with the old velocity first). `ParticleFakeSpring` relies on this order: before, its stiff springs (stiffness 400 and up at 60 steps a second) swung wider and wider; now they hold at every stiffness tried, up to 1,000,000. Particles resting on the ground also sit still now, where before they jittered by a frame of gravity. Every particle demo moves very slightly differently.
- **Contacts can be resolved together** (`ContactResolver::Method::sequentialImpulses`, opt-in; see [Resolving contacts together](#resolving-contacts-together)), so awake stacks stand still. `World::getResolver()` exposes a world's resolver to choose it. The new `stack` demo shows the difference, and M switches resolver in every rigid-body demo.
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
| `contacts_test` | The contact resolver (bounce at restitution x speed, overlap pushed out, head-on elastic collision); joints; particle cables and rods, between particles and to a fixed point; the particle world (a dropped particle, registered forces, a pendulum's length and period); the rigid-body world (a dropped box coming to rest, stacks of two and three boxes standing, sharing out the contact array); sleep (a lone box, stacks falling asleep together, a box dropped on a sleeping stack waking it, knocking out a stack's bottom box, a tall stack set up asleep staying up, a body that can't sleep keeping its group awake, a body that leaves its group sleeping on its own); resolving contacts together (awake stacks of 3, 5, 8 and 10 boxes standing still for 30 s with no creep, while the book's resolver drops 8; a dropped box coming to rest; stacks sleeping and waking; knocking out a stack's bottom box; a sliding box slowing at friction x g; a bounce at restitution x landing speed). Its box world updates each box's collision transform after `World` integrates it, so contacts are found where the boxes are now (finding them a step behind put a sliding box's contacts 8 cm back, and it tipped over) |
| `random_test` | The random number generator: repeatable from a seed; reals, integers, vectors and orientations spread evenly over their ranges; asking for an integer below 0 is an error |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos draw with legacy OpenGL,** which macOS has deprecated (its warnings are silenced with `GL_SILENCE_DEPRECATION`) but still provides. They are tested on macOS; on Linux they link GL and GLU and on Windows opengl32 and glu32, but neither has been tried.
- **With the book's resolver (the default), awake stacks creep, and tall ones fall before they can sleep.**
  - *Creep:* while its boxes are awake, a stack slides sideways a little: three boxes about 3 cm in 30 s, five faster, falling after 15-25 s; eight fall after about 7 s.
  - *Sleep stops it:* a stack of up to five falls asleep within about 2.5 s and then stays put until something knocks it. Eight or more rock and fall within a few seconds of being built, before they settle.
  - *Fix:* resolve the contacts together (`Method::sequentialImpulses`; see [Resolving contacts together](#resolving-contacts-together)). Awake stacks of ten then stand still for as long as they're left; fifteen stand but still sway a little.
  - *Or:* set tall stacks up asleep (`setAwake(false)`), as the book suggests for a level. They then stand until hit.
  - *Cause:* the book explains it (sections 15.5 and 17.3.1). The resolver handles one contact at a time, so the friction at one contact isn't respected while it resolves another. The book's remedies are sleep, which SAKBox now does for stacks, and resolving all the contacts at once (chapter 18).
- **A new stack leans a little before it settles,** with either resolver: `boxAndBox` finds one contact per pair of boxes, and the contact cache gathers the rest of a pair's corners over the first few steps as the box rocks. Until it has them, an upper box is held at one corner. A ten-box stack leans up to about 15 cm at the top in its first seconds, then stands still.
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

1. **Make resolving contacts together the default,** once joints suit it: give a joint its own constraint between its two anchor points, rather than the book's single contact at their midpoint, so the ragdoll comes to rest as quickly as with the book's resolver.
2. **Start a resting pair of boxes with all its corners,** so a new stack doesn't lean while the contact cache gathers them: when `boxAndBox` finds a corner on a face, also add the other corners of the same face that are within the collision tolerance.
