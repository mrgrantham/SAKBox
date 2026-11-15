// The generator: exactly xoshiro128** (pinned against the authors' reference
// code), repeatable from a seed and from a saved state, and each kind of
// value spread evenly over its range.
#include <SeededRandom/Generator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <set>

using SeededRandom::Generator;

namespace {
constexpr int kSamples = 60000;
} // namespace

// The first outputs from the state {1, 2, 3, 4}, from the reference C code
// (prng.di.unimi.it/xoshiro128starstar.c).
TEST(GeneratorTest, MatchesTheReferenceCode) {
  Generator random = Generator::fromState({1, 2, 3, 4});
  const std::array<std::uint32_t, 6> expected = {0x00002d00, 0x00000000, 0x005a7080,
                                                 0x04389d80, 0x79199d9b, 0x61963b24};
  for (std::uint32_t value : expected)
    EXPECT_EQ(random.next(), value);
}

TEST(GeneratorTest, JumpMatchesTheReferenceCode) {
  Generator random = Generator::fromState({1, 2, 3, 4});
  random.jump();
  EXPECT_EQ(random.state(), (Generator::State{0xa9765206, 0x797aa168, 0x5b62e331, 0x02abd971}));
}

// A seed is spread over the state by splitmix64 (its first two outputs from
// 0 are 0xe220a8397b1dcdaf, a well-known value, then the second step).
TEST(GeneratorTest, SeedsAreSpreadBySplitMix64) {
  Generator random(0);
  EXPECT_EQ(random.state(), (Generator::State{0x7b1dcdaf, 0xe220a839, 0x4d197e6f, 0xa706dd2f}));
  EXPECT_EQ(random.next(), 0xdec9045du);
  EXPECT_EQ(random.next(), 0x2c756322u);
  EXPECT_EQ(random.next(), 0x20284051u);
}

TEST(GeneratorTest, TheSameSeedGivesTheSameSequence) {
  Generator a(42), b(42), c(43);
  bool differsFromOtherSeed = false;
  for (int i = 0; i < 100; ++i) {
    const std::uint32_t value = a.next();
    EXPECT_EQ(value, b.next());
    differsFromOtherSeed = differsFromOtherSeed || value != c.next();
  }
  EXPECT_TRUE(differsFromOtherSeed);
}

TEST(GeneratorTest, ASavedStateCarriesOnExactly) {
  Generator random(7);
  for (int i = 0; i < 10; ++i)
    random.next();
  Generator restored = Generator::fromState(random.state());
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(restored.next(), random.next());
}

// It works at compile time too, so a constant table can be made from a seed.
TEST(GeneratorTest, WorksAtCompileTime) {
  constexpr std::uint32_t kFirst = [] {
    Generator random(0);
    return random.next();
  }();
  static_assert(kFirst == 0xdec9045du);
}

// below() gives every value under the limit equally often, including for a
// limit that doesn't divide 2^32 evenly.
TEST(GeneratorTest, BelowIsEvenOverItsRange) {
  Generator random(1);
  constexpr std::uint32_t kLimit = 6;
  std::array<int, kLimit> counts{};
  for (int i = 0; i < kSamples; ++i) {
    const std::uint32_t value = random.below(kLimit);
    ASSERT_LT(value, kLimit);
    ++counts[value];
  }
  for (int count : counts)
    EXPECT_NEAR(count, double(kSamples) / kLimit, 0.05 * kSamples / kLimit);
  EXPECT_EQ(random.below(1), 0u);
}

TEST(GeneratorTest, BetweenIncludesBothEnds) {
  Generator random(2);
  std::set<std::int32_t> seen;
  for (int i = 0; i < 1000; ++i) {
    const std::int32_t value = random.between(-3, 3);
    ASSERT_GE(value, -3);
    ASSERT_LE(value, 3);
    seen.insert(value);
  }
  EXPECT_EQ(seen.size(), 7u);
  EXPECT_EQ(random.between(5, 5), 5);
  random.between(INT32_MIN, INT32_MAX); // the whole range is allowed
}

TEST(GeneratorTest, ChanceComesUpAsOftenAsItSays) {
  Generator random(3);
  int sixes = 0;
  for (int i = 0; i < kSamples; ++i)
    sixes += random.chance(1, 6);
  EXPECT_NEAR(sixes, kSamples / 6.0, 0.05 * kSamples / 6);
}

TEST(GeneratorTest, UnitNumbersAreSpreadOverZeroToOne) {
  Generator random(4);
  std::array<int, 10> floats{}, doubles{};
  for (int i = 0; i < kSamples; ++i) {
    const float f = random.unitFloat();
    const double d = random.unitDouble();
    ASSERT_TRUE(f >= 0 && f < 1) << f;
    ASSERT_TRUE(d >= 0 && d < 1) << d;
    ++floats[static_cast<int>(f * 10)];
    ++doubles[static_cast<int>(d * 10)];
  }
  for (int i = 0; i < 10; ++i) {
    EXPECT_NEAR(floats[i], kSamples / 10.0, 0.05 * kSamples / 10);
    EXPECT_NEAR(doubles[i], kSamples / 10.0, 0.05 * kSamples / 10);
  }
}

// The largest values: all bits set gives just under 1, never 1.
TEST(GeneratorTest, UnitNumbersNeverReachOne) {
  Generator random = Generator::fromState({1, 2, 3, 4});
  random.next(); // (the reference's second output is 0)
  EXPECT_EQ(random.unitFloat(), 0.0f);
  EXPECT_LT(static_cast<float>(0xffffffu) * 0x1.0p-24f, 1.0f);
  EXPECT_LT((134217727.0 * 67108864.0 + 67108863.0) * 0x1.0p-53, 1.0);
}

TEST(GeneratorTest, ShuffleKeepsEveryElementAndIsRepeatable) {
  std::array<int, 20> a{}, b{};
  std::iota(a.begin(), a.end(), 0);
  b = a;
  Generator first(5), second(5);
  first.shuffle(a.begin(), a.end());
  second.shuffle(b.begin(), b.end());
  EXPECT_EQ(a, b);
  std::array<int, 20> sorted = a;
  std::sort(sorted.begin(), sorted.end());
  for (int i = 0; i < 20; ++i)
    EXPECT_EQ(sorted[i], i);
  EXPECT_FALSE(std::is_sorted(a.begin(), a.end()));
}

// Every order of three comes up about equally often.
TEST(GeneratorTest, ShuffleGivesEveryOrderEquallyOften) {
  Generator random(6);
  int counts[6] = {};
  for (int i = 0; i < kSamples; ++i) {
    std::array<int, 3> order = {0, 1, 2};
    random.shuffle(order.begin(), order.end());
    ++counts[order[0] * 2 + (order[1] > order[2])];
  }
  for (int count : counts)
    EXPECT_NEAR(count, kSamples / 6.0, 0.05 * kSamples / 6);
}

#ifndef NDEBUG
TEST(GeneratorTest, BelowZeroIsAnError) {
  Generator random(0);
  EXPECT_DEATH(random.below(0), "limit > 0");
}
#endif
