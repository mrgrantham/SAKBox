# Cyclone

Cyclone is a rigid-body physics engine by Ian Millington, written to accompany his book *Game Physics Engine Development*. It simulates particles, mass-aggregate systems (particles joined by springs, cables and rods) and full rigid bodies with contact generation and resolution.

It was originally published at [idmillington/cyclone-physics](https://github.com/idmillington/cyclone-physics). That repository stopped changing in 2020, so SAKBox maintains its own copy: it builds with Bazel and strict C++23, has tests, and carries the fixes and additions described below. It's licensed under the MIT licence (see [`LICENSE`](LICENSE)); the original "Icosagon" copyright headers are kept in each file.

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
| `:app`, `:app_flex`, `:timing`, `:gl` | The demo framework (GLUT window, camera, timing), for each engine build. |

The engine libraries are plain C++ with no graphics or platform dependency; only the demo framework links GLUT and OpenGL.

The main pieces of the engine, in `include/cyclone/`:

| Header | Contents |
| --- | --- |
| `core.h` | `Vector3`, `Quaternion`, `Matrix3`, `Matrix4` |
| `precision.h` | The `real` type and math functions (double, or `FlexReal` in the flex build) |
| `particle.h`, `pfgen.h`, `plinks.h`, `pcontacts.h`, `pworld.h` | Particles, their force generators, links (cables, rods), contacts and world |
| `body.h`, `fgen.h`, `joints.h`, `world.h` | Rigid bodies, force generators (gravity, springs, aerodynamics, buoyancy), joints and world |
| `collide_coarse.h`, `collide_fine.h`, `contacts.h` | Broad-phase bounding volumes, primitive collision tests (spheres, boxes, planes), and contact resolution |
| `random.h` | A random number generator for the demos |

## Demos

Each demo is a binary: `bazel run //projects/cyclone:<name>`. They open a GLUT window.

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

## What SAKBox has changed

Compared with the last upstream version:

- **Build:** Bazel targets, strict C++23 (`-std=c++23 -pedantic-errors`) and `-Werror`, LF line endings, and clang-format (repo style). The format-only commit is listed in `.git-blame-ignore-revs` at the repo root.
- **The flex build:** `CYCLONE_FLEX_REAL` in `precision.h` makes `real` flexmath's `FlexReal`. In that build, time steps use `real_duration` (a `double`), and `RigidBody::integrate` and `Particle::integrate` compute the time step and drag factors at full precision inside `CYCLONE_FULL_PRECISION_SCOPE`. Only the bodies' state snaps, because snapping the time step or drag would bias every frame the same way.
- **Bug fixes:**
  - `Matrix3::setOrientation` and `Matrix4::setOrientationAndPos` built the transposed matrix, rotating the opposite way. They now match the rigid-body transform.
  - `CollisionDetector::sphereAndSphere` placed the contact point outside the first sphere instead of midway between the two, so friction spun that sphere the wrong way.
  - `Application`, the demos' base class, lacked a virtual destructor although demos are deleted through it.
  - The ragdoll flew away instead of falling over. It was pushed with forces of 500-1000 N applied for one frame, so the push grew with the frame length: 0.05 s frames launched it at 65 m/s. The push is now a velocity change (a sideways shove and a twist of the chest), the same at any frame rate. Its physics lives in `src/demos/ragdoll/ragdoll_physics.h`, shared with `ragdoll_test`.
- **Demo option:** `--run` starts the rigid-body demos unpaused.
- **Cleanups:** `Quaternion`'s components are plain members rather than an anonymous struct in a union (a GNU extension), `sprintf` became `snprintf`, constructor initialisers follow declaration order, and unused code is gone.

Changes inside the code are marked with `SAKBox:` comments.

## Tests

| Test | Covers |
| --- | --- |
| `core_test` | `Vector3`, `Quaternion`, `Matrix3` and `Matrix4` maths; rigid-body and particle integration against their closed forms; damping; local and world space; sphere-sphere and sphere-plane contacts. Written against the original engine before it was reformatted, so it also guards the cleanups. |
| `ragdoll_test` | The ragdoll demo's physics without a window: it collapses in place when not pushed, and a push knocks it over nearby at both 1/60 s and 0.05 s steps |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos use GLUT,** which macOS has deprecated; its warnings are silenced with `GL_SILENCE_DEPRECATION`, and the demo framework links the macOS GLUT, OpenGL and Cocoa frameworks, so the demos only build on macOS (the engine libraries build anywhere). Porting the demos to GLFW, like the rest of the repo, would make them cross-platform.
- **Most of the engine has no tests yet:** box collisions, joints, the contact resolver and the force generators are covered only by the demos.
