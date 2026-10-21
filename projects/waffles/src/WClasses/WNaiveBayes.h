/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#ifndef __GNAIVEBAYES_H__
#define __GNAIVEBAYES_H__

#include "WLearner.h"

namespace WClasses
{

class WXMLTag;
struct WNaiveBayesOutputAttr;

/// A naive Bayes classifier
class WNaiveBayes : public WIncrementalLearner
{
protected:
	size_t m_nSampleCount;
	WNaiveBayesOutputAttr** m_pOutputs;
	double m_equivalentSampleSize;

public:
	WNaiveBayes();

	/// Load from a DOM.
	WNaiveBayes(const WDomNode* pNode);

	~WNaiveBayes() override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalLearner::trainSparse
	/// This method assumes that the values in pData are all binary values (0 or 1).
	void trainSparse(WSparseMatrix& features, WMatrix& labels) override;

	/// To ensure that unsampled values don't dominate the joint
	/// distribution by multiplying by a zero, each value is given
	/// at least as much representation as specified here. (The default
	/// is 0.5, which is as if there were half of a sample for each value.)
	void setEquivalentSampleSize(double d) { m_equivalentSampleSize = d; }

	/// Returns the equivalent sample size. (The number of samples of each
	/// possible value that is added by default to prevent zeros.)
	double equivalentSampleSize() { return m_equivalentSampleSize; }

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// Adds a single training sample to the collection
	void trainIncremental(const WVec& in, const WVec& out) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WTransducer::canImplicitlyHandleContinuousFeatures
	bool canImplicitlyHandleContinuousFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;
};

} // namespace WClasses

#endif // __GNAIVEBAYES_H__
