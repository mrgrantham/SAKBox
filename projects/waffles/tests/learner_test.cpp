// GAutoFilter.
#include <gtest/gtest.h>

#include <WClasses/GError.h>
#include <WClasses/GLearner.h>
#include <WClasses/GMatrix.h>

#include <vector>

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

// GAutoFilter asked itself (a filter, which handles anything) instead of
// the learner whether nominal values were a problem, so these rules never
// ran, and the features' rule checked the labels.
namespace {
// Wants features and labels in [2, 3] and can't take nominal values
// (nominal ones are one-hot coded to 0 and 1 for it)
struct NeedsTwoToThree : RangeRecorder {
  bool canImplicitlyHandleNominalFeatures() override { return false; }
  bool canImplicitlyHandleNominalLabels() override { return false; }
  bool supportedFeatureRange(double *pMin, double *pMax) override {
    *pMin = 2.0;
    *pMax = 3.0;
    return false;
  }
  bool supportedLabelRange(double *pMin, double *pMax) override {
    *pMin = 2.0;
    *pMax = 3.0;
    return false;
  }
};
} // namespace

// Numbers in [2.4, 2.6] use under a quarter of [2, 3], so they're spread
// out; with nominal values beside them, the range is widened to take in
// their one-hot 0 and 1 too, giving [0, 3].
TEST(AutoFilterTest, RangeTakesInOneHotValues) {
  for (bool nominalFeature : {false, true}) {
    for (bool nominalLabel : {false, true}) {
      SCOPED_TRACE(testing::Message() << "nominal feature " << nominalFeature
                                      << ", nominal label " << nominalLabel);
      std::vector<size_t> f = {0}, l = {0};
      if (nominalFeature)
        f.push_back(3);
      if (nominalLabel)
        l.push_back(3);
      GMatrix features(f), labels(l);
      for (size_t i = 0; i < 3; i++) {
        GVec &fr = features.newRow();
        GVec &lr = labels.newRow();
        fr[0] = lr[0] = 2.4 + 0.1 * i;
        if (nominalFeature)
          fr[1] = (double)i;
        if (nominalLabel)
          lr[1] = (double)i;
      }
      NeedsTwoToThree rec;
      GAutoFilter af(&rec, false);
      af.train(features, labels);
      EXPECT_DOUBLE_EQ(rec.featureMin, nominalFeature ? 0.0 : 2.0);
      EXPECT_DOUBLE_EQ(rec.featureMax, 3.0);
      EXPECT_DOUBLE_EQ(rec.labelMin, nominalLabel ? 0.0 : 2.0);
      EXPECT_DOUBLE_EQ(rec.labelMax, 3.0);
    }
  }
}

namespace {
// Can't take labels of either kind
struct NoLabelsAtAll : RangeRecorder {
  bool canImplicitlyHandleNominalLabels() override { return false; }
  bool canImplicitlyHandleContinuousLabels() override { return false; }
};
} // namespace

// A learner that can take neither kind of label is an error (it used to
// pass the check, since the filter answered for it)
TEST(AutoFilterTest, LearnerMustTakeSomeKindOfLabel) {
  std::vector<size_t> category = {3};
  GMatrix features(3, 1), labels(category);
  for (size_t i = 0; i < 3; i++) {
    features[i][0] = (double)i;
    labels.newRow()[0] = (double)i;
  }
  NoLabelsAtAll rec;
  GAutoFilter af(&rec, false);
  EXPECT_THROW(af.train(features, labels), std::exception);
}
