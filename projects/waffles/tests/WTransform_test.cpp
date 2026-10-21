// WNominalToCat and WDataPreprocessor::autoTrans.
#include <gtest/gtest.h>

#include <WClasses/WMatrix.h>
#include <WClasses/WTransform.h>

#include <memory>
#include <vector>

using namespace WClasses;

TEST(NominalToCatTest, OneHotOnlyBetweenThreeValuesAndTheCap) {
  // A number, then categories with 2, 3, 5, 11 and 12 values (cap 12)
  std::vector<size_t> valueCounts = {0, 2, 3, 5, 11, 12};
  WMixedRelation rel(valueCounts);
  WNominalToCat ntc(12);
  ntc.train(rel);
  // 1 + 1 + 3 + 5 + 11 + 1
  EXPECT_EQ(ntc.after().size(), 22u);

  // A row: number 0.25, then values 1, 2, 4, 10, 7
  WVec in(6), out(ntc.after().size());
  in[0] = 0.25;
  in[1] = 1;
  in[2] = 2;
  in[3] = 4;
  in[4] = 10;
  in[5] = 7;
  ntc.transform(in, out);
  EXPECT_EQ(out[0], 0.25);
  EXPECT_EQ(out[1], 1.0); // two values: one column, 0 or 1
  for (size_t i = 0; i < 3; i++)
    EXPECT_EQ(out[2 + i], i == 2 ? 1.0 : 0.0) << "3-valued, column " << i;
  for (size_t i = 0; i < 5; i++)
    EXPECT_EQ(out[5 + i], i == 4 ? 1.0 : 0.0) << "5-valued, column " << i;
  for (size_t i = 0; i < 11; i++)
    EXPECT_EQ(out[10 + i], i == 10 ? 1.0 : 0.0) << "11-valued, column " << i;
  EXPECT_EQ(ntc.after().valueCount(21), 0u); // 12 values: a single number

  // And back
  WVec back(6);
  ntc.untransform(out, back);
  for (size_t i = 0; i < 5; i++)
    EXPECT_EQ(back[i], in[i]) << "column " << i;
}

namespace {
// Whether autoTrans rescales a single number column holding [lo, hi] for a
// target range of [-1, 1], and if so where lo and hi end up
bool rescales(double lo, double hi, double *pLoOut = nullptr, double *pHiOut = nullptr) {
  WMatrix data(2, 1);
  data[0][0] = lo;
  data[1][0] = hi;
  std::unique_ptr<WIncrementalTransform> trans(
      WDataPreprocessor::autoTrans(data, false, false, true, -1.0, 1.0));
  if (!trans)
    return false;
  trans->train(data);
  WVec out(1);
  trans->transform(data[0], out);
  if (pLoOut)
    *pLoOut = out[0];
  trans->transform(data[1], out);
  if (pHiOut)
    *pHiOut = out[0];
  return true;
}
} // namespace

TEST(AutoTransTest, RescalesWhenOutOfRangeOrUsingLittleOfIt) {
  double lo, hi;
  EXPECT_TRUE(rescales(0.0, 5.0, &lo, &hi)); // past the top
  EXPECT_DOUBLE_EQ(lo, -1.0);
  EXPECT_DOUBLE_EQ(hi, 1.0);
  EXPECT_TRUE(rescales(-3.0, 0.5));          // past the bottom
  EXPECT_TRUE(rescales(0.0, 0.1, &lo, &hi)); // under a quarter of the range
  EXPECT_DOUBLE_EQ(lo, -1.0);
  EXPECT_DOUBLE_EQ(hi, 1.0);
  EXPECT_FALSE(rescales(-0.9, 0.9)); // fits and uses most of it
  EXPECT_FALSE(rescales(0.0, 0.6));  // over a quarter: left alone
  EXPECT_FALSE(rescales(0.3, 0.3));  // constant: nothing to spread out
}
