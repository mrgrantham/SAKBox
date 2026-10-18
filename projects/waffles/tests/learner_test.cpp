// GAutoFilter.
#include <gtest/gtest.h>

#include <WClasses/GError.h>
#include <WClasses/GLearner.h>
#include <WClasses/GMatrix.h>

using namespace GClasses;

namespace {
// A learner that wants values in [-1, 1] and records the range of the
// features and labels it is actually trained on
struct RangeRecorder : GSupervisedLearner {
  double featureMin = 0, featureMax = 0, labelMin = 0, labelMax = 0;
  bool supportedFeatureRange(double *pMin, double *pMax) override {
    *pMin = -1.0;
    *pMax = 1.0;
    return false;
  }
  bool supportedLabelRange(double *pMin, double *pMax) override {
    *pMin = -1.0;
    *pMax = 1.0;
    return false;
  }
  void trainInner(const GMatrix &features, const GMatrix &labels) override {
    featureMin = features.columnMin(0);
    featureMax = features.columnMax(0);
    labelMin = labels.columnMin(0);
    labelMax = labels.columnMax(0);
  }
  GDomNode *serialize(GDom *) const override { throw Ex("not needed"); }
  void predict(const GVec &, GVec &out) override { out.fill(0.0); }
  void predictDistribution(const GVec &, GPrediction *) override {}
  void clear() override {}
};

// Trains an auto-filtered RangeRecorder on one feature column holding
// [fLo, fHi] and one label column holding [lLo, lHi]
void trainThroughAutoFilter(RangeRecorder *pRec, double fLo, double fHi,
                            double lLo, double lHi) {
  GAutoFilter af(pRec, false);
  GMatrix features(2, 1), labels(2, 1);
  features[0][0] = fLo;
  features[1][0] = fHi;
  labels[0][0] = lLo;
  labels[1][0] = lHi;
  af.train(features, labels);
}
} // namespace

TEST(AutoFilterTest, RescalesValuesUsingLittleOfTheRange) {
  // Features use under a quarter of [-1, 1]: spread out. Labels use most of
  // it: left alone.
  RangeRecorder a;
  trainThroughAutoFilter(&a, 0.0, 0.1, -0.9, 0.9);
  EXPECT_DOUBLE_EQ(a.featureMin, -1.0);
  EXPECT_DOUBLE_EQ(a.featureMax, 1.0);
  EXPECT_DOUBLE_EQ(a.labelMin, -0.9);
  EXPECT_DOUBLE_EQ(a.labelMax, 0.9);

  // And the other way round
  RangeRecorder b;
  trainThroughAutoFilter(&b, -0.9, 0.9, 0.0, 0.1);
  EXPECT_DOUBLE_EQ(b.featureMin, -0.9);
  EXPECT_DOUBLE_EQ(b.featureMax, 0.9);
  EXPECT_DOUBLE_EQ(b.labelMin, -1.0);
  EXPECT_DOUBLE_EQ(b.labelMax, 1.0);

  // Out of range: rescaled into it
  RangeRecorder c;
  trainThroughAutoFilter(&c, 0.0, 5.0, -3.0, 0.5);
  EXPECT_DOUBLE_EQ(c.featureMin, -1.0);
  EXPECT_DOUBLE_EQ(c.featureMax, 1.0);
  EXPECT_DOUBLE_EQ(c.labelMin, -1.0);
  EXPECT_DOUBLE_EQ(c.labelMax, 1.0);
}
