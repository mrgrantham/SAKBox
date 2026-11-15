// Seeds: codes that read back as the same seed, forgiving typing, and named
// streams that are repeatable and unrelated to each other.
#include <SeededRandom/Seed.hpp>

#include <gtest/gtest.h>

#include <bit>

using SeededRandom::Generator;
using SeededRandom::Seed;

namespace {
// The seed a code stands for (a failure if it isn't one)
Seed seedOf(std::string_view code) {
  const std::optional<Seed> seed = Seed::fromCode(code);
  EXPECT_TRUE(seed.has_value()) << code;
  return seed.value_or(Seed(0));
}
} // namespace

TEST(SeedTest, CodesAreEightCharactersInTwoGroups) {
  EXPECT_EQ(Seed(0).code(), "0000-0000");
  EXPECT_EQ(Seed(Seed::kMask).code(), "ZZZZ-ZZZZ");
  EXPECT_EQ(Seed(31).code(), "0000-000Z");
  EXPECT_EQ(Seed(std::uint64_t{1} << 35).code(), "1000-0000");
}

TEST(SeedTest, ACodeReadsBackAsItsSeed) {
  for (std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{123456789},
                              std::uint64_t{0xabcdef1234}, Seed::kMask}) {
    EXPECT_EQ(seedOf(Seed(value).code()).value(), value);
  }
}

// Lowercase, no dash, spaces, and the letters people mistake for digits.
TEST(SeedTest, TypingIsForgiving) {
  const Seed seed = seedOf("K7QM-3XPA");
  EXPECT_EQ(Seed::fromCode("k7qm3xpa"), seed);
  EXPECT_EQ(Seed::fromCode("K7QM 3XPA"), seed);
  EXPECT_EQ(Seed::fromCode("1O1O-1O1O"), Seed::fromCode("1010-1010"));
  EXPECT_EQ(Seed::fromCode("IL00-0000"), Seed::fromCode("1100-0000"));
}

TEST(SeedTest, WhatIsntACodeIsRefused) {
  EXPECT_FALSE(Seed::fromCode(""));
  EXPECT_FALSE(Seed::fromCode("K7QM-3XP"));   // too short
  EXPECT_FALSE(Seed::fromCode("K7QM-3XPAA")); // too long
  EXPECT_FALSE(Seed::fromCode("K7QM-3XPU"));  // U isn't in the code
  EXPECT_FALSE(Seed::fromCode("K7QM_3XPA"));
}

// Only 40 bits are kept, so every seed has a code.
TEST(SeedTest, SeedsHaveFortyBits) {
  EXPECT_EQ(Seed(~std::uint64_t{0}).value(), Seed::kMask);
  EXPECT_EQ(Seed((std::uint64_t{1} << 40) | 5).value(), 5u);
}

// Neighbouring numbers (the time a second apart) give unrelated seeds.
TEST(SeedTest, MixedFromSpreadsNearbyNumbers) {
  const std::uint64_t a = Seed::mixedFrom(1000).value(), b = Seed::mixedFrom(1001).value();
  EXPECT_NE(a, b);
  EXPECT_GT(std::popcount(a ^ b), 8);
  EXPECT_EQ(Seed::mixedFrom(1000), Seed::mixedFrom(1000));
}

TEST(SeedTest, AStreamIsRepeatable) {
  const Seed seed(12345);
  Generator a = seed.stream("course"), b = seed.stream("course");
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(a.next(), b.next());
}

// Streams with different names, or different seeds, share nothing, not even
// their first number.
TEST(SeedTest, StreamsDiffer) {
  const Seed seed(12345);
  Generator course = seed.stream("course"), bots = seed.stream("bots"),
            otherSeed = Seed(12346).stream("course");
  int same = 0;
  for (int i = 0; i < 1000; ++i) {
    const std::uint32_t value = course.next();
    same += value == bots.next();
    same += value == otherSeed.next();
  }
  EXPECT_EQ(same, 0);
}

// The numbers a seed and name give are part of the format: saved seeds and
// shared codes must keep making the same courses. Changing these means every
// code anyone has shared now makes something else.
TEST(SeedTest, StreamsArePinned) {
  Generator course = seedOf("K7QM-3XPA").stream("course");
  const std::uint32_t first = course.next(), second = course.next();
  EXPECT_EQ(first, 0xa951bc4eu) << std::hex << first;
  EXPECT_EQ(second, 0xc9658e97u) << std::hex << second;
}
