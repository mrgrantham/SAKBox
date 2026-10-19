// GFunctionParser.
#include <gtest/gtest.h>

#include <WClasses/GFunction.h>

#include <vector>
#include <stdexcept>

using namespace GClasses;

TEST(FunctionParserTest, NumbersMayStartWithAPoint) {
  GFunctionParser mfp;
  mfp.add("f(x)=x*.5+.25");
  GFunction *pFunc = mfp.getFunction("f");
  std::vector<double> params = {3.0};
  EXPECT_DOUBLE_EQ(pFunc->call(params, mfp), 1.75);
}

// A number in a formula was read with atof. The tokenizer only passes it
// well-formed numbers (1.2.3 is already two), but one too big for a double
// (1e999) became infinity.
TEST(FunctionParserTest, NumbersTooBigAreErrors) {
  GFunctionParser mfp;
  EXPECT_THROW(mfp.add("f(x)=x*1e999"), std::exception);
}
