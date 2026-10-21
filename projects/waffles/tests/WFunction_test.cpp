// WFunctionParser.
#include <gtest/gtest.h>

#include <WClasses/WFunction.h>

#include <stdexcept>
#include <vector>

using namespace WClasses;

TEST(FunctionParserTest, NumbersMayStartWithAPoint) {
  WFunctionParser mfp;
  mfp.add("f(x)=x*.5+.25");
  WFunction *pFunc = mfp.getFunction("f");
  std::vector<double> params = {3.0};
  EXPECT_DOUBLE_EQ(pFunc->call(params, mfp), 1.75);
}

// A number in a formula was read with atof. The tokenizer only passes it
// well-formed numbers (1.2.3 is already two), but one too big for a double
// (1e999) became infinity.
TEST(FunctionParserTest, NumbersTooBigAreErrors) {
  WFunctionParser mfp;
  EXPECT_THROW(mfp.add("f(x)=x*1e999"), std::exception);
}
