# Seeded random numbers

One random number generator for SAKBox's games, Cyclone and (as an option) Waffles. It gives the same numbers on every platform, the Mac, Linux, Windows, the DS and the 3DS, so a seed always makes the same course, board or run, and a recorded game replays exactly. Header-only, with no dependencies.

```cpp
#include <SeededRandom/Seed.hpp>

SeededRandom::Seed seed = *SeededRandom::Seed::fromCode("K7QM-3XPA"); // or Seed::mixedFrom(time)
SeededRandom::Generator course = seed.stream("course");
SeededRandom::Generator bots = seed.stream("bots");

int face = course.between(1, 6);        // 1 to 6, both included
bool six = course.chance(1, 6);
float f = course.unitFloat();            // 0 up to 1
bots.shuffle(deck.begin(), deck.end());
```

## The generator

`Generator` (`Generator.hpp`) is **xoshiro128\*\*** 1.1, by David Blackman and Sebastiano Vigna (public domain; [prng.di.unimi.it](https://prng.di.unimi.it)). It's small (16 bytes of state), fast, and it works in 32-bit steps, so it's quick on the DS's ARM9 too. A seed is spread over the state with splitmix64, as its authors recommend, so seeds 0, 1, 2, ... give unrelated sequences.

| | |
| --- | --- |
| `next()` | 32 random bits |
| `below(limit)` | 0 up to, not including, `limit`, every value equally likely (Lemire's method) |
| `between(low, high)` | `low` to `high`, both included |
| `chance(times, outOf)` | true `times` in `outOf`, on average |
| `unitFloat()`, `unitDouble()` | 0 up to 1, with 24 and 53 random bits |
| `shuffle(first, last)` | a random order, every order equally likely (Fisher-Yates) |
| `jump()` | 2^64 numbers ahead, for sequences that never overlap |
| `state()`, `Generator::fromState()` | save and restore it exactly |

Everything is integer arithmetic or exact scaling by a power of two, so the results don't depend on the compiler, the standard library or the FPU. That's why it has its own `below()` and `shuffle()`: the standard library's distributions and `std::shuffle` are left to each library, and differ between them. It is a C++ uniform random bit generator, so it can be passed to anything that takes one, but then the results are only as portable as that code.

It works at compile time too (`constexpr`), so a constant table can be made from a seed.

## Seeds and named streams

`Seed` (`Seed.hpp`) is the number that picks one course or board. It's shown as a code to type and share: **8 characters of Crockford's base 32** (digits and capitals without I, L, O and U), as two groups of four, `K7QM-3XPA`. So a seed has 40 bits, about a trillion courses. Reading a code back is forgiving: lowercase is fine, the dash and spaces are optional, O reads as 0, and I and L as 1. `Seed::fromCode` returns nothing for anything else.

Each part of a game draws from its own **named stream** (`seed.stream("course")`, `seed.stream("bots")`), so adding a feature that draws numbers changes only its own stream, not every other sequence. Different names give unrelated sequences, not even sharing their first number.

A new seed for a new game: `Seed::mixedFrom(anything that varies)`, such as the time or the frame the player pressed start on. Nearby values give unrelated seeds.

The numbers a seed and stream name give are part of the format: shared codes and saved games must keep making the same courses. `Seed_test` pins them.

## Who uses it

- **Cyclone:** `cyclone::Random` is built on it (see Cyclone's README). A game can hand Cyclone one of its streams: `cyclone::Random debris(seed.stream("debris"))`.
- **Waffles:** `WRandXoshiro128`, a `WRand` that draws from it. `WRand` stays Waffles' default, so its recorded results keep their values.
- **The games** (Infinifly's courses, Cosmic Hex's boards, bots and recorded games) will use it directly.

## Building

Bazel: depend on `//projects/seeded_random`, and include `<SeededRandom/Generator.hpp>` or `<SeededRandom/Seed.hpp>`. It needs C++20 (`<bit>`, `constexpr` and defaulted comparisons).

A DS or 3DS app built with devkitPro's Makefiles: add `"//projects/seeded_random:sources": "seeded_random"` to the app's `extra_srcs`, and `seeded_random/include` to its Makefile's `INCLUDES` (Cyclone's apps do; see `projects/cyclone/ds`).

## Tests

| Test | What it checks |
| --- | --- |
| `Generator_test` | The outputs and the jump match the authors' reference code; splitmix64's seeding; repeatable from a seed and from a saved state; works at compile time; `below`, `between`, `chance`, the unit numbers and `shuffle` spread evenly over their ranges |
| `Seed_test` | Codes read back as their seeds; typing is forgiving; what isn't a code is refused; 40 bits; `mixedFrom` spreads nearby numbers; streams are repeatable, unrelated to each other, and pinned |

```sh
bazel test //projects/seeded_random/...
```
