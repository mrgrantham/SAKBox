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
| `explosion` | Boxes and balls blown apart | E edit mode, T up mode, W wakes every body; plus the rigid-body keys |
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
  - `ParticleFakeSpring` gave NaN forces for a spring damped too heavily to oscillate (its formula takes the square root of 4 x stiffness - damping^2). It now gives no force there, as its "check they are in bounds" comment intended.
- **Demo options:** `--run` starts the rigid-body demos unpaused; `--keys` plays scripted key presses and clicks.
- **GLFW instead of GLUT:** the demos open a GLFW window (GLUT is deprecated on macOS and tied the demos to it). GLUT's sphere, cube and bitmap text are replaced by `src/demos/shapes.cpp` (the same geometry: GLU spheres, GLUT's cube faces and normals) and `stb_easy_font`. They draw at full Retina resolution, with text and line widths scaled to keep their size on screen.
- **Cleanups:** `Quaternion`'s components are plain members rather than an anonymous struct in a union (a GNU extension), `sprintf` became `snprintf`, constructor initialisers follow declaration order, and unused code is gone.

Changes inside the code are marked with `SAKBox:` comments.

## Tests

| Test | Covers |
| --- | --- |
| `core_test` | `Vector3`, `Quaternion`, `Matrix3` and `Matrix4` maths; rigid-body and particle integration against their closed forms; damping; local and world space; sphere-sphere and sphere-plane contacts. Written against the original engine before it was reformatted, so it also guards the cleanups. |
| `ragdoll_test` | The ragdoll demo's physics without a window: it collapses in place when not pushed, and a push knocks it over nearby at both 1/60 s and 0.05 s steps |
| `collision_test` | Box against plane, box and sphere (contact count, depth, normal, point); a full contact array; the yes/no intersection tests agree with contact generation; bounding spheres and the bounding-volume tree's candidate pairs |
| `forces_test` | Each force generator against its formula: gravity, springs (stretched and squashed), bungees, drag, buoyancy, the fake spring, and aerodynamic surfaces (drag, lift turning with the body, wind, torque from an off-centre surface, control surfaces blending their tensors); the force registries' add, remove and clear; plus the sailboat demo's boat floating |
| `contacts_test` | The contact resolver (bounce at restitution x speed, overlap pushed out, head-on elastic collision); joints; particle cables and rods, between particles and to a fixed point; the particle world (a dropped particle, registered forces, a pendulum's length and period); the rigid-body world (a dropped box coming to rest, two stacked boxes standing, sharing out the contact array) |
| `random_test` | The random number generator: repeatable from a seed, and reals, integers, vectors and orientations spread evenly over their ranges |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos draw with legacy OpenGL,** which macOS has deprecated (its warnings are silenced with `GL_SILENCE_DEPRECATION`) but still provides. They are tested on macOS; on Linux they link GL and GLU and on Windows opengl32 and glu32, but neither has been tried.
- **Stacks of three or more boxes topple.** `CollisionDetector::boxAndBox` makes one contact per pair of boxes, at the deepest vertex, so a box resting level on another is held up at one corner. Two stacked boxes still stand (they sink about 2 cm into each other and creep a few centimetres in 30 s); three start rocking at once and the top one falls off after about 3.5 s. Making a contact at each corner of the area where the boxes touch fixes three boxes for a few seconds, but with the resolver as it is, the extra contacts push too hard and even two boxes start rocking and fall in about 20 s. A real fix needs both: corner contacts, and a resolver that shares the push between contacts on the same face.
- **`ParticleFakeSpring` is unstable for stiff springs,** the case it exists for. Its force aims the particle at where an exact spring would put it a frame later, assuming velocity is updated before position. `Particle::integrate` moves the particle with its old velocity first (`RigidBody::integrate` does it the other way round), so the aim is a frame late: at 60 frames a second a soft spring (stiffness 4, damping 2) takes about 15 s to settle instead of 5, and an undamped one of stiffness 400 swings wider and wider without limit. Changing the particle integrator's order would fix it but changes every particle's motion, so it's left for its own change.
- **`Explosion` does nothing:** its rigid-body `updateForce` is empty and its particle one is pure virtual, so it can't be created. No demo uses it.
- **`Random::randomInt(0)` divides by zero.**
- **Buoyancy has no g:** `Buoyancy` and `ParticleBuoyancy` apply `density x volume x fraction` as the force, so a fully submerged 1 m^3 of water lifts 1000 units, not 1000 g newtons. The demos are tuned for this, so it is left as is.
- **The sailboat's sea is drawn at y = 0** while its buoyancy uses a water line of 1.6, so the boat floats a little above the drawn tiles.
- **Particles resting on `GroundContacts` jitter by a frame of gravity:** the contact only exists below y = 0, so on alternate frames the particle has gravity's velocity for one step (invisible in position, about g x dt^2).
