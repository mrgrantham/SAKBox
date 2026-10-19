// GRand: random deviates.
#include <gtest/gtest.h>

#include <WClasses/GRand.h>

#include <string>

using namespace GClasses;

// Poisson deviates with mu >= 30 come from a rejection method that rounds
// its candidate to the nearest integer. The rounding moved from
// (int)(x + 0.5) to lround; these values, recorded from the code before,
// pin down that the deviates didn't change.
TEST(RandTest, PoissonDeviatesForLargeMu) {
  GRand rand(1234);
  std::string seq;
  for (int i = 0; i < 12; i++)
    seq += std::to_string(rand.poisson(100.0)) + " ";
  long sum = 0;
  for (int i = 0; i < 10000; i++)
    sum += rand.poisson(40.0);
  EXPECT_EQ(seq, "123 106 104 98 102 104 96 113 90 100 88 100 ");
  EXPECT_EQ(sum, 399321); // a mean of 39.93
}
