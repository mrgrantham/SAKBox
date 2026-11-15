// Generator.hpp - a seeded random number generator that gives the same
// numbers on every platform (the Mac, Linux, Windows, the DS and the 3DS),
// so a seed always makes the same course, board or run, and a recorded game
// replays exactly.
//
// The algorithm is xoshiro128** 1.1 (David Blackman and Sebastiano Vigna,
// public domain; https://prng.di.unimi.it): small (16 bytes of state), fast,
// and working in 32-bit steps, so it's quick on the DS's ARM9 too. Seeds are
// spread over the state with splitmix64, as its authors recommend.
//
// Everything here is integer arithmetic or exact scaling by a power of two,
// so the results don't depend on the compiler, the standard library or the
// FPU. (That's why it has its own below() and shuffle(): the standard
// library's distributions and std::shuffle differ between libraries.)
//
//   SeededRandom::Generator random(1234);
//   unsigned face = random.between(1, 6);
//   random.shuffle(deck.begin(), deck.end());
//
// For a game, start from a Seed (Seed.hpp), which gives each part of the game
// its own named stream.
#pragma once

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <utility>

namespace SeededRandom {

// splitmix64 (Vigna, public domain): one step of a 64-bit generator whose
// outputs are well spread even for seeds like 0, 1, 2, ...; used to fill
// the state from a seed, and to mix names into seeds.
constexpr std::uint64_t splitMix64(std::uint64_t &state) {
  std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

class Generator {
public:
  // Its whole state: four 32-bit words, never all zero. A save that stores
  // these and restores them with fromState() carries on exactly.
  using State = std::array<std::uint32_t, 4>;

  // It's a C++ uniform random bit generator, so it can go to anything that
  // takes one; but for results that are the same everywhere, use its own
  // methods rather than the standard library's distributions.
  using result_type = std::uint32_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return 0xffffffffu; }

  // The same seed always gives the same numbers.
  constexpr explicit Generator(std::uint64_t seed) : Generator(seed, 0) {}

  // A seed and a stream key (Seed::stream() makes the key from a name):
  // each key gives a different, unrelated sequence from the same seed.
  constexpr Generator(std::uint64_t seed, std::uint64_t streamKey) : state_{} {
    std::uint64_t mixer = seed;
    const std::uint64_t first = splitMix64(mixer) ^ streamKey;
    mixer = first;
    const std::uint64_t second = splitMix64(mixer);
    state_ = {static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32),
              static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32)};
    if (state_ == State{})
      state_[0] = 1; // all zero would only ever give zeros
  }

  // Carries on from a saved state (state()). It mustn't be all zero.
  static constexpr Generator fromState(const State &state) {
    assert(state != State{});
    Generator generator(0);
    generator.state_ = state;
    return generator;
  }
  constexpr const State &state() const { return state_; }

  // The next 32 random bits.
  constexpr std::uint32_t next() {
    const std::uint32_t result = std::rotl(state_[1] * 5, 7) * 9;
    const std::uint32_t t = state_[1] << 9;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = std::rotl(state_[3], 11);
    return result;
  }
  constexpr result_type operator()() { return next(); }

  // A whole number from 0 up to, but not including, limit, every one equally
  // likely (Lemire's method: a multiply, and rarely a retry). limit must be
  // above 0.
  constexpr std::uint32_t below(std::uint32_t limit) {
    assert(limit > 0);
    std::uint64_t product = std::uint64_t{next()} * limit;
    auto low = static_cast<std::uint32_t>(product);
    if (low < limit) {
      const std::uint32_t threshold = (0u - limit) % limit; // 2^32 mod limit
      while (low < threshold) {
        product = std::uint64_t{next()} * limit;
        low = static_cast<std::uint32_t>(product);
      }
    }
    return static_cast<std::uint32_t>(product >> 32);
  }

  // A whole number from low to high, both included (a die: between(1, 6)).
  constexpr std::int32_t between(std::int32_t low, std::int32_t high) {
    assert(low <= high);
    const std::uint32_t count =
        static_cast<std::uint32_t>(high) - static_cast<std::uint32_t>(low) + 1;
    const std::uint32_t offset = count == 0 ? next() : below(count); // 0: every int32
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(low) + offset);
  }

  // True `times` times in `outOf`, on average (chance(1, 6): a six).
  constexpr bool chance(std::uint32_t times, std::uint32_t outOf) { return below(outOf) < times; }

  // A number from 0 up to, but not including, 1: a float with 24 random
  // bits, or a double with 53 (from two draws).
  constexpr float unitFloat() { return static_cast<float>(next() >> 8) * 0x1.0p-24f; }
  constexpr double unitDouble() {
    const std::uint32_t high = next() >> 5, low = next() >> 6; // 27 and 26 bits
    return (static_cast<double>(high) * 67108864.0 + static_cast<double>(low)) * 0x1.0p-53;
  }

  // Puts the elements in a random order, every order equally likely
  // (Fisher-Yates), the same everywhere for the same state.
  template <typename RandomIt> constexpr void shuffle(RandomIt first, RandomIt last) {
    using std::swap;
    for (auto count = static_cast<std::uint32_t>(std::distance(first, last)); count > 1; --count)
      swap(first[count - 1], first[below(count)]);
  }

  // Moves on 2^64 numbers in one go, as if next() were called that many
  // times: copies of one generator, each jumped a different number of times,
  // give sequences that never overlap.
  constexpr void jump() {
    constexpr std::uint32_t kJump[] = {0x8764000b, 0xf542d2d3, 0x6fa035c3, 0x77f2db5b};
    State jumped{};
    for (std::uint32_t word : kJump) {
      for (int bit = 0; bit < 32; ++bit) {
        if (word & (1u << bit)) {
          for (int i = 0; i < 4; ++i)
            jumped[i] ^= state_[i];
        }
        next();
      }
    }
    state_ = jumped;
  }

  constexpr bool operator==(const Generator &) const = default;

private:
  State state_;
};

} // namespace SeededRandom
