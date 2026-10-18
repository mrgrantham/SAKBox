// GFunctionParser.
#include <gtest/gtest.h>

#include <WClasses/GFunction.h>

#include <vector>

using namespace GClasses;

TEST(FunctionParserTest, NumbersMayStartWithAPoint) {
  GFunctionParser mfp;
  mfp.add("f(x)=x*.5+.25");
  GFunction *pFunc = mfp.getFunction("f");
  std::vector<double> params = {3.0};
  EXPECT_DOUBLE_EQ(pFunc->call(params, mfp), 1.75);
}
