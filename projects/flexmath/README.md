# flexmath

**Numeric precision you can change while the program runs.**

flexmath lets a game or simulation lower or raise the precision of whole subsystems on the fly: physics that turns slippery, collisions that skip past thin walls, geometry that wobbles and snaps. Precision becomes something a powerup or level event can change, not a compile-time choice.

It was built for a Star Fox-style flying game in which collected powerups change precision as part of play. You can see it at work in [`//projects/examples/precision_flight`](../examples/precision_flight) and in Cyclone's flex build ([`//projects/cyclone:cyclone_flex`](../../third_party/cyclone)).

## Contents

- [The core idea](#the-core-idea)
- [Quick start](#quick-start)
- [Concepts](#concepts)
  - [Precision and `quantize()`](#precision-and-quantize)
  - [Rounding and bias](#rounding-and-bias)
  - [Groups and policies](#groups-and-policies)
  - [Overrides](#overrides)
  - [The controller](#the-controller)
- [Value types](#value-types)
- [Determinism](#determinism)
- [Using flexmath with Cyclone](#using-flexmath-with-cyclone)
- [Building and testing](#building-and-testing)
- [Portability](#portability)
- [Design notes](#design-notes)
- [Roadmap](#roadmap)

## The core idea

**Values are always stored at full precision. Precision is applied when a value is read or written back.**

```
stored value  ──view()──▶  4.3  seen at 1 fractional bit  ──▶  4.0
(full precision)           4.3  seen at 5 fractional bits ──▶  4.28125
```

Because nothing is ever stored at low precision unless you ask for it, changing a precision setting costs nothing. There's no list of values to visit; every value simply resolves differently on its next read. Turning precision back up restores exact values, unless you chose to throw the detail away (see [policies](#groups-and-policies)).

## Quick start

```cpp
#include <FlexController.hpp>
using namespace Flex::Math;

FlexController precision;

// One group per subsystem whose precision should be adjustable
FlexGroup &flight = precision.addGroup("Flight", Precision{.fracBits = 1});  // 0.5 steps
FlexGroup &render = precision.addGroup("Render", Precision{.fracBits = 9});

// Store state at full precision...
FlexPoint x = 4.3;

// ...and read it through the group that governs it
flight.view(x);                                  // 4.0

// A powerup raises flight precision while its handle is alive
FlexOverride focus = flight.push(Precision{.fracBits = 5});
flight.view(x);                                  // 4.28125
focus.release();                                 // back to 0.5 steps
```

Depend on it from Bazel with `"//projects/flexmath"`, and include `<FlexController.hpp>`. That header brings in everything else.

## Concepts

### Precision and `quantize()`

A `Precision` (in `FlexPrecision.hpp`) describes how finely and how far a value can resolve:

| Field | Meaning | Default |
|---|---|---|
| `fracBits` | Values snap to multiples of `2^-fracBits`: 4 means steps of 1/16, 0 whole numbers, −1 steps of 2. 52 or more leaves a `double` untouched. | 52 (full) |
| `intBits` | With `overflow` set, values are limited to `[-2^intBits, 2^intBits)`. So `intBits = 4` behaves like a 5-bit signed register. | 0 |
| `rounding` | `TowardZero` (drop the excess), `Nearest` (exact halves round away from zero), or `Stochastic` (up or down at random, weighted by distance; see [Rounding and bias](#rounding-and-bias)). | `TowardZero` |
| `overflow` | `None` (unlimited range), `Saturate` (clamp), or `Wrap` (wrap around like a register). | `None` |

`quantize(value, precision)` applies one to a `double`; `FlexPoint::quantized(precision)` does the same with integer arithmetic:

```cpp
quantize(4.3,  Precision{.fracBits = 1});                                     // 4.0
quantize(-1.75, Precision{.fracBits = 1});                                    // -1.5 (toward zero)
quantize(1.8,  Precision{.fracBits = 1, .rounding = Rounding::Nearest});      // 2.0
quantize(3.9,  Precision{.fracBits = -1});                                    // 2.0 (steps of 2)
quantize(16.0, Precision{.intBits = 4, .overflow = Overflow::Wrap});          // -16.0
quantize(100.0, Precision{.fracBits = 4, .intBits = 4,
                          .overflow = Overflow::Saturate});                   // 15.9375
```

Two properties hold for `TowardZero` and `Nearest`: snapping twice gives the same result as snapping once, and snapping never reorders values. That's what makes low-precision behaviour predictable enough to design gameplay around. `Stochastic` keeps the first property but not the second.

### Rounding and bias

`TowardZero` and `Nearest` are *biased*: each one errs the same way every time for a given value. That's fine for a single read, but in a loop that feeds its result back in, such as integrating velocity or applying drag every frame, the errors add up instead of cancelling:

- **Truncation** shrinks every result a little, so a simulation steadily loses energy. At 6 bits, a plane that should reach 16 m/s tops out around 13.
- **Nearest** drops any change smaller than half a step. Small accelerations, and drag on a slow object, vanish entirely.

`Rounding::Stochastic` rounds up with probability equal to how far the value is past the lower grid point: 1.25 at 0 bits becomes 2 a quarter of the time and 1 otherwise. Each result is still on the grid, but the *average* is exact, so even changes far smaller than a step add up correctly. Low precision then shows up as noise rather than drift:

```cpp
Precision sixteenths{.fracBits = 4, .rounding = Rounding::Stochastic};
double total = 0;
for (int i = 0; i < 10000; ++i)
  total = quantize(total + 0.001, sixteenths); // ~10; TowardZero and Nearest give 0
```

The random numbers come from a small generator per thread with a fixed starting seed, so a run is reproducible as long as it performs the same operations in the same order. Call `seedStochasticRounding(seed)` to vary it. Stochastic rounding is the standard fix for this problem in low-precision arithmetic (it's widely used for training neural networks in 8- and 16-bit formats).

`Overflow::Wrap` is there to create range glitches *on purpose*, such as an enemy flung across the map when its position wraps. Accidental overflow inside other math is much harder to control.

### Groups and policies

A `FlexGroup` (in `FlexGroup.hpp`) is a precision setting shared by everything that reads through it, usually one subsystem: flight physics, collision checks, rendering. It has a **base** precision, optional **overrides** on top, and a **policy** that decides whether detail lost at low precision stays lost:

| Policy | `view(v)` | `commit(v)` | Reversible? | Good for |
|---|---|---|---|---|
| `View` | snaps | returns `v` unchanged | Yes: raise precision and every value is exact again | Rendering. Checks that should *see* coarse values while the simulation stays exact, such as collision tests that tunnel through thin walls. |
| `Lossy` | snaps | snaps | Going forward only: detail lost while precision is low stays lost | State that should genuinely degrade: drift, slippery controls, damping that stops working. |
| `PerOperation` | snaps | snaps | No | Running whole algorithms on `FlexReal` under `activate()`, so *every* intermediate result snaps. The most dramatic option. |

The pattern is the same whichever policy a group has:

```cpp
// Reading: use view()
FlexPoint seenX = flight.view(x);          // what collisions, AI, cameras... see

// Updating: compute at full precision, then commit()
x = flight.commit(x + velocity * dt);      // snaps only if the policy is Lossy or PerOperation
```

Gameplay code doesn't need to know the policy. Switching a group from `View` to `Lossy` changes the behaviour without touching the call sites.

### Overrides

`group.push(precision)` applies a temporary precision, typically from a powerup, and returns a `FlexOverride` handle. The override lasts while the handle is alive:

```cpp
FlexOverride focus = flight.push(Precision{.fracBits = 5});  // applies now
focus.release();                                              // or let the handle go out of scope
```

- Overrides can overlap and be released **in any order**. The group uses its most recently pushed override that's still alive, otherwise its base.
- Handles are move-only. Assigning a new handle releases the old one, so "replace the current override" is one statement: `focus = flight.push(...);`.
- The group must outlive its handles.

### The controller

`FlexController` (in `FlexController.hpp`) owns a game's groups and finds them by name, so powerups, level scripts and debug tools can reach any subsystem:

```cpp
FlexController precision;
precision.addGroup("Hull", Precision{.fracBits = 2}, Policy::View);

// Anywhere with access to the controller:
FlexOverride phase = precision.group("Hull").push(Precision{.fracBits = -1});

// e.g. a debug panel
precision.forEachGroup([](FlexGroup &g) { /* show g.name(), g.precision() */ });
```

It's an ordinary object rather than a global singleton: the game owns one, and tests or separate simulations each create their own. References returned by `addGroup()` and `group()` stay valid for the controller's lifetime. `addGroup()` throws `std::invalid_argument` if the name is taken; `group()` throws `std::out_of_range` if it doesn't exist.

## Value types

| Type | Header | What it is | Use it for |
|---|---|---|---|
| `FlexPoint` | `FlexPoint.hpp` | Signed **Q32.32 fixed point**: a 64-bit integer counting units of 2^-32. | Game state, especially state that must be [deterministic](#determinism). |
| `FlexReal` | `FlexReal.hpp` | A `double` that snaps **every operation** to the thread's active precision. | The `PerOperation` policy; replacing an engine's `real` typedef. |
| `FlexVector` | `FlexVector.hpp` | A variable-length vector of `FlexPoint`s: element-wise `+ - *`, scaling, `dot`, `quantized`. | Small deterministic vector math. |
| `FlexMatrix` | `FlexMatrix.hpp` | A row-major matrix of `FlexPoint`s: `+ -`, matrix product, scaling, `quantized`. | Small deterministic matrix math. |
| `double` | | Plain doubles work with `quantize()` and with groups' `view()` / `commit()`. | Anything where determinism across machines doesn't matter. |

flexmath deliberately doesn't wrap other math libraries. glm vectors, Cyclone's `Vector3` and plain `double`s all work: read their components through a group.

### FlexPoint

```cpp
FlexPoint a = 1.5, b = 2;              // from floating point (rounded) or integers (exact)
FlexPoint c = a * b + 0.25;            // 3.25, exactly
c.quantized(Precision{.fracBits = 1}); // 3.0
fmt::format("{} {:x}", c, c);          // "3.250000 0x340000000"
FlexPoint::fromFixed(c.valueFixed());  // round-trips the raw bits, e.g. for saves or networking
```

Things to know:
- **Range is about ±2^31.** Exceeding it, in construction or arithmetic, overflows the underlying `int64_t`.
- **`+` and `-` are exact.** `*` rounds toward negative infinity in the last place (2^-32), and `/` truncates toward zero. **Dividing by zero is undefined behaviour**, so check divisors first.
- `value()` converts to `double` exactly while |value| < 2^21.
- Comparisons are exact. To compare at a reduced precision, compare quantized values: `a.quantized(p) == b.quantized(p)`.

### FlexReal

```cpp
{
  ScopedActivePrecision coarse(Precision{.fracBits = 2});  // or: auto a = group.activate();
  FlexReal x = 0.9;            // 0.75
  FlexReal y = x * x;          // 0.5625 snaps to 0.5
}
FlexReal z = 0.9;              // 0.9: full precision again outside the scope
```

Things to know:
- The active precision is **per thread** and starts at full precision. Scopes nest and restore the previous precision on exit.
- Mixed arithmetic with `int`, `float` or `double` on either side produces a snapped `FlexReal`.
- It converts implicitly to `double`, but **not through C varargs**: `printf("%f", x)` needs `(double)x` or `x.value()`.
- It emulates precision loss, not overflow: values keep `double`'s range.
- `flex_sin`, `flex_cos`, `flex_exp`, `flex_pow` and `flex_atan2` are correctly rounded (see [Determinism](#determinism)), so they give the same result on every machine; `flex_sqrt`, `flex_abs` and `flex_fmod` are exact anyway.

## Determinism

`FlexPoint`, `FlexVector`, `FlexMatrix` and `FlexPoint::quantized()` use only integer arithmetic, so they give bit-identical results on every machine and compiler. Use them for state that has to stay in sync: replays, lockstep multiplayer, verified leaderboards.

`quantize()` on `double`s and `FlexReal` give the same results on every platform too, built the repo's way. Two things made floating point differ between machines (the top-level README's [Floating-point results across platforms](../../README.md#floating-point-results-across-platforms) has the details and measurements), and both are dealt with:

- **Fused multiply-adds:** Clang fuses `a * b + c` into one operation, rounded once, and GCC doesn't. The repo turns fusion off everywhere (`-ffp-contract=off` in `.bazelrc`).
- **The maths library:** the system's `sin`, `pow` and the like (Apple's libm, glibc) aren't correctly rounded, and each gets the last bit wrong on different inputs. `CorrectlyRounded.hpp` (`//projects/flexmath:correctly_rounded`) gives CORE-MATH's instead ([core-math.gitlabpages.inria.fr](https://core-math.gitlabpages.inria.fr/), MIT, fetched by `MODULE.bazel`): `Flex::Math::CorrectlyRounded::sin`, `cos`, `exp`, `log`, `pow`, `atan2`, `asin` and `acos`, each returning the double nearest the exact result, the one right answer, so every platform agrees. `FlexReal`'s functions and Cyclone's `real_sin` and friends use them. `sqrt`, `fabs` and `fmod` are exact under IEEE 754 already. `CorrectlyRounded_test` checks six of CORE-MATH's hard cases against 200-bit results (from mpmath); Apple's libm is one bit off on each.

Code that calls `std::sin` and the like directly still gets the system's: use `CorrectlyRounded.hpp` where results must match across machines. The cost is speed: CORE-MATH's functions can be slower than the system's, and unfused arithmetic slightly so.

Quantizing state does *not* make floating point deterministic by itself. It makes cross-machine differences rarer, since most tiny discrepancies snap to the same grid point. A value sitting right on a grid boundary can still snap differently.

## Using flexmath with Cyclone

SAKBox's copy of the Cyclone physics engine ([`projects/cyclone`](../cyclone/README.md)) has a flex build in which its `real` type is `Flex::Math::FlexReal`, so the whole engine runs at an adjustable precision:

```sh
bazel run //projects/cyclone:flightsim_flex    # [ and ] change precision, N rounding, F full
bazel test //projects/cyclone:flex_test
```

The plain `//projects/cyclone:cyclone` build doesn't use `FlexReal`; it uses only `:correctly_rounded`, for its maths functions.

Precision applies to the bodies' *state*: positions, velocities, orientations, and every result computed from them. The time step and the per-frame drag factors derived from it (`pow(damping, dt)`) keep full precision. Snapping those would bias every frame the same way: at 6 bits a drag factor of 0.99644 truncates to 0.984375, four times the drag, and the flight sim's plane slowed to a crawl. The `real_duration` type and `CYCLONE_FULL_PRECISION_SCOPE` in `precision.h` implement this.

`flightsim_flex` starts in stochastic rounding, so lowering precision makes the flight noisier without slowing it down. Press N to compare truncation and nearest rounding, which bias the flight. Precision goes down to 2 bits.

## Building and testing

```sh
bazel build //projects/flexmath
bazel test  //projects/flexmath/...
```

Each test file is named after what it tests, as `<Source>_test.cpp` (see the top-level README's [Code conventions](../../README.md#code-conventions)).

| Test | Covers |
|---|---|
| `FlexPoint_test` | Fixed-point arithmetic, rounding, overflow modes, and agreement between the integer and `double` snapping paths over thousands of random cases |
| `FlexReal_test` | Per-operation snapping, nested scopes, `quantize()` properties (snapping twice = once, order preserved) |
| `Rounding_test` | Stochastic rounding: on-grid values unchanged, results are neighbouring grid values, unbiased on average, small increments accumulate, seeding, overflow |
| `FlexGroup_test` | Overrides (stacking, release order, moves), policies, `activate()`, the controller, and the two Precision Flight mechanics as library properties |
| `FlexVector_test`, `FlexMatrix_test` | Element-wise operations, products, quantization, dimension checks |
| `CorrectlyRounded_test` | CORE-MATH's functions on six hard cases, against results worked out with 200-bit arithmetic, and some easy ones |
| `Examples_test` | Every code example in this README |

## Portability

`FlexPoint` needs integers wider than its 64 bits in three places, and uses `__int128` for them, through the alias `FlexPoint::FlexPointWide`:

| Where | Why 64 bits aren't enough |
|---|---|
| `operator*` (`FlexPoint.hpp`) | The product of two 32.32 values is 128 bits wide before it is shifted back down by 32 |
| `operator/` (`FlexPoint.cpp`) | The dividend is shifted up by 32 first, to keep the fraction |
| `quantized()` with `Overflow::Wrap` (`FlexPoint.cpp`) | The span the value wraps within, 2 x limit, can reach 2^63, one more than an `int64_t` holds |

CORE-MATH's functions (`:correctly_rounded`) use `unsigned __int128` too, in C, and need GCC or Clang the same way.

`__int128` isn't standard C++ but an extension that GCC and Clang provide on 64-bit targets. The alias is declared with `__extension__` so that the build's `-pedantic-errors` allows it (GCC rejects it otherwise; Clang doesn't mind). GCC and Clang on macOS and Linux are covered.

**Windows, when it's added:** MSVC has no 128-bit integer type, so these three places (and CORE-MATH) need another route there:

- **Simplest: build with clang-cl** (Clang with MSVC's command line), which provides `__int128` on 64-bit Windows, so nothing changes.
- **With MSVC itself:** replace each use with a helper function, kept in one header, that uses `__int128` where the compiler has it and MSVC's intrinsics where it doesn't:
  - Multiply: `_mul128` gives the full signed 128-bit product as two 64-bit halves (x64 only; on ARM64, `__mulh` gives the high half), then shift the pair down by 32.
  - Divide: `_div128` divides a 128-bit value by a 64-bit one (Visual Studio 2019 and later, x64 only). Unlike the GCC version, which keeps the low 64 bits of an oversized quotient, it raises a hardware exception when the quotient doesn't fit in 64 bits, so dividing by a very small value would need a check first.
  - Wrap: no 128-bit type is needed. The span fits in an unsigned 64-bit integer (`uint64_t`), so the wrap can be done in unsigned arithmetic.
- Test the helpers against the `__int128` results: `FlexPoint_test` already compares the integer path with `double` over thousands of random cases.


flexmath's original design (February 2023) had each `FlexPoint` register itself with its `FlexGroup` through a singleton `FlexController`, so a group could find and update its values when precision changed. The concepts — groups of values sharing a precision knob, and a central controller — are unchanged. The mechanism changed: values now *read* their group's precision when used, instead of groups *pushing* changes to registered values. That removes per-value registration, which was costly for the temporaries that arithmetic creates. It also makes a precision change instant, and lets each group choose whether detail lost at low precision is kept (`View`) or discarded (`Lossy`).

## Roadmap

1. **LLVM libc's maths functions instead of CORE-MATH's, once they're header-only.** LLVM's C library is moving its correctly rounded functions into headers (`libc/shared/math/`, starting with `sin` in early 2026), which would make them one include with no C library to build, maintained by the LLVM project. Being correctly rounded, they give exactly the same results as CORE-MATH's, so switching changes nothing (the pinned jump height and `CorrectlyRounded_test` would show it). Worth doing once `sin`, `cos`, `exp`, `log`, `pow`, `atan2`, `asin` and `acos` are all there.
2. **Cyclone integration hooks.** Snap Cyclone's state at chosen points instead of every operation: `commit()` after integration and contact resolution, `view()` for collision reads. Each point would be tied to its own group, so integration, collision and contacts can have separate precision.
3. **CORDIC trig for `FlexPoint`.** Integer-only sine and cosine whose iteration count doubles as an *angle precision* setting, for visibly stepped rotation.
