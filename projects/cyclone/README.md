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
| `//projects/cyclone:cyclone` | The engine. Include `<cyclone/cyclone.h>`. Its number type, `cyclone::real`, is `double` unless the build setting `:real` says otherwise (see [The number type](#the-number-type)). |
| `:app`, `:timing` | The demo framework (GLFW window, camera, timing, text, sphere, cube and capsule drawing). A program with its own `main`, such as the [Waffles jumper](../waffles/README.md), opens a demo with `runInWindow` (`<demos/window.h>`). |

The engine libraries are plain C++ with no graphics or platform dependency; only the demo framework links GLFW and OpenGL.

The main pieces of the engine, in `include/cyclone/`:

| Header | Contents |
| --- | --- |
| `core.h` | `Vector3`, `Quaternion`, `Matrix3`, `Matrix4` |
| `precision.h` | The `real` type and math functions (see [The number type](#the-number-type)). For `double` and flexmath's types, `real_sin`, `real_cos`, `real_exp`, `real_pow` and `real_atan2` are correctly rounded (flexmath's `CorrectlyRounded.hpp`), so the engine gives the same results on every platform |
| `particle.h`, `pfgen.h`, `plinks.h`, `pcontacts.h`, `pworld.h` | Particles, their force generators, links (cables, rods), contacts and world |
| `body.h`, `fgen.h`, `joints.h`, `world.h` | Rigid bodies, force generators (gravity, springs, aerodynamics, buoyancy), joints (ball and socket, cable, hinge, cone) and world (`world.h` isn't in `cyclone.h`; include it directly) |
| `collide_coarse.h`, `collide_fine.h`, `contacts.h` | Broad-phase bounding volumes, primitive collision tests (spheres, boxes, capsules, planes), and contact resolution |
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
| `flightsim_flex` | The flight sim on flexmath's `FlexReal` (`--//projects/cyclone:real=flex_real`, always) | As `flightsim`, plus `[` and `]` step the engine's precision (down to 2 bits), N cycles stochastic, truncating and nearest rounding, F returns to full precision |
| `sailboat` | A boat with a sail and buoyancy | Q/E trim the sail, W centres it |

The rigid-body demos (`bigballistic`, `explosion`, `fracture`, `ragdoll`, `stack`) start paused and share these keys: P pauses or resumes, Space advances one step, R resets, C toggles drawing contacts, M switches the contact resolver (see [Resolving contacts together](#resolving-contacts-together); the one in use is shown bottom right), and `[` and `]` take fewer or more physics sub-steps (they start at 4; see [Sub-steps](#sub-steps)). Drag with the mouse to turn the camera. Pass `--run` to start them running, e.g. `bazel run //projects/cyclone:fracture -- --run`.

**Sound.** Every demo has sound, played with [ScratchPadd's audio engine](../scratchpadd/README.md#audio) through the sound device; V turns it off and on, and the bottom right says which. The sounds are made in code (`ScratchPadd::Audio::MadeSounds`), so there are no sound files, and `DemoSounds` (`src/demos/demo_sounds.h`), which the `Application` base holds, plays them: once (`play`), or as loops a demo changes as it goes (`setLoop`: an engine's pitch with its speed), which go quiet while sound is off.

| Demo | Sounds |
| --- | --- |
| Rigid-body demos, the jumper | Knocks when bodies hit; the explosion's blast |
| `ballistic` | A shot as each round is fired: a gunshot, a cannon (lower), a whoosh of fire, a zap; artillery booms where it lands |
| `fireworks` | A whoosh at each launch, and a crackle with a soft boom at each burst, panned to where it is |
| `bridge`, `platform`, `blob` | Knocks as particles hit the ground or a platform, or a cable or rod snaps taut (the bridge clatters as it first drops into its cables, then is quiet) |
| `flightsim` | The engine and the wind, higher (and the wind louder) the faster it flies; a crash booms |
| `sailboat` | The wind, louder and higher as it gets up |

Knocks come from `ImpactKnocks` (`src/demos/impact_knocks.h`): after each physics step it works out how fast each pair of touching bodies (or particles) is closing at its fastest contact, from the contacts the world hands its `contactsFound` hook before resolving them, and at the end of the frame it chooses knocks for the pairs hitting at over 1 m/s (resting contacts stay quiet), which `ImpactSounds` (`src/demos/impact_sounds.h`) plays: louder the faster (full at 10 m/s), lower the heavier the lighter of the two is (a 1 kg box as made, 100 kg about an octave down, with a little variation each time), and panned to where it happened on screen. A pair knocks at most every 0.1 s, and at most six pairs a frame, the hardest. `ImpactKnocks` has no audio dependency (`:impact_knocks`), so other code can play its knocks its own way: the [Cyclone padds example](../examples/cyclone_padds/README.md) and the DS app send them to AudioPadd. A burst of hits plays its loudest, newest ones: when every voice is busy the engine takes over the quietest sound playing (see voice stealing in ScratchPadd's README).

Every demo also takes `--keys "SECONDS:INPUT,..."`, which plays input into it so its controls can be tested without touching the keyboard: at each time (seconds after launch) each character of INPUT goes to the demo's key handler, or INPUT `click` is a left click. For example `bazel run //projects/cyclone:bigballistic -- --run --keys "1:2,1.5:click,4:r"` picks artillery, fires it and resets.

## The number type

`cyclone::real` (`precision.h`) is the engine's number type, picked for the whole build by the setting `//projects/cyclone:real`:

```bash
bazel run //projects/cyclone:ragdoll --//projects/cyclone:real=flex_point16
bazel test //projects/cyclone/... --//projects/cyclone:real=flex_point
```

| Value | `real` is | Notes |
| --- | --- | --- |
| `double` (the default) | `double` | Maths functions correctly rounded, so results match across platforms (see the top-level README's [Floating-point results across platforms](../../README.md#floating-point-results-across-platforms)) |
| `float` | `float` | The system's maths functions |
| `flex_real` | flexmath's `FlexReal` | A double that snaps to a precision that can change as it runs (`flightsim_flex`) |
| `flex_point` | flexmath's `FlexPoint` | Q32.32 fixed point: integer arithmetic, the same on every machine with any build |
| `flex_point16` | flexmath's `FlexPoint16` | Q16.16 fixed point, fast on 32-bit processors without floating-point hardware: the DS build uses it ([On the Nintendo DS](#on-the-nintendo-ds)) |

It sets a define that `precision.h` reads (`CYCLONE_FLEX_POINT16` and so on) and propagates to everything built on Cyclone, so the demos, the tests and the Waffles jumper all build with any of them. `real_binary` and `real_test` (`real.bzl`) build one target with a given type whatever the command line says: `flightsim_flex`, `flex_test`, and `ragdoll_test_flex_point16` and `ragdoll_test_flex_point`, which `bazel test //...` runs.

The engine keeps to what all of them can do: maths through `real_sqrt` and friends, constants as `(real)0.5`, and tiny guards compared with `<=` (`length <= (real)1e-9`), since 1e-9 is zero in fixed point and a zero length must still be caught. In fixed point the range and precision are the limits: see [Known limitations](#known-limitations).

## The world

`World` (`world.h`) runs a whole simulation. Add the bodies, their collision shapes (spheres, boxes and capsules, several to a body if need be), the planes they rest on and the joints between them; then each step, call `startFrame()`, add any forces by hand, and call `runPhysics(seconds)`:

```cpp
cyclone::World world({.subSteps = 2, .restitution = (cyclone::real)0.3});
world.addPlane(ground);           // a CollisionPlane
world.addBody(&body);             // the bodies, shapes and joints are the caller's;
world.addShape(&box);             // the world holds pointers to them
world.addJoint(&joint);
world.startFrame();
world.runPhysics(1.0 / 60);
```

It finds the contacts itself: each shape against each plane, and each pair of shapes whose bounding spheres meet, by the test for that pair of shapes (box pairs through a `BoxContactCache`). Shapes on the same body don't collide, nor do two bodies a joint connects unless the joint's `collideConnected` says so (the ragdoll's do: its bones' spheres keep the limbs from folding through each other). A shape's `filter` (`CollisionFilter`, a category and a mask, as in Box2D) keeps it from what it shouldn't touch: two shapes collide only if each one's category is in the other's mask, and planes have filters too. Contact generators (the book's interface, `getContactGenerators()`) add contacts of their own.

`Settings` holds the most contacts a step, the sub-steps (see [Sub-steps](#sub-steps)), the friction and restitution of every contact, the margin (below) and the book's resolver's iterations. Two hooks let code act as it runs: `beforeSubStep` (a controller setting motors; the Waffles jumper's policy acts there) and `contactsFound` (with the contacts, before they're resolved: the demos' knocks listen there). `getStepContacts()` gives the last step's contacts, and `profile()` how long finding contacts, integrating and resolving took (ms).

Every rigid-body demo, the ragdoll test, the [Cyclone padds example](../examples/cyclone_padds/README.md), the DS app and the Waffles jumper simulate with a `World`. They used to each step their bodies and find their contacts by hand; the demos' framework (`RigidBodyApplication`) now steps the demo's world (`getWorld()`), with `beforeStep()`, `afterStep()` and `contactsFound()` for a demo that acts as it goes (fracture splits its block after the step its ball hit it, bigballistic retires the shots that hit). The ragdoll (`ragdoll_physics.h`) is a world of twelve bones and eleven joints.

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
- A cable's starting speed, which sets how far it may stretch this step, is taken from the velocities the bodies had when `integrate()` moved them, before any warm start. It was taken after the joints set up before it had warm-started, which changes the velocities of any body they share, so in a chain like the ragdoll each joint felt its neighbours' kicks: the ragdoll jittered at rest (1.9 cm/s at 60 steps a second, 3.3 at 20; now 0.3 and 0.15).
- A joint can also limit how its bodies turn: see [Hinges and cones](#hinges-and-cones).

The ragdoll comes to rest better this way than with the book's resolver. Over 20 pushes, its fastest bone in the tenth second averages 0.3 cm/s at 60 steps a second and 0.15 cm/s at 20, where the book's resolver leaves it jittering at 3.2 and 9.2 cm/s. Its joints stretch beyond their slack by 1 mm on average (1 cm at 20 steps a second). (`ragdoll_test` checks the twelfth second instead, 0.16 and 0.09 cm/s: in Q16.16 one push leaves the ragdoll propped up on a limb until then.) In sub-steps it takes longer: an arm left dangling from a shoulder at its full slack swings like a pendulum, slowed only by damping, so with four sub-steps some pushes leave it swinging for over 10 s. Stopping collisions between bones a joint connects, which Erin Catto recommends for ragdolls ("Ragdolls", GDC 2012), made no difference, so they still collide. Box2D damps a ragdoll's limbs with friction in each joint; with joints solved this way the ragdoll comes to rest without it.

### Hinges and cones

`HingeJoint` and `ConeJoint` (`joints.h`) are balls and sockets (`error` 0) that also limit how their two bodies turn relative to each other. The book names hinges among the joints "that restrict the ability of one object to rotate with respect to another", but finds them "quite frankly, inconvenient" with its one-contact-at-a-time resolver (section 17.2.1), and points to chapter 18 instead, where a joint or a motor is made "by adding entries directly into the Jacobian" (section 18.1). Resolving contacts together is the iterative way of solving that chapter's equations, so that's how these are built: each limit on turning is a row of its own, an `AngularConstraint`, solved in the same sweeps as the contacts and the anchors, with a running total clamped to its bounds and warm-started from the last step (`Joint::angularImpulse`), as Erin Catto describes for a revolute joint ("Iterative Dynamics with Temporal Coherence", 2005; "Modeling and Solving Constraints", GDC 2009). Each is an equal and opposite twist (angular impulse) on the two bodies, about one axis:

- **Hold:** stops them turning relative to each other about the axis, twisting either way. A hinge holds the two directions across its axis. The position sweeps turn back what they've drifted, in full, up to 8 degrees a step (`maxAngularCorrection`, Box2D's `b2_maxAngularCorrection`).
- **Limit:** like a contact, it only pushes. The bodies may turn into the room left before the limit this step, and no further (speculative, as contacts are), so it only acts once they reach it. Beyond it by more than 2 degrees (`angularSlop`, Box2D's `b2_angularSlop`), the position sweeps turn them back, up to 8 degrees a step.
- **Motor:** drives their relative turning to a speed, with a twist of up to its torque x the step's duration, so it's as strong at any step length and in sub-steps.

```cpp
cyclone::HingeJoint knee;
knee.set(thigh, cyclone::Vector3(0, -0.2, 0), shin, cyclone::Vector3(0, 0.2, 0), 0); // the anchors
knee.setAxis(cyclone::Vector3(-1, 0, 0)); // in world coordinates; the angle is zero as they are now
knee.setLimits(0, 1.57);                  // radians, growing about the axis by the right-hand rule
knee.setMotor(2, 40);                     // 2 rad/s, with up to 40 N m
world.getJoints().push_back(&knee);
```

`getAngle()` gives a hinge's angle now. `ConeJoint::setAxis(worldAxis, maxAngle)` lets body two's axis tilt any way, up to `maxAngle` from body one's: the swing limit of Bullet's cone-twist joint (it has no twist limit). Both need contacts resolved together; the book's resolver takes only their anchors (`Joint::addContact`). A joint measures its angles from its bodies' orientations as they are (`RigidBody::getOrientationNow`), since the resolver turns bodies without updating their transforms.

A row's starting speed, which a limit uses to work out how far it may turn this step, is taken from the velocities the bodies had when `integrate()` moved them, before any warm start, as a cable's is (see [Joints](#joints)): measured after the anchors' warm start, a hinged pendulum resting on its limit was let through every other step.

### Capsules

`CollisionCapsule` (`collide_fine.h`) is "a cylinder with hemispherical ends", which the book lists among the primitives its contact generation is for (section 13.1), though it only works through spheres and boxes. It's every point within `radius` of a segment along the primitive's Y axis, from `-halfLength` to `+halfLength`: a sphere swept along the segment. So its contacts are a sphere's, at the points of the segment nearest the other shape (`sphereAt` gives the sphere there), made by the book's sphere functions:

- `capsuleAndHalfSpace`: each end touches as a sphere would. A capsule's lowest point is always on an end, and lying flat it touches at both, so it rests level, as a box does on its corners.
- `capsuleAndSphere`: the sphere against the capsule's sphere at the point of its segment nearest the sphere's centre.
- `capsuleAndCapsule`: the spheres at the nearest points of the two segments (Christer Ericson, *Real-Time Collision Detection*, section 5.1.9, which the book recommends for more on collision detection). Side by side (within about 2 degrees of parallel), they touch along a line, so there's a contact at each end of where the segments overlap, and one lies level on the other.
- `capsuleAndBox`: each end that touches the box, as a sphere (`boxAndSphere`); lying across an edge, with neither end touching, the point of the segment nearest the box. How far a point is from a box rises and falls only once along a segment (it's convex), so narrowing in by thirds finds it.

`Matrix3::setCapsuleInertiaTensor(radius, halfLength, mass)` builds a capsule's inertia tensor from the book's cylinder and sphere (appendix A), the ends moved out by the parallel axis theorem. The demos draw one with `solidCapsule`. The [Waffles jumper](../waffles/README.md)'s robot is made of capsules, hinges and a cone.

### How it compares with Box2D and Bullet

Checked against Erin Catto's talks and Box2D's source, and against Bullet's source:

- **The same as both:** sequential impulses with running totals clamped to their limits (Catto, "Modeling and Solving Constraints", GDC 2009); warm starting; friction limited by friction x push; restitution only above a closing-speed threshold (Box2D's `b2_velocityThreshold`; here the book's 0.25 m/s, and the book leaves out the speed gravity added this step); new positions from the new velocities (symplectic Euler, Catto 2009); contacts that only push once they touch (speculative contacts, Catto, "Continuous Collision", GDC 2013).
- **Overlap:** removed on velocities of their own that don't change the bodies' real ones, as Bullet's split impulse does (`m_splitImpulse`), 20% a step as in Bullet (`m_erp2`) and Box2D (`b2_baumgarte`), at most 0.2 m a step as in Box2D (`b2_maxLinearCorrection`).
- **Warm starting matches a contact to last step's** by the nearest point on the first body within a distance, as Bullet's `btPersistentManifold::getCacheEntry` does. Box2D matches by which edges or corners touch.
- **Joints:** Box2D's rope joint is the cable here, line for line: slack, pull only, speculative while slack, overstretch corrected and capped. The ball and socket is Box2D's and Bullet's point-to-point constraint. The hinge is their revolute joint (Box2D's `b2RevoluteJoint`, Bullet's `btHingeConstraint`): two angular rows hold the axis, a limit and a motor act about it; Box2D's limit and motor are rows of the same kind, its correction and slop the same 8 and 2 degrees.
- **Friction:** clamped to a circle (the friction cone). Box2D is 2D, with one friction direction; Bullet clamps two directions separately by default (a pyramid).
- **Sub-steps:** as Box2D v3's "soft step", a `World` finds contacts once a step, a little apart (2 cm, Box2D's speculative distance), and follows them through its sub-steps from how their bodies move, with each contact's anchors fixed (see [Sub-steps](#sub-steps)). Box2D solves once a sub-step; here the sub-steps share the step's sweeps, a few each.

What it needs:
- **Call it straight after integrating the bodies,** as Cyclone's game loop does (integrate, generate contacts, resolve), with the same duration. Steps 3 and 4 assume `integrate()` has just moved the bodies by their velocities; a contact set up by hand between bodies that haven't moved is resolved as if they had.
- **Call `forgetWarmStart()`** after teleporting bodies or deleting them, so a body allocated at a freed address doesn't start from a deleted body's impulses.
- **Pass joints to `resolveContacts`** (or `World::getJoints()`), not as contact generators, so they are solved between their anchors.

The `stack` demo compares the two methods, and every rigid-body demo switches between them with M.

## Sub-steps

Splitting each step into smaller ones makes joints hold far better when something hits, and swings lose less energy, for that many times the solving. `World::setSubSteps(n)` (or `Settings::subSteps`) has `runPhysics(duration)` take `n` sub-steps of `duration / n`, each applying the force generators, integrating and resolving the contacts and joints. Forces added to bodies by hand before `runPhysics` act through every sub-step, so a frame gives the same push however it's split. It can be changed between calls.

**Contacts are found once a step,** after the first sub-step's integration (where the book's game loop finds them), and followed through the rest, as the book suggests (updating penetrations as bodies move, section 14.4; finding contacts a little apart so they're there when needed, its "pessimistic collision detection", section 16.1.4; keeping contact data from step to step, section 16.2.4) and as Box2D v3 does:
- **Found a little apart:** shapes within 2 cm (`Settings::margin`) make contacts, which push only once they touch. Against a plane the margin also covers how far the shape can move in the step, since a contact with a plane is followed exactly; a box falling at 20 m/s lands without sinking in (`world_test`).
- **Followed:** each later sub-step works out a contact's depth from where its two anchors (the contact point on each body, fixed for the step) have moved along its normal (`Contact::followAnchors`), and its bodies' speeds there. The contact's set-up (directions, warm start) is kept for the step; how a push changes the bodies' speeds is worked out again each sub-step, as a body's inertia in the world turns with it.
- **Found after the first sub-step,** not before it: found before and followed through the first sub-step too, a new stack of ten boxes leant 3 cm in its first seconds rather than 1.8.

With one sub-step a step, nothing is followed and no margin is used. With the book's resolver (`Method::oneContactAtATime`) each sub-step finds the contacts afresh, as a whole step, so `n` sub-steps are the same as `n` times as many steps (`contacts_test` checks the pendulum matches).

**Finding contacts once isn't what makes sub-steps cheaper.** Finding them was never much of a sub-step: on a Mac, about 1% of the ragdoll's; on the DS, 1.5 of 9.5 ms. The resolver's sweeps are nearly all of it, and the margin adds contacts to sweep (19 a step for the ragdoll, against 16), so finding them once came out 6-16% slower on a Mac and even on the DS.

**The sweeps are a step's, shared among its sub-steps** (`ContactResolver::setSweeps`): each sub-step takes its share, rounded up (`sweepsEachSubStep`), so Cyclone's 40/10/10 is 10/3/3 in each of 4 sub-steps and 5/2/2 in each of 8, and more sub-steps cost about the same as fewer. Each sweep in a sub-step does more good than one in a whole step, since it starts from velocities a quarter as far off. Until this change each sub-step took all 40/10/10, so 4 sub-steps cost four times one. Box2D v3 goes further, one velocity sweep and one relax a sub-step, but its constraints are soft; with these hard ones a sub-step needs a few more: at 1/1/1 in 8 sub-steps a stack of ten never quite stops moving, at 2/1/1 it stands. The ragdoll on a Mac (20 pushes of 4 s; the stack is ten boxes standing for 30 s):

| Sub-steps | Sweeps each | ms a step | Worst stretch at 60 steps a second | At 20 | Ten boxes lean |
| --- | --- | --- | --- | --- | --- |
| 1 | 40/10/10 | 0.096 | 7.2 cm | 64 cm | 18 mm |
| 2 | 20/5/5 | 0.111 | 0.11 cm | 10 cm | 14 mm |
| 4 | 10/3/3 | 0.114 | 0.004 cm | 0.29 cm | 7 mm |
| 8 | 5/2/2 | 0.145 | 0.03 cm | 0.04 cm | 3 mm |
| 16 | 3/1/1 | 0.191 | 0.02 cm | 0.15 cm | 1 mm |
| 2 (each taking 40/10/10, as before) | 40/10/10 | 0.203 | 0.08 cm | 9.5 cm | 7 mm |

A sub-step still has a cost of its own beyond its sweeps (integrating the bodies, following the contacts, working out how each one's bodies respond as they've turned), which is why 16 cost twice what 1 does. On the DS that cost is most of a sub-step, so there more sub-steps buy steadier joints, not speed (see [On the Nintendo DS](#on-the-nintendo-ds)).

The rigid-body demos step their world this way (`RigidBodyApplication::runStep`): `[` and `]` set 1 to 16 sub-steps, and the bottom right shows how many and how long the physics takes a frame. The ragdoll shows, bottom left, the most any joint has stretched since R.

**The demos start at 4 sub-steps** (`FixedSteps::kDefaultSubSteps`): for about what one costs, the ragdoll's worst stretch goes from 7 cm to under a tenth of a millimetre at 60 steps a second, and from 64 cm to 3 mm at 20 (above). Press `[` for whole steps, or `]` for more. `World` itself still defaults to 1 (`setSubSteps` to change it), so code using the engine directly keeps its results unless it asks for sub-steps.

The demos' physics runs in **fixed steps** of 1/60 s (`FixedSteps`, `src/demos/fixed_steps.h`; `setStepSeconds` changes the length, which the [Cyclone padds example](../examples/cyclone_padds/README.md) offers as a slider): each frame adds its time, and runs the whole steps it covers. Frames used to step by their own length, so the simulation changed with the screen's refresh rate and with every stutter. A frame over 0.1 s (a stall) counts as 0.1 s, so the simulation slows rather than taking a burst of steps to catch up; a step is due up to a quarter of a step early, so 60 Hz frames of 16 and 17 ms take one step each. Drawing isn't blended between steps, so on a screen whose rate isn't a multiple of 60 motion is a little uneven (roadmap).

## On the Nintendo DS

[`ds`](ds/source/main.cpp) runs the ragdoll (the same `ragdoll_physics.h` as the demo) on a Nintendo DS, in [ScratchPadd](../scratchpadd/README.md#on-the-nintendo-3ds-and-ds) padds: `RagdollPadd` steps it on its own thread in fixed steps and sub-steps (`FixedSteps`), timing each step, and `ScreenPadd` draws it with the DS's 3D hardware on the top screen, with the figures on the bottom one. A resets it with a new shove, X changes the sub-steps (1, 2, 4), and Y switches between 60 and 30 steps a second. It knocks as bones hit, as the demos do: `RagdollPadd` hears the world's contacts (`ImpactKnocks`, from when the timing at the start is done) and sends AudioPadd, which plays on the DS's 16 hardware sound channels, a `PlaySound` for each knock; the knock is made in code and rendered when the app is built (`MadeSoundsPcm16`), so the DS does no float work to make it. The bottom screen counts the knocks played. Hearing the hits adds about 1 ms to a step (53 ms with the ragdoll lying down).

```bash
bazel run //projects/cyclone/ds:cyclone-ds-ragdoll   # needs Docker running (tools/devkitpro); opens melonDS
```

It builds Cyclone in Q16.16 fixed point: `real` is flexmath's `FlexPoint16` (`CYCLONE_FLEX_POINT16`; see [The number type](#the-number-type)). The DS has no floating-point hardware, so `float`, which it used before, was all done in software by the C library, on a 67 MHz ARM9. Fixed point is integer arithmetic, with the DS's hardware divider and square root unit for division and square roots (`FixedPointNds.hpp` in flexmath, which locks interrupts around each calculation, since the unit's registers are shared; the drawing thread does the same around `gluLookAt`, which uses them too). The snapshots the drawing thread gets are 4.12 vertex coordinates already, a shift from Q16.16, where they used to go through `float`.

**Steps take a third of the time they did, but it's still short of real time at 60 steps a second.** When it starts, the app times a few settings (each from a fresh ragdoll, falling, over its first five steps) and shows them; in melonDS, ms a step:

| Steps a second | Sub-steps | Resolver sweeps a step (velocity/settle/position; each sub-step's) | `float` | Q16.16 | Real time Q16.16 manages |
| --- | --- | --- | --- | --- | --- |
| 60 | 1 | 40/10/10 (Cyclone's own) | 122 | 29.0 | 0.57x |
| 60 | 2 | 16/4/4; 8/2/2 (what it runs with) | 87 | 19.7 | 0.85x |
| 60 | 4 | 16/4/4; 4/1/1 | 131 | 26.3 | 0.63x |
| 30 | 2 | 16/4/4; 8/2/2 | 85 | 17.8 | 1.9x |

- A step at 60 steps a second, in 2 sub-steps at 8/2/2 sweeps each, takes 2.1 ms to move the bones, 0.9 ms to find contacts and 14.3 ms to resolve them (`World::profile()`). Before the `World` found contacts once a step, it was 2.0, 3.0 and 14.0 ms (19.3 ms a step): finding contacts once saves 2.1 ms, and resolving the contacts its margin adds costs most of it back. Resolving took 19.6 ms until the resolver stopped looking bodies up in hash maps (see [What SAKBox has changed](#what-sakbox-has-changed)): on the ARM9 each hash costs a software division. (In `float`, a sub-step took 5, 6 and 32 ms to move, find and resolve.)
- **ARM instructions, not Thumb, matter most.** devkitPro's template compiles to Thumb, which on the ARM9 has no 32 x 32 -> 64-bit multiply, so every fixed-point multiply was a library call: in Thumb, Q16.16 took 70 ms a step, hardly better than `float`. ARM instructions (`-marm` in the Makefile) brought it to 24.9 ms. (With `float` they gained only 2%: the time was in the software floating point.)
- Fixed point also brought two engine changes that every build benefits from: `RigidBody` keeps its drag factors (`pow` of the damping over the step) from step to step instead of working them out again (moving the bones went from 8.6 to 3.3 ms a sub-step in Thumb), and constants such as `(real)0.5` become fixed point when compiling.
- Once the ragdoll lies on the ground, every bone touches it, and a step costs about twice as much: 52 ms, 0.32x real time, at 60 steps a second.
- The figures on the bottom screen are written only when they change, about once a second: formatting numbers is float work, and the main thread's time comes out of the physics'. Writing them each frame made the physics 25% slower.

Real time at 60 steps a second needs about one and a half to three times as much speed again, mostly in resolving contacts (roadmap). Measured in melonDS, not on a console.

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
- **The same results on every platform:** the maths functions in `precision.h` are correctly rounded (CORE-MATH's, through flexmath's `CorrectlyRounded.hpp`) rather than the system's, and the repo builds with fused multiply-adds off, so a simulation runs the same to the last bit on macOS and Linux, with Clang or GCC, on ARM64 or x86-64. The Waffles jumper's test pins its jump height to check it. See the top-level README's [Floating-point results across platforms](../../README.md#floating-point-results-across-platforms).
- **Any number type, by a build setting** (`//projects/cyclone:real`; see [The number type](#the-number-type)): `double`, `float`, or flexmath's `FlexReal`, `FlexPoint` (Q32.32) or `FlexPoint16` (Q16.16, the DS's). It replaces the separate `:cyclone_flex` and `:app_flex` targets; `flightsim_flex` and `flex_test` are built with the setting fixed (`real.bzl`). Guards against tiny lengths compare with `<=`, so they still catch zero in fixed point, where `1e-9` rounds to zero.
- **The resolver keeps what it works out per body in slots, not maps.** Each body it works on in a call gets a slot (`RigidBody::resolverSlot` points at it) for its velocities at the start of the step, the position sweeps' velocities and its sleep group, and the warm-start impulses are a flat list sorted by pair of bodies, refilled in the memory it already has. They were hash maps and a map of lists, looked up by body and allocated afresh each call: a tenth of the resolver's time on a Mac, more on the DS, where each hash is a software division. Results are bit-for-bit the same. Resolving the ragdoll got 5% to 25% faster on a Mac (more the fewer sweeps), and 29% on the DS.
- **`RigidBody` keeps its drag factors** (`pow` of the damping over the step, and the sleep bias, as the book works them out in sections 10.4 and 16.2.1) from step to step, and works them out again only when the step length or damping changes: steps are usually all the same, and `pow` is slow where it's done in software.
- **Single precision when asked:** `CYCLONE_SINGLE_PRECISION` picks `float` for `real` (upstream had `#elif 0`, so only by editing `precision.h`); the DS app uses it. Without it, `real` is `double`, as before.
- **Sub-steps** (`World::setSubSteps`, and `[` and `]` in the rigid-body demos), and the rigid-body demos run in fixed steps rather than steps as long as each frame; see [Sub-steps](#sub-steps). Resolving contacts together, a world finds its contacts once a step and follows them through its sub-steps (`ContactResolver::beginStep`, `resolveSubStep` and `endStep`; `resolveContacts` is all three with one sub-step). The resolver's sweeps are a step's, shared among its sub-steps (`setSweeps`), so more sub-steps cost about the same.
- **`World` runs the whole simulation** (see [The world](#the-world)): it holds collision shapes and planes and finds the contacts between them itself, with collision filters and joints that keep the bodies they join from colliding (`Joint::collideConnected`), and steps in sub-steps with hooks before each sub-step and when contacts are found. Upstream's `World` kept its bodies and generators in private linked lists with no way to add to them, so it could never simulate anything; every demo found its own contacts and stepped its own bodies. Each demo, the ragdoll, the DS app, the Cyclone padds example and the Waffles jumper now use one. Contacts are found in a different order, so every rigid-body demo moves a little differently (the jumper's hand-made policy jumps 0.51 m, where it jumped 0.47).
- **Hinges, cones and capsules** (see [Hinges and cones](#hinges-and-cones) and [Capsules](#capsules)), for the Waffles jumper, which moved from Bullet to Cyclone. `real_atan2` joins `precision.h`'s functions, for hinge angles.
- **Demo options:** `--run` starts the rigid-body demos unpaused; `--keys` plays scripted key presses and clicks.
- **The demo window is `runInWindow`** (`src/demos/window.h`), which `main.cpp` calls, so a program with its own `main` (the Waffles jumper, which has commands of its own) can open a demo. `RigidBodyApplication`'s `cameraDistance` and `cameraTargetHeight` bring the camera closer for a smaller scene (18 m and 5 m, as before, by default).
- **GLFW instead of GLUT:** the demos open a GLFW window (GLUT is deprecated on macOS and tied the demos to it). GLUT's sphere, cube and bitmap text are replaced by `src/demos/shapes.cpp` (the same geometry: GLU spheres, GLUT's cube faces and normals) and `stb_easy_font`. They draw at full Retina resolution, with text and line widths scaled to keep their size on screen.
- **Cleanups:** `Quaternion`'s components are plain members rather than an anonymous struct in a union (a GNU extension), `sprintf` became `snprintf`, constructor initialisers follow declaration order, and unused code is gone.

Changes inside the code are marked with `SAKBox:` comments.

## Tests

| Test | Covers |
| --- | --- |
| `core_test` | `Vector3`, `Quaternion`, `Matrix3` and `Matrix4` maths, and a box's inertia tensor; rigid-body and particle integration against their closed forms; damping; local and world space; sphere-sphere and sphere-plane contacts. Written against the original engine before it was reformatted, so it also guards the cleanups. |
| `ragdoll_test` | The ragdoll demo's physics without a window, with both resolvers: it collapses in place when not pushed, and a push knocks it over nearby at both 1/60 s and 0.05 s steps; resolving contacts together, 20 pushed ragdolls come to rest better than with the book's resolver, with their joints stretched under 1.5 cm beyond their slack on average; four sub-steps (the demos' default), sharing a step's sweeps, cut the worst any joint stretches over 20 pushes over ten times, and two over 2.5 times, at 60 and 20 steps a second, and it still comes to rest |
| `collision_test` | Box against plane, box and sphere (including a sphere whose centre is inside the box, plain and turned, going out the nearest face, and resolving without NaN; contact count, depth, normal, point); a full contact array; the box contact cache (all four corners of a resting box on the first frame, and still while it rocks; the area where the faces touch for a box turned 10 and 45 degrees, hanging off, and smaller, with its middle over the contacts; dropping contacts that part or slide off their face, edge contacts, forget and clear); the yes/no intersection tests agree with contact generation; bounding spheres and the bounding-volume tree's candidate pairs; capsules: lying on the ground (both ends) and standing (the lower end), against a sphere's side and end, crossed (one contact where they cross) and side by side (one at each end of the overlap), on a box's face and across its edge, and their inertia tensor against a sphere's and a rod's |
| `forces_test` | Each force generator against its formula: gravity (which leaves a sleeping body asleep), springs (stretched and squashed), bungees, drag, buoyancy, the fake spring, and aerodynamic surfaces (drag, lift turning with the body, wind, torque from an off-centre surface, control surfaces blending their tensors), and explosions (each stage's reach, strength and timing, the shock wave's fall-off and dependence on an object's speed, spinning rigid bodies, a ring of objects thrown outwards); the force registries' add, remove and clear; plus the sailboat demo's boat floating |
| `contacts_test` | The contact resolver (bounce at restitution x speed, overlap pushed out, head-on elastic collision); the particle resolver counting a velocity-only resolve as no move; joints; particle cables and rods, between particles and to a fixed point; the particle world (a dropped particle, registered forces, a pendulum's length and period); the rigid-body world (a dropped box coming to rest, stacks of two and three boxes standing, sharing out the contact array, forces added by hand acting through every sub-step); sleep (a lone box, stacks falling asleep together, a box dropped on a sleeping stack waking it, knocking out a stack's bottom box, a tall stack set up asleep staying up, a body that can't sleep keeping its group awake, a body that leaves its group sleeping on its own); resolving contacts together (awake stacks of 3, 5, 8 and 10 boxes standing still for 30 s with no creep, while the book's resolver drops 8; a dropped box coming to rest; stacks sleeping and waking; knocking out a stack's bottom box; a sliding box slowing at friction x g; a bounce at restitution x landing speed; sub-steps sharing a step's sweeps; ten boxes standing still in 4, 8 and 16 sub-steps); joints as constraints (a ball-and-socket pendulum holding to its pivot, losing under 7% of its speed at 60 steps a second and under half that at 240; four sub-steps a step swinging it exactly as 240 whole steps a second; a chain of five staying joined; a cable free until taut, then holding; the book's resolver taking joints as their contacts). Its box world is a `World` of box shapes on the ground plane, which updates each shape after integrating its body, so contacts are found where the boxes are now (finding them a step behind put a sliding box's contacts 8 cm back, and it tipped over); hinges (a pendulum set spinning off its axis still swinging only about it, the angle's sign, a limit stopping a swing and holding it there, a motor reaching its speed, or turning as fast as its torque can) and cones (stopping a tilt at its angle) |
| `world_test` | `World` finding contacts: between every kind of shape and against the ground, filters keeping shapes apart (the ground's too), a body's own shapes and jointed bodies not colliding unless the joint says so; shapes coming to rest on the ground and each other in 1 and 4 sub-steps; contacts found once a step when resolving together and in each sub-step with the book's resolver, with `beforeSubStep` called for each; a box falling at 20 m/s landing in sub-steps without sinking into the ground; removing a body removing its shapes |
| `stress_test` | Random scenes, 200 of each: boxes and spheres (50 g to 500 kg, up to 100 m/s and 20 rad/s, some joined by joints) with both resolvers, and particles joined by rods, cables, anchors, springs and bungees, each at 60 and at 20 steps a second for 10 s (the rigid bodies also at 20 in four sub-steps), every body checked after every step: finite position, velocity, spin and orientation, a unit orientation, under 1,000 m/s and within 10 km. A failure names the seed, step and body. It finds the bugs scenes built by hand miss: with the old `boxAndSphere` it fails on its first scene. (About 60 s.) |
| `random_test` | The random number generator: repeatable from a seed; reals, integers, vectors and orientations spread evenly over their ranges; asking for an integer below 0 is an error |
| `impact_sounds_test` | The demos' sounds and knocks, heard through an offline output: a hit knocks and resting doesn't (particles' contacts too, a cable snapping taut among them), a pair knocks at most once in its repeat time, only the hardest six hits a frame knock (also over a frame of several steps, where a pair hit in more than one knocks once), turned off nothing plays, and a hit sounds on the side of the screen it happened (the camera turned either way); a loop playing until stopped, silent while sound is off and back when it's on; nothing playing while off |
| `fixed_steps_test` | The demos' physics clock: one step a frame at 60 Hz despite 16 and 17 ms frames; 60 steps a second at 144, 120, 30 and 20 Hz; a 2 s stall counting as 0.1 s; changing the step length as it runs (240 steps a second, time not yet stepped carried over); sub-steps splitting the step, starting at 4 and between 1 and 16 |
| `ragdoll_test_flex_point16`, `ragdoll_test_flex_point` | `ragdoll_test` in Q16.16 (as the DS runs it) and Q32.32 fixed point, with the same expectations |
| `flex_test` | The flex build: `FlexReal` as `real`, precision snapping in the engine, and that time steps and drag stay at full precision (stochastic rounding keeps drag unbiased) |

## Known limitations

- **The demos draw with legacy OpenGL,** which macOS has deprecated (its warnings are silenced with `GL_SILENCE_DEPRECATION`) but still provides. They are tested on macOS; on Linux they link GL and GLU and on Windows opengl32 and glu32, but neither has been tried.
- **With the book's resolver, awake stacks creep, and tall ones fall before they can sleep.**
  - *Creep:* while its boxes are awake, a stack slides sideways a little: three boxes about 3 cm in 30 s, five faster, falling after 15-25 s; eight fall after about 7 s.
  - *Sleep stops it:* a stack of up to five falls asleep within about 2.5 s and then stays put until something knocks it. Eight or more rock and fall within a few seconds of being built, before they settle.
  - *Fix:* resolve the contacts together (`Method::sequentialImpulses`, the default; see [Resolving contacts together](#resolving-contacts-together)). Awake stacks of ten then stand still for as long as they're left (moving under 2 cm in all); fifteen stand, moving under 10 cm.
  - *Or:* set tall stacks up asleep (`setAwake(false)`), as the book suggests for a level. They then stand until hit.
- **A body fast enough to cross the margin in a step can sink into another body.** A `World` resolving contacts together in sub-steps finds them once a step, 2 cm apart (`Settings::margin`), so a body moving faster than that a step towards another (1.2 m/s at 60 steps a second) can sink up to the rest of its step's travel into it before the next step finds their contact; finding contacts in every sub-step limited it to a sub-step's travel. The contact then pushes them apart over a few steps. Against a plane the margin covers the step's travel, so nothing falls into the ground. A wider margin for fast pairs was tried: contacts found that far apart, followed as bodies turned, pushed a light box spinning between heavy ones until it flew off (`stress_test` seed 118). For fast bodies, more steps or the book's resolver (which finds contacts in each sub-step).
- **Joints, resolved together, stretch for a moment when a chain hits something hard,** and a swinging joint loses a little energy each step.
  - *Stretch:* when the ragdoll's first bone hits the ground, the rest of it is still falling at about 8 m/s, and stopping the whole chain in one step leaves some joints up to 7 cm beyond their slack for a step or two at 60 steps a second (64 cm at 20). More sweeps don't help; the book's resolver stretches them as far. Box2D's documentation says the same of its joint chains.
  - *Energy:* each step takes out the speed that would carry a body off the circle its joint holds it to, which also takes a little of its swing. A pendulum reaches the bottom of its first swing about 6% slower than a real one at 60 steps a second, and less than half that short at 240. This happens in any velocity-level solver.
  - *Fix:* sub-steps (see [Sub-steps](#sub-steps)). Four a step, sharing the step's sweeps, cut the ragdoll's worst stretch to 0.004 cm at 60 steps a second (0.3 cm at 20), for about the physics work of one.
  - *Cause:* the book explains it (sections 15.5 and 17.3.1). The resolver handles one contact at a time, so the friction at one contact isn't respected while it resolves another. The book's remedies are sleep, which SAKBox now does for stacks, and resolving all the contacts at once (chapter 18).
- **Fixed point has less range and precision than `double`.** The ragdoll behaves as well in Q16.16 and Q32.32 as in `double` (`ragdoll_test_flex_point16` and `..._flex_point`), but other tests fail in fixed point:
  - *Range (Q16.16):* values run to ±32768, so the stress test's extreme scenes (500 kg at 100 m/s is a momentum of 50,000) overflow, and a body's state turns to garbage.
  - *Precision (Q16.16):* the step is 1.5e-5, so stacks of boxes drift and topple, a pendulum loses its swing faster, and orientations stay unit length only to about 1e-3 (the stress test allows less).
  - *Q32.32* passes the stress test's scenes when contacts are resolved together, but the book's resolver blows up in one (seed 64).
  - *Exact results:* tests that check results to 1e-9 (`core_test`, `forces_test`) can't pass in fixed point.
  Keep a fixed-point scene small: masses of a few kilograms, speeds of a few metres a second, a few metres across.
- **Hinges and cones need contacts resolved together** (`Method::sequentialImpulses`, the default): the book's resolver holds only their anchors, so they turn freely, as balls and sockets. A cone limits how far its axis tilts, not how far the bodies twist about it.
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

1. **Blended drawing between fixed steps in the demos:** draw each body between its last two steps' positions, by how far the frame is into the next step, so motion is smooth on screens whose rate isn't a multiple of 60 (it costs a step of delay). The [Cyclone padds example](../examples/cyclone_padds/README.md) does this for the ragdoll (`SnapshotTimeline`).
2. **Sub-steps for the particle world** (`ParticleWorld`), and the mass-aggregate demos in fixed steps.
3. **Real time on the DS.** In Q16.16 a step at 60/2 and 16/4/4 (8/2/2 each sub-step) is 19.7 ms, 14.3 of them resolving contacts, and a ragdoll on the ground costs twice as much. Sharing the sweeps among sub-steps (done) keeps more sub-steps from costing more sweeps, but a sub-step's own cost is most of it on the DS; so cheaper rows (each contact's three rows set up and swept in fixed point), or 30 steps a second, which runs at 1.9x real time. Profile on the DS first (see [On the Nintendo DS](#on-the-nintendo-ds)).
