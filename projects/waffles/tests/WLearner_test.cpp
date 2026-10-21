// WAutoFilter.
#include <gtest/gtest.h>

#include <WClasses/WError.h>
#include <WClasses/WLearner.h>
#include <WClasses/WMatrix.h>
#include <WClasses/WTransform.h>
#include <WClasses/WVec.h>

#include <concepts>
#include <memory>

#include <vector>

using namespace WClasses;

namespace {
// A learner that wants values in [-1, 1] and records the range of the
// features and labels it is actually trained on
struct RangeRecorder : WSupervisedLearner {
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
  void trainInner(const WMatrix &features, const WMatrix &labels) override {
    featureMin = features.columnMin(0);
    featureMax = features.columnMax(0);
    labelMin = labels.columnMin(0);
    labelMax = labels.columnMax(0);
  }
  WDomNode *serialize(WDom *) const override { throw Ex("not needed"); }
  void predict(const WVec &, WVec &out) override { out.fill(0.0); }
  void predictDistribution(const WVec &, WPrediction *) override {}
  void clear() override {}
};

// Trains an auto-filtered RangeRecorder on one feature column holding
// [fLo, fHi] and one label column holding [lLo, lHi]
void trainThroughAutoFilter(RangeRecorder *pRec, double fLo, double fHi, double lLo, double lHi) {
  WAutoFilter af(pRec, false);
  WMatrix features(2, 1), labels(2, 1);
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

// WAutoFilter asked itself (a filter, which handles anything) instead of
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
      SCOPED_TRACE(testing::Message()
                   << "nominal feature " << nominalFeature << ", nominal label " << nominalLabel);
      std::vector<size_t> f = {0}, l = {0};
      if (nominalFeature)
        f.push_back(3);
      if (nominalLabel)
        l.push_back(3);
      WMatrix features(f), labels(l);
      for (size_t i = 0; i < 3; i++) {
        WVec &fr = features.newRow();
        WVec &lr = labels.newRow();
        fr[0] = lr[0] = 2.4 + 0.1 * i;
        if (nominalFeature)
          fr[1] = (double)i;
        if (nominalLabel)
          lr[1] = (double)i;
      }
      NeedsTwoToThree rec;
      WAutoFilter af(&rec, false);
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
  WMatrix features(3, 1), labels(category);
  for (size_t i = 0; i < 3; i++) {
    features[i][0] = (double)i;
    labels.newRow()[0] = (double)i;
  }
  NoLabelsAtAll rec;
  WAutoFilter af(&rec, false);
  EXPECT_THROW(af.train(features, labels), std::exception);
}

// A filter's prefilterFeatures and prefilterLabels return their input itself
// when there is nothing to transform, so the result of passing a temporary
// would refer to a destroyed vector. Passing one is a compile error, for
// every filter: WFilter declares both functions (with the deleted overloads
// for temporaries), and filters implement transformedFeatures and
// transformedLabels instead.
namespace {
// A filter written later, implementing only what a filter implements (left
// abstract: it is only checked, not made)
struct NewFilter : WFilter {
  using WFilter::WFilter;

protected:
  const WVec *transformedFeatures(const WVec &) override { return nullptr; }
  const WVec *transformedLabels(const WVec &) override { return nullptr; }
};

template <class F>
concept PrefiltersTemporaries = requires(F &f) {
  f.prefilterFeatures(WVec(2));
  f.prefilterLabels(WVec(2));
};
template <class F>
concept PrefiltersVectors = requires(F &f, const WVec &v) {
  { f.prefilterFeatures(v) } -> std::same_as<const WVec &>;
  { f.prefilterLabels(v) } -> std::same_as<const WVec &>;
};
} // namespace
static_assert(!PrefiltersTemporaries<WFilter>);
static_assert(!PrefiltersTemporaries<WFeatureFilter>);
static_assert(!PrefiltersTemporaries<WLabelFilter>);
static_assert(!PrefiltersTemporaries<WAutoFilter>);
static_assert(!PrefiltersTemporaries<NewFilter>);
static_assert(PrefiltersVectors<NewFilter>);
static_assert(PrefiltersVectors<WFeatureFilter>);
static_assert(PrefiltersVectors<WLabelFilter>);
static_assert(PrefiltersVectors<WAutoFilter>);

// The pass-through cases return the input itself, not a copy
TEST(FilterTest, PassThroughReturnsTheInput) {
  WVec v({1.0, 2.0});
  WFeatureFilter features(new WBaselineLearner(), new WNormalize());
  EXPECT_EQ(&features.prefilterLabels(v), &v);
  WLabelFilter labels(new WBaselineLearner(), new WNormalize());
  EXPECT_EQ(&labels.prefilterFeatures(v), &v);
  WAutoFilter autoFilter(new WBaselineLearner());
  EXPECT_EQ(&autoFilter.prefilterFeatures(v), &v);
  EXPECT_EQ(&autoFilter.prefilterLabels(v), &v);
}

// And the transforming cases: a feature filter normalizes the features, a
// label filter the labels, and a filter inside another gets what the outer
// one passes on (one transformed, the other passed through).
TEST(FilterTest, TransformsAndPassesOnThroughNestedFilters) {
  WMatrix features(4, 1), labels(4, 1);
  for (size_t i = 0; i < 4; i++) {
    features[i][0] = 10.0 * i;      // 0..30
    labels[i][0] = 100.0 * (i + 1); // 100..400
  }
  auto *inner = new WLabelFilter(new WBaselineLearner(), new WNormalize());
  WFeatureFilter outer(inner, new WNormalize());
  outer.train(features, labels);

  const WVec x({20.0}), y({300.0});
  const WVec &fx = outer.prefilterFeatures(x); // normalized by outer only
  EXPECT_NE(&fx, &x);
  EXPECT_DOUBLE_EQ(fx[0], 2.0 / 3.0);
  const WVec &fy = outer.prefilterLabels(y); // passed on to inner, normalized
  EXPECT_NE(&fy, &y);
  EXPECT_DOUBLE_EQ(fy[0], 2.0 / 3.0);
  // The inner filter alone: labels transformed, features passed through
  EXPECT_DOUBLE_EQ(inner->prefilterLabels(y)[0], 2.0 / 3.0);
  EXPECT_EQ(&inner->prefilterFeatures(x), &x);

  // A feature filter on its own (its inner learner isn't a filter)
  WFeatureFilter single(new WBaselineLearner(), new WNormalize());
  single.train(features, labels);
  EXPECT_DOUBLE_EQ(single.prefilterFeatures(x)[0], 2.0 / 3.0);
  EXPECT_EQ(&single.prefilterLabels(y), &y);

  // Whole matrices, through the same functions
  std::unique_ptr<WMatrix> pf(outer.prefilterFeatures(features));
  EXPECT_DOUBLE_EQ((*pf)[3][0], 1.0);
  std::unique_ptr<WMatrix> pl(outer.prefilterLabels(labels));
  EXPECT_DOUBLE_EQ((*pl)[0][0], 0.0);
}
