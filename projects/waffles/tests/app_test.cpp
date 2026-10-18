// GApp's command-line helpers.
#include <gtest/gtest.h>

#include <WClasses/GApp.h>

#include <stdexcept>

using namespace GClasses;

// Command-line numbers: "abc" used to become 0.
TEST(ArgReaderTest, PopDoubleRejectsNonNumbers) {
  char good[] = "2.5", sci[] = "-1e-3", bad[] = "abc", trailing[] = "3x";
  char *argv[] = {good, sci, bad, trailing};
  GArgReader args(4, argv);
  EXPECT_EQ(args.pop_double(), 2.5);
  EXPECT_EQ(args.pop_double(), -1e-3);
  EXPECT_THROW(args.pop_double(), std::exception);
  EXPECT_THROW(args.pop_double(), std::exception);
}
