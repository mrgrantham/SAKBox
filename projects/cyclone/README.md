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

The rigid-body demos (`bigballistic`, `explosion`, `fracture`, `ragdoll`, `stack`) start paused and share these keys: P pauses or resumes, Space advances one step, R resets, C toggles drawing contacts, M switches the contact resolver (see [Resolving contacts together](#resolving-contacts-together); the one in use is shown bottom right), and `[` and `]` take fewer or more physics sub-steps (see [Sub-steps](#sub-steps)). Drag with the mouse to turn the camera. Pass `--run` to start them running, e.g. `bazel run //projects/cyclone:fracture -- --run`.

**Sound.** The rigid-body demos knock when bodies hit, played with [ScratchPadd's audio engine](../scratchpadd/README.md#audio) through the sound device; V turns it off and on, and the bottom right shows how many knocks have played. After each physics step, `ImpactSounds` (`src/demos/impact_sounds.h`) works out how fast each pair of touching bodies is closing at its fastest contact, and at the end of the frame it knocks for the pairs hitting at over 1 m/s (resting contacts stay quiet): louder the faster (full at 10 m/s), lower the heavier the lighter of the two is (a 1 kg box as made, 100 kg about an octave down, with a little variation each time), and panned to where it happened on screen. A pair knocks at most every 0.1 s, and at most six pairs a frame, the hardest. The explosion's blast has its own sound. The sounds are made in code (`ScratchPadd::Audio::MadeSounds`), so there are no sound files. A burst of hits plays its loudest, newest ones: when every voice is busy the engine takes over the quietest sound playing (see voice stealing in ScratchPadd's README).

Every demo also takes `--keys "SECONDS:INPUT,..."`, which plays input into it so its controls can be tested without touching the keyboard: at each time (seconds after launch) each character of INPUT goes to the demo's key handler, or INPUT `click` is a left click. For example `bazel run //projects/cyclone:bigballistic -- --run --keys "1:2,1.5:click,4:r"` picks artillery, fires it and resets.

## Resolving contacts together

`ContactResolver` can resolve contacts two ways, chosen with `setMethod` (`World::getResolver()` gives a world's resolver):

```cpp
resolver.setMethod(cyclone::ContactResolver::Method::oneContactAtATime); // the book's
```

- **`Method::oneContactAtATime`** is the book's resolver: it removes overlap, then fixes velocities, one contact at a time, the worst first. It's fast and handles impacts well, but while it resolves one contact it ignores friction at the others, so an awake stack creeps sideways and a tall one falls (see [Known limitations](#known-limitations)).
- **`Method::sequentialImpulses`** (the default) resolves all the contacts together: sequential impulses (Catto, *Iterative Dynamics with Temporal Coherence*, GDC 2005), which the book outlines in chapter 18. Awake stacks of ten boxes stand still. One call:
  1. **Wakes** sleeping bodies an awake one is pushing, as the book's resolver does.
  2. **Warm starts** each contact from the impulse it applied last step, matched by where it is on the first body (within 5 cm). Without this, three boxes drift 14 cm and five fall.
  3. **Velocity sweeps** (40 by default) visit every contact in turn, keeping a running total of its push and friction: the push may only push, and friction stays within friction x push (the friction cone), so every contact on a body is satisfied together. Contacts still apart may close the gap this step but no further, so they only push once they touch; touching ones mustn't close, or bounce by the book's rule.
  4. **Move:** `integrate()` has already moved each body by the velocities it started the step with. The resolver moves it as if it had had the resolved velocities instead, which is semi-implicit Euler, as `integrate()` uses. This cancels the step's fall into whatever a body rests on, and because the resolved velocities respect friction, so does the move.
  5. **Settle sweeps** (10) leave the contacts the move has just closed still (or bouncing) rather than closing.
  6. **Position sweeps** (10) remove what overlap is left beyond `positionEpsilon`, on velocities of their own that only move the bodies (split impulses), so pushing bodies apart doesn't set them moving. Each step removes 20% of it, and moves bodies at most 0.2 m, as Box2D and Bullet do (see below). Joints are put back together in full each step (up to the same 0.2 m), measured where the bodies are after the move.

  `setSweeps(velocity, settle, position)` changes the counts. Each sweep visits each contact once, so a step costs (40 + 10 + 10) visits per contact. The book's resolver instead takes up to `setIterations` resolutions of one contact (which `World` sets to 4 x the number of contacts), each scanning every contact.

It took some trying to get here. Each of these was tried and dropped:
- **Removing overlap with a push in the velocity sweeps** (Baumgarte), or pushing and then relaxing (Box2D v3's soft step): the stack swayed like an inverted pendulum. The push acts as a spring.
- **Letting resting contacts sink a few millimetres** (a slop) before they push back: the same sway.
- **Removing overlap only by moving bodies along the contact normals**, after the book's position step (split impulses without the move in step 4): the bodies sank a frame's fall into each other every step, and pushing them back out along slightly tilted normals walked the stack sideways about 3 mm a second. Friction can't stop a push that changes no velocity.

Moving the bodies by the change in velocity (step 4) is what stopped both the sway and the creep.

### Joints

`resolveContacts(contacts, count, duration, joints)` takes the joints separately from the contacts (`World::getJoints()` holds a world's; the ragdoll passes its own). Resolving contacts together, each joint is a constraint between its two anchor points, solved in the same sweeps as the contacts and warm-started from the impulse it keeps (`Joint::impulse`):

- **No slack (`error` 0):** a ball and socket. The anchors are held together in every direction, by an impulse that may push or pull, worked out from a 3x3 effective mass (as Box2D's point-to-point and revolute joints and Bullet's `btPoint2PointConstraint` do).
- **Slack (`error` above 0):** a cable, as the book's joint behaves. It only pulls, along the line between the anchors, once they are `error` apart; until then they may move apart freely, up to the slack this step but no further. This is Box2D's rope joint.
- Each end uses its own anchor, so the lever arms are right. The book's resolver instead gets the contact `Joint::addContact` makes, at the midpoint between the anchors, which is what it always had.
- Bodies joined by a joint wake and sleep together.

The ragdoll comes to rest better this way than with the book's resolver. Over 20 pushes, its fastest bone in the tenth second averages 1.9 cm/s at 60 steps a second and 3.3 cm/s at 20, where the book's resolver leaves it jittering at 3.2 and 9.2 cm/s. Its joints stretch beyond their slack by 1 mm on average (1 cm at 20 steps a second). Stopping collisions between bones a joint connects, which Erin Catto recommends for ragdolls ("Ragdolls", GDC 2012), made no difference, so they still collide. Box2D damps a ragdoll's limbs with friction in each joint; with joints solved this way the ragdoll comes to rest without it.

### How it compares with Box2D and Bullet

Checked against Erin Catto's talks and Box2D's source, and against Bullet's source:

- **The same as both:** sequential impulses with running totals clamped to their limits (Catto, "Modeling and Solving Constraints", GDC 2009); warm starting; friction limited by friction x push; restitution only above a closing-speed threshold (Box2D's `b2_velocityThreshold`; here the book's 0.25 m/s, and the book leaves out the speed gravity added this step); new positions from the new velocities (symplectic Euler, Catto 2009); contacts that only push once they touch (speculative contacts, Catto, "Continuous Collision", GDC 2013).
- **Overlap:** removed on velocities of their own that don't change the bodies' real ones, as Bullet's split impulse does (`m_splitImpulse`), 20% a step as in Bullet (`m_erp2`) and Box2D (`b2_baumgarte`), at most 0.2 m a step as in Box2D (`b2_maxLinearCorrection`).
- **Warm starting matches a contact to last step's** by the nearest point on the first body within a distance, as Bullet's `btPersistentManifold::getCacheEntry` does. Box2D matches by which edges or corners touch.
- **Joints:** Box2D's rope joint is the cable here, line for line: slack, pull only, speculative while slack, overstretch corrected and capped. The ball and socket is Box2D's and Bullet's point-to-point constraint.
- **Friction:** clamped to a circle (the friction cone). Box2D is 2D, with one friction direction; Bullet clamps two directions separately by default (a pyramid).
- **Sub-steps:** Box2D v3 takes several smaller steps per step (its "soft step"), finding contacts once a step and solving them in each sub-step. Here each sub-step is a whole step, contacts found afresh (see [Sub-steps](#sub-steps)): more work, the same physics as smaller steps.

What it needs:
- **Call it straight after integrating the bodies,** as Cyclone's game loop does (integrate, generate contacts, resolve), with the same duration. Steps 3 and 4 assume `integrate()` has just moved the bodies by their velocities; a contact set up by hand between bodies that haven't moved is resolved as if they had.
- **Call `forgetWarmStart()`** after teleporting bodies or deleting them, so a body allocated at a freed address doesn't start from a deleted body's impulses.
- **Pass joints to `resolveContacts`** (or `World::getJoints()`), not as contact generators, so they are solved between their anchors.

The `stack` demo compares the two methods, and every rigid-body demo switches between them with M.

## Sub-steps

Splitting each step into smaller ones makes joints hold far better when something hits, and swings lose less energy, for that many times the physics work. `World::setSubSteps(n)` has `runPhysics(duration)` take `n` steps of `duration / n`, each applying the force generators, integrating, finding contacts and resolving them: the same as `n` times as many whole steps (`contacts_test` checks the pendulum matches). Forces added to bodies by hand before `runPhysics` act through every sub-step, so a frame gives the same push however it's split. It can be changed between calls.

The rigid-body demos step the same way (`RigidBodyApplication::runStep`): `[` and `]` set 1 to 16 sub-steps, and the bottom right shows how many and how long the physics takes a frame. The ragdoll shows, bottom left, the most any joint has stretched since R.

Over 20 pushed ragdolls (`ragdoll_test`), the most any joint stretched beyond its slack:

| Steps a second | 1 sub-step | 2 | 4 | 8 |
| --- | --- | --- | --- | --- |
| 60 | 6.7 cm | 0.9 cm | 0.03 cm | 0.003 cm |
| 20 | 62 cm | 21 cm | 2.6 cm | 0.3 cm |

The demos' physics runs in **fixed steps** of 1/60 s (`FixedSteps`, `src/demos/fixed_steps.h`): each frame adds its time, and runs the whole steps it covers. Frames used to step by their own length, so the simulation changed with the screen's refresh rate and with every stutter. A frame over 0.1 s (a stall) counts as 0.1 s, so the simulation slows rather than taking a burst of steps to catch up; a step is due up to a quarter of a step early, so 60 Hz frames of 16 and 17 ms take one step each. Drawing isn't blended between steps, so on a screen whose rate isn't a multiple of 60 motion is a little uneven (roadmap).

## What SAKBox has changed

Compared with the last upstream version:

- **Build:** Bazel targets, strict C++26 (`-std=c++26 -pedantic-errors`) and `-Werror`, LF line endings, and clang-format (repo style). The format-only commit is listed in `.git-blame-ignore-revs` at the repo root.
- **The flex build:** `CYCLONE_FLEX_REAL` in `precision.h` makes `real` flexmath's `FlexReal`. In that build, time steps use `real_duration` (a `double`), and `RigidBody::integrate` and `Particle::integrate` compute the time step and drag factors at full precision inside `CYCLONE_FULL_PRECISION_SCOPE`. Only the bodies' state snaps, because snapping the time step or drag would bias every frame the same way.
- **Sound** in the rigid-body demos (see [Demos](#demos)).
- **Bug fixes:**
  - `Matrix3::setOrientation` and `Matrix4::setOrientationAndPos` built the transposed matrix, rotating the opposite way. They now match the rigid-body transform.
  - `CollisionDetector::sphereAndSphere` placed the contact point outside the first sphere instead of midway between the two, so friction spun that sphere the wrong way.
  - `CollisionDetector::boxAndSphere` gave no normal when the sphere's centre was inside the box, which a fast sphere reaches in one step: the closest point on the box was the centre itself, so the normal was a zero vector, and resolving the contact turned both bodies' state to NaN. In `bigballistic` the first laser shot did it: the box it hit vanished, and every later shot "hit" the vanished box as it left the gun, so it was never seen. The sphere is now pushed out through the box's nearest face, as Box2D and Bullet do. (The demo's rounds also kept their orientation and spin when reused; they're reset now.)
  - The particle contact resolver corrected contacts by stale moves. After each resolve it updates the other contacts' penetration by how far that resolve moved the particles, but `ParticleContact::resolveInterpenetration` returned early, leaving its last move in place (from an earlier frame, or uninitialised), when a contact was closing without overlapping. Other contacts were then counted as resolved when they weren't, and particles tied by rods could fly apart, worse the more iterations. It now counts as no move. Found by `stress_test`.
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
  - **A pair of boxes gets the whole area where they touch at once.** Gathering one corner a frame, a new box was held at a single corner for its first few frames, which tipped it: a new ten-box stack leant about 15 cm before it stood still. Dirk Gregorius explains it ("Robust Contact Creation for Physics Simulations", GDC 2015): a manifold built one point a frame needs at least four frames to hold a box, and until then adds a torque. When `boxAndBox` finds a corner on a face, the cache now also adds the other points where that face and the most nearly facing face of the other box touch, the points clipping one face against the other finds (Erin Catto, "Contact Manifolds", GDC 2007):
    - each corner of either face that is over the other face;
    - each edge of the other box's face where it crosses the side of this face, at its depth below this face, along this face's normal. This is a new kind of kept contact (`BoxContactFeature::EdgeCrossingFaceOfOne` and `...FaceOfTwo`), because the edge-on-edge contact takes its normal from the two edges, which goes wrong for nearly parallel edges.

    A point found twice (where the faces' corners meet, each box's corner is on the other's face) is kept once. If there are more than eight, the eight that best cover the area are kept (Gregorius's choice: the deepest, the farthest from it, the largest triangle, then the most area outside it). Gregorius keeps four, but with four the book's resolver dropped eight awake boxes within a second. A new ten-box stack now leans under 2 cm, and with the contacts resolved together, ten boxes move under 2 cm in all and fifteen under 10 cm.
  - **`boxAndBox` prefers faces when axes tie.** Two boxes resting flat have several separating axes nearly tied for the shallowest, and which one won flipped between frames with rounding, moving the contact from face to face or to an edge (Catto calls it "feature flip-flop"). An axis tested later must now be clearly shallower to win: 0.95 x the best so far, less 1% of the smaller box's smallest half-size (Box2D-Lite's tolerances). Box one's faces are tested first, then box two's, then pairs of edges, so faces win ties over edges.
  - **A stack falls asleep as one** (`ContactResolver::sleepInGroups`). Each body used to sleep on its own once it settled, and a sleeping body is woken by an awake one pressing on it (`Contact::matchAwakeState`, as in the book), so the boxes of a stack, settling at slightly different times, kept waking each other and a stack never slept. After resolving contacts, the resolver now joins the bodies they touch into groups (the ground and immovable bodies don't join groups) and puts a group to sleep when every body in it has settled; `RigidBody::integrate` leaves a body in a group to it (the mark lasts one step, so a body that leaves its group sleeps on its own again). Waking is unchanged. Stacks of up to five boxes now fall asleep about 2.5 s after they're built, and then stay put. The book's section 16.2.3 groups contacts the same way, but to speed up resolution rather than for sleep.
  - **`Gravity` woke sleeping bodies:** it adds its force with `addForce`, which wakes the body (as the book's code does), so a body under the gravity force generator could never sleep. The book says forces that are always there, like gravity, shouldn't wake a body. It now leaves sleeping bodies alone. The demos give gravity as an acceleration instead, so none changes.
  - **Particles update their velocity before moving,** as rigid bodies already did (upstream, and the book's section 3.3, moved them with the old velocity first). `ParticleFakeSpring` relies on this order: before, its stiff springs (stiffness 400 and up at 60 steps a second) swung wider and wider; now they hold at every stiffness tried, up to 1,000,000. Particles resting on the ground also sit still now, where before they jittered by a frame of gravity. Every particle demo moves very slightly differently.
- **Contacts are resolved together** (`ContactResolver::Method::sequentialImpulses`, the default since joints became constraints; `setMethod(Method::oneContactAtATime)` brings back the book's resolver; see [Resolving contacts together](#resolving-contacts-together)), so awake stacks stand still. Joints are then constraints between their two anchor points (see [Joints](#joints)), which `resolveContacts` takes separately from the contacts, and `World` holds in `getJoints()`. `World::getResolver()` exposes a world's resolver to choose it. The new `stack` demo shows the difference, and M switches resolver in every rigid-body demo.
- **Sub-steps** (`World::setSubSteps`, and `[` and `]` in the rigid-body demos), and the rigid-body demos run in fixed steps rather than steps as long as each frame; see [Sub-steps](#sub-steps). A demo that acts on a step's contacts does it in `afterStep()` (fracture splits the block there, after the step that hit).
- **Demo options:** `--run` starts the rigid-body demos unpaused; `--keys` plays scripted key presses and clicks.
- **GLFW instead of GLUT:** the demos open a GLFW window (GLUT is deprecated on macOS and tied the demos to it). GLUT's sphere, cube and bitmap text are replaced by `src/demos/shapes.cpp` (the same geometry: GLU spheres, GLUT's cube faces and normals) and `stb_easy_font`. They draw at full Retina resolution, with text and line widths scaled to keep their size on screen.
- **Cleanups:** `Quaternion`'s components are plain members rather than an anonymous struct in a union (a GNU extension), `sprintf` became `snprintf`, constructor initialisers follow declaration order, and unused code is gone.

Changes inside the code are marked with `SAKBox:` comments.

## Tests

| Test | Covers |
| --- | --- |
| `core_test` | `Vector3`, `Quaternion`, `Matrix3` and `Matrix4` maths, and a box's inertia tensor; rigid-body and particle integration against their closed forms; damping; local and world space; sphere-sphere and sphere-plane contacts. Written against the original engine before it was reformatted, so it also guards the cleanups. |
| `ragdoll_test` | The ragdoll demo's physics without a window, with both resolvers: it collapses in place when not pushed, and a push knocks it over nearby at both 1/60 s and 0.05 s steps; resolving contacts together, 20 pushed ragdolls come to rest better than with the book's resolver, with their joints stretched under 1.5 cm beyond their slack on average; four sub-steps cut the worst any joint stretches over 20 pushes over ten times, at 60 and 20 steps a second, and it still comes to rest |
| `collision_test` | Box against plane, box and sphere (including a sphere whose centre is inside the box, plain and turned, going out the nearest face, and resolving without NaN; contact count, depth, normal, point); a full contact array; the box contact cache (all four corners of a resting box on the first frame, and still while it rocks; the area where the faces touch for a box turned 10 and 45 degrees, hanging off, and smaller, with its middle over the contacts; dropping contacts that part or slide off their face, edge contacts, forget and clear); the yes/no intersection tests agree with contact generation; bounding spheres and the bounding-volume tree's candidate pairs |
| `forces_test` | Each force generator against its formula: gravity (which leaves a sleeping body asleep), springs (stretched and squashed), bungees, drag, buoyancy, the fake spring, and aerodynamic surfaces (drag, lift turning with the body, wind, torque from an off-centre surface, control surfaces blending their tensors), and explosions (each stage's reach, strength and timing, the shock wave's fall-off and dependence on an object's speed, spinning rigid bodies, a ring of objects thrown outwards); the force registries' add, remove and clear; plus the sailboat demo's boat floating |
| `contacts_test` | The contact resolver (bounce at restitution x speed, overlap pushed out, head-on elastic collision); the particle resolver counting a velocity-only resolve as no move; joints; particle cables and rods, between particles and to a fixed point; the particle world (a dropped particle, registered forces, a pendulum's length and period); the rigid-body world (a dropped box coming to rest, stacks of two and three boxes standing, sharing out the contact array, forces added by hand acting through every sub-step); sleep (a lone box, stacks falling asleep together, a box dropped on a sleeping stack waking it, knocking out a stack's bottom box, a tall stack set up asleep staying up, a body that can't sleep keeping its group awake, a body that leaves its group sleeping on its own); resolving contacts together (awake stacks of 3, 5, 8 and 10 boxes standing still for 30 s with no creep, while the book's resolver drops 8; a dropped box coming to rest; stacks sleeping and waking; knocking out a stack's bottom box; a sliding box slowing at friction x g; a bounce at restitution x landing speed; ten boxes standing still in four sub-steps); joints as constraints (a ball-and-socket pendulum holding to its pivot, losing under 7% of its speed at 60 steps a second and under half that at 240; four sub-steps a step swinging it exactly as 240 whole steps a second; a chain of five staying joined; a cable free until taut, then holding; the book's resolver taking joints as their contacts). Its box world updates each box's collision transform after `World` integrates it, so contacts are found where the boxes are now (finding them a step behind put a sliding box's contacts 8 cm back, and it tipped over) |
| `stress_test` | Random scenes, 200 of each: boxes and spheres (50 g to 500 kg, up to 100 m/s and 20 rad/s, some joined by joints) with both resolvers, and particles joined by rods, cables, anchors, springs and bungees, each at 60 and at 20 steps a second for 10 s (the rigid bodies also at 20 in four sub-steps), every body checked after every step: finite position, velocity, spin and orientation, a unit orientation, under 1,000 m/s and within 10 km. A failure names the seed, step and body. It finds the bugs scenes built by hand miss: with the old `boxAndSphere` it fails on its first scene. (About 60 s.) |
| `random_test` | The random number generator: repeatable from a seed; reals, integers, vectors and orientations spread evenly over their ranges; asking for an integer below 0 is an error |
| `impact_sounds_test` | The demos' knocks, heard through an offline output: a hit knocks and resting doesn't, a pair knocks at most once in its repeat time, only the hardest six hits a frame knock (also over a frame of several steps, where a pair hit in more than one knocks once), turned off nothing plays, and a hit sounds on the side of the screen it happened (the camera turned either way) |
| `fixed_steps_test` | The demos' physics clock: one step a frame at 60 Hz despite 16 and 17 ms frames; 60 steps a second at 144, 120, 30 and 20 Hz; a 2 s stall counting as 0.1 s; sub-steps splitting the step, between 1 and 16 |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos draw with legacy OpenGL,** which macOS has deprecated (its warnings are silenced with `GL_SILENCE_DEPRECATION`) but still provides. They are tested on macOS; on Linux they link GL and GLU and on Windows opengl32 and glu32, but neither has been tried.
- **With the book's resolver, awake stacks creep, and tall ones fall before they can sleep.**
  - *Creep:* while its boxes are awake, a stack slides sideways a little: three boxes about 3 cm in 30 s, five faster, falling after 15-25 s; eight fall after about 7 s.
  - *Sleep stops it:* a stack of up to five falls asleep within about 2.5 s and then stays put until something knocks it. Eight or more rock and fall within a few seconds of being built, before they settle.
  - *Fix:* resolve the contacts together (`Method::sequentialImpulses`, the default; see [Resolving contacts together](#resolving-contacts-together)). Awake stacks of ten then stand still for as long as they're left (moving under 2 cm in all); fifteen stand, moving under 10 cm.
  - *Or:* set tall stacks up asleep (`setAwake(false)`), as the book suggests for a level. They then stand until hit.
- **Joints, resolved together, stretch for a moment when a chain hits something hard,** and a swinging joint loses a little energy each step.
  - *Stretch:* when the ragdoll's first bone hits the ground, the rest of it is still falling at about 8 m/s, and stopping the whole chain in one step leaves some joints up to 7 cm beyond their slack for a step or two at 60 steps a second (62 cm at 20). More sweeps don't help; the book's resolver stretches them as far. Box2D's documentation says the same of its joint chains.
  - *Energy:* each step takes out the speed that would carry a body off the circle its joint holds it to, which also takes a little of its swing. A pendulum reaches the bottom of its first swing about 6% slower than a real one at 60 steps a second, and less than half that short at 240. This happens in any velocity-level solver.
  - *Fix:* sub-steps (see [Sub-steps](#sub-steps)). Four a step cut the ragdoll's worst stretch to 0.03 cm at 60 steps a second (2.6 cm at 20), and swing the pendulum as 240 steps a second do, for four times the physics work.
  - *Cause:* the book explains it (sections 15.5 and 17.3.1). The resolver handles one contact at a time, so the friction at one contact isn't respected while it resolves another. The book's remedies are sleep, which SAKBox now does for stacks, and resolving all the contacts at once (chapter 18).
- **`ParticleFakeSpring` rings longer than a real spring.** Each step puts the particle where the real spring would be, but leaves its velocity a little off (the book calls this the velocity mismatch), so the damping comes out weaker: a soft spring (stiffness 4, damping 2) takes about 15 s to settle where a real one takes 5.
- **A heavy particle hanging from a much lighter one can't be held.** When a light particle carries a much heavier one by a rod or cable (over about 100 times its mass), the particle resolver's corrections overshoot more each step, and the light one is flung about, at 1,000 m/s within a few seconds in `stress_test`'s worst scenes. Resolving one contact at a time can't hold that case, as Box2D's author notes of iterative solvers in general; the answer is solving the whole chain at once (a direct solver). Keep the masses of particles joined by rods and cables within about 100 times of each other. (The stress test does.)
- **Springs are one-sided:** `ParticleSpring` and `ParticleBungee` pull only the particle they're registered on, as in the book. For a spring between two particles, register one on each, with the same stiffness: one alone has nothing pulling back on the other end, and can feed energy into a scene. A spring too stiff for the step (stiffness / mass x step^2 over about 0.15) gains energy and flies apart, as with any step-by-step integration.
- **The convection chimney doesn't depend on an object's speed.** The book says its peak force is worked out like the shock wave's (less for an object already moving with the air), but the chimney has no air speed to compare with, so it pushes the same on still and rising objects.
- **Buoyancy has no g:** `Buoyancy` and `ParticleBuoyancy` apply `density x volume x fraction` as the force, so a fully submerged 1 m^3 of water lifts 1000 units, not 1000 g newtons. The demos are tuned for this, so it is left as is.
- **The sailboat's sea is drawn at y = 0** while its buoyancy uses a water line of 1.6, so the boat floats a little above the drawn tiles.

## Checked against the book

The engine's formulas were checked against Millington's *Game Physics Engine Development* (2007): integration, the force generators, aerodynamics, collision detection, contact resolution and friction, and the explosion. They match it except where SAKBox has fixed a bug (listed above). Some of those bugs are in the book too: its sphere-sphere contact point, bungee and spring signs, buoyancy fraction and box-plane contact point; its rigid-body integrator applies damping twice and its fake spring has two typos in its acceleration (Cyclone's copies of those two were already right). Notes where the book and the code differ in intent:

- **Aerodynamics:** the book defines the tensor's input as "the velocity of the air", but the demos' tensors (negative for drag) are written for the body's velocity through the air, body velocity minus wind, which is what `Aero` now uses. Upstream's body velocity plus wind matched neither.
- **Stacking and the fake spring:** the book's box-box contacts rely on a contact cache, and its fake spring on velocity being updated before position, but its code had neither. Both are now in place (listed above).
- **Sleep:** the book relies on sleep to keep stacks still, and its gravity shouldn't wake bodies, but its body-by-body sleep never let a stack sleep and its force generators wake whatever they push. Stacks now sleep as a group and `Gravity` leaves sleeping bodies asleep (listed above).

## Roadmap

1. **Cheaper sub-steps,** as Box2D v3's "soft step" takes them: find contacts once a step and only integrate and solve in each sub-step, updating each contact's depth from how its bodies moved. Worth it if finding contacts is what makes sub-steps cost too much (a big scene, or the 3DS); the sub-steps here are the results to match.
2. **Blended drawing between fixed steps:** draw each body between its last two steps' positions, by how far the frame is into the next step, so motion is smooth on screens whose rate isn't a multiple of 60 (it costs a step of delay).
3. **Sub-steps for the particle world** (`ParticleWorld`), and the mass-aggregate demos in fixed steps.
