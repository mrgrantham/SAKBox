// WRand: random deviates.
#include <gtest/gtest.h>

#include <WClasses/WRand.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using namespace WClasses;

// Poisson deviates with mu >= 30 come from a rejection method that rounds
// its candidate to the nearest integer. The rounding moved from
// (int)(x + 0.5) to lround; these values, recorded from the code before,
// pin down that the deviates didn't change.
TEST(RandTest, PoissonDeviatesForLargeMu) {
  WRand rand(1234);
  std::string seq;
  for (int i = 0; i < 12; i++)
    seq += std::to_string(rand.poisson(100.0)) + " ";
  long sum = 0;
  for (int i = 0; i < 10000; i++)
    sum += rand.poisson(40.0);
  EXPECT_EQ(seq, "123 106 104 98 102 104 96 113 90 100 88 100 ");
  EXPECT_EQ(sum, 399321); // a mean of 39.93
}

// WRand is a standard random bit generator: calling it is calling next()
TEST(RandGeneratorTest, CallingItIsNext) {
  WRand a(42), b(42);
  for (int i = 0; i < 100; i++)
    EXPECT_EQ(a(), b.next()) << i;
  WRandMersenneTwister mtA(42), mtB(42);
  for (int i = 0; i < 100; i++)
    EXPECT_EQ(mtA(), mtB.next()) << i;
}

// Its range is every 64-bit value (min() 0, max() all ones), which the
// distributions rely on: each bit is set about half the time
TEST(RandGeneratorTest, EveryBitIsUsed) {
  WRand rand(7);
  std::array<int, 64> set{};
  const int draws = 20000;
  for (int i = 0; i < draws; i++) {
    const uint64_t value = rand();
    for (int bit = 0; bit < 64; bit++)
      set[bit] += (value >> bit) & 1;
  }
  for (int bit = 0; bit < 64; bit++) {
    EXPECT_GT(set[bit], draws * 0.47) << "bit " << bit;
    EXPECT_LT(set[bit], draws * 0.53) << "bit " << bit;
  }
}

// The standard library's distributions and algorithms take it directly
TEST(RandGeneratorTest, WorksWithTheStandardLibrary) {
  WRand rand(3);
  std::uniform_int_distribution<int> die(1, 6);
  std::array<int, 7> faces{};
  for (int i = 0; i < 6000; i++) {
    const int face = die(rand);
    ASSERT_GE(face, 1);
    ASSERT_LE(face, 6);
    faces[face]++;
  }
  for (int face = 1; face <= 6; face++)
    EXPECT_NEAR(faces[face], 1000, 150) << face;

  std::vector<int> deck(52);
  for (int i = 0; i < 52; i++)
    deck[i] = i;
  std::vector<int> shuffled = deck;
  std::shuffle(shuffled.begin(), shuffled.end(), rand);
  EXPECT_NE(shuffled, deck);
  std::ranges::sort(shuffled);
  EXPECT_EQ(shuffled, deck); // the same cards

  std::vector<int> hand;
  std::sample(deck.begin(), deck.end(), std::back_inserter(hand), 5, rand);
  EXPECT_EQ(hand.size(), 5u);

  std::normal_distribution<double> normal(10.0, 2.0);
  double sum = 0;
  for (int i = 0; i < 10000; i++)
    sum += normal(rand);
  EXPECT_NEAR(sum / 10000, 10.0, 0.1);
}
