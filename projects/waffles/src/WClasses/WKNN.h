/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
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

#ifndef __GKNN_H__
#define __GKNN_H__

#include "WLearner.h"

namespace WClasses
{

class WNeighborFinderGeneralizing;
class WRand;
class WKnnScaleFactorCritic;
class WOptimizer;
class WRowDistanceScaled;
class WSparseSimilarity;
class WBitTable;


/// The k-Nearest Neighbor learning algorithm
class WKNN : public WIncrementalLearner
{
public:
	enum InterpolationMethod
	{
		Linear,
		Mean,
		Learner,
	};

	enum TrainMethod
	{
		StoreAll,
		ValidationPrune,
		DrawRandom,
	};

protected:
	// Settings
	WMatrix* m_pFeatures;
	WSparseMatrix* m_pSparseFeatures;
	WMatrix* m_pLabels;
	size_t m_nNeighbors;
	InterpolationMethod m_eInterpolationMethod;
	TrainMethod m_eTrainMethod;
	double m_trainParam;
	WSupervisedLearner* m_pLearner;
	bool m_bOwnLearner;

	// Scale Factor Optimization
	bool m_normalizeScaleFactors;
	bool m_optimizeScaleFactors;
	WDistanceMetric* m_pDistanceMetric;
	WSparseSimilarity* m_pSparseMetric;
	bool m_ownMetric;
	WKnnScaleFactorCritic* m_pCritic;
	WOptimizer* m_pScaleFactorOptimizer;

	// Working Buffers
	WVec m_valueCounts;

	// Neighbor Finding
	WNeighborFinderGeneralizing* m_pNeighborFinder;

public:
	/// General-purpose constructor
	WKNN();

	/// Load from a DOM.
	WKNN(const WDomNode* pNode);

	~WKNN() override;

	/// Returns the number of neighbors
	size_t neighborCount() { return m_nNeighbors; }

	/// Specify the number of neighbors to use. (The default is 1.)
	void setNeighborCount(size_t k);

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WIncrementalLearner::trainSparse
	void trainSparse(WSparseMatrix& features, WMatrix& labels) override;

	/// Discard any training (but not any settings) so it can be trained again
	void clear() override;

	/// Sets the distance metric to use for finding neighbors. If own is true, then
	/// this object will delete pMetric when it is done with it.
	void setMetric(WDistanceMetric* pMetric, bool own);

	/// Sets the sparse similarity metric to use for finding neighbors. If own is true, then
	/// this object will delete pMetric when it is done with it.
	void setMetric(WSparseSimilarity* pMetric, bool own);

	/// Sets the technique for interpolation. (If you want to use the "Learner" method,
	/// you should call SetInterpolationLearner instead of this method.)
	void setInterpolationMethod(InterpolationMethod eMethod);

	/// Sets the interpolation method to "Learner" and sets the learner to use. If
	/// bTakeOwnership is true, it will delete the learner when this object is deleted.
	void setInterpolationLearner(WSupervisedLearner* pLearner, bool bTakeOwnership);

	/// Adds a copy of pVector to the internal set.
	size_t addVector(const WVec& in, const WVec& out);

	/// Returns the dissimilarity metric
	WDistanceMetric* metric() { return m_pDistanceMetric; }

	/// Specify whether to normalize the scaling of each attribute. (The default is to normalize.)
	void setNormalizeScaleFactors(bool b);

	/// If you set this to true, it will use a hill-climber to optimize the
	/// attribute scaling factors. If you set it to false (the default), it won't.
	void setOptimizeScaleFactors(bool b);

	/// Returns the internal feature set
	WMatrix* features() { return m_pFeatures; }

	/// Returns the internal set of sparse features
	WSparseMatrix* sparseFeatures() { return m_pSparseFeatures; }

	/// Returns the internal label set
	WMatrix* labels() { return m_pLabels; }

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// Specify to train by drawing 'n' random patterns from the training set.
	void drawRandom(size_t n)
	{
		m_eTrainMethod = DrawRandom;
		m_trainParam = (double)n;
	}

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;

	/// Adds a vector to the internal set. Also, if the (k+1)th nearest
	/// neighbor of that vector is less than "elbow room" from it, then
	/// the closest neighbor is deleted from the internal set. (You might
	/// be wondering why the decision to delete the closest neighbor is
	/// determined by the distance of the (k+1)th neigbor. This enables a
	/// clump of k points to form in the most frequently sampled locations.
	/// Also, If you make this decision based on a closer neighbor, then big
	/// holes may form in the model if points are sampled in a poor order.)
	/// Call SetElbowRoom to specify the elbow room distance.
	void trainIncremental(const WVec& in, const WVec& out) override;

	/// Finds the nearest neighbors of pVector. Returns the number of neighbors found.
	size_t findNeighbors(const WVec& vector);

	/// Interpolate with each neighbor having equal vote
	void interpolateMean(size_t nc, const WVec& in, WPrediction* pOut, WVec* pOut2);

	/// Interpolate with each neighbor having a linear vote. (Actually it's linear with
	/// respect to the squared distance instead of the distance, because this is faster
	/// to compute.)
	void interpolateLinear(size_t nc, const WVec& in, WPrediction* pOut, WVec* pOut2);

	/// Interpolates with the provided supervised learning algorithm
	void interpolateLearner(size_t nc, const WVec& in, WPrediction* pOut, WVec* pOut2);

	/// See the comment for WTransducer::canImplicitlyHandleMissingFeatures
	bool canImplicitlyHandleMissingFeatures() override { return false; }
};


/// An instance-based transduction algorithm
class WNeighborTransducer : public WTransducer
{
protected:
	size_t m_friendCount;

public:
	/// General-purpose constructor
	WNeighborTransducer();

	/// Returns the number of neighbors.
	size_t neighbors() { return m_friendCount; }

	/// Specify the number of neighbors to use with each point.
	void setNeighbors(size_t k) { m_friendCount = k; }

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

protected:
	/// See the comment for WTransducer::transduce
	std::unique_ptr<WMatrix> transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }
};


/// This represents a grid of values. It might be useful as a Q-table with Q-learning.
class WInstanceTable : public WIncrementalLearner
{
protected:
	size_t m_dims;
	size_t* m_pDims;
	size_t* m_pScales;
	double* m_pTable;
	size_t m_product;

public:
	/// dims specifies the number of feature dimensions.
	/// pDims specifies the number of discrete zero-based values for each feature dim.
	WInstanceTable(size_t dims, size_t* pDims);
	~WInstanceTable() override;

	/// Serialize this table
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalLearner::trainSparse
	void trainSparse(WSparseMatrix& features, WMatrix& labels) override;

	/// Clears the internal model
	void clear() override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;

	/// See the comment for WIncrementalLearner::trainIncremental
	void trainIncremental(const WVec& in, const WVec& out) override;
};


/// An experimental instance-based learner that prunes attribute-values (or predicates) instead of entire instances.
/// This may be viewed as a form of rule-learning that beings with instance-based learning and then prunes.
class WSparseInstance : public WSupervisedLearner
{
protected:
	size_t m_neighborCount;
	WSparseMatrix* m_pInstanceFeatures;
	WMatrix* m_pInstanceLabels;
	WSparseSimilarity* m_pMetric;
	WBitTable* m_pSkipRows;

public:
	/// General-purpose constructor
	WSparseInstance();

	/// Load from a DOM.
	WSparseInstance(const WDomNode* pNode);

	~WSparseInstance() override;

	/// Returns the number of neighbors
	size_t neighborCount() { return m_neighborCount; }

	/// Specify the number of neighbors to use. (The default is 1.)
	void setNeighborCount(size_t k);

#ifndef NO_TEST_CODE
	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();
#endif

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Discard any training (but not any settings) so it can be trained again
	void clear() override;

	/// Sets the distance metric. Takes ownership of pMetric.
	void setMetric(WSparseSimilarity* pMetric);

protected:
	/// Throw out elements and/or entire instances one at-a-time until no improvements can be found
	void prune(const WMatrix& holdOutFeatures, const WMatrix& holdOutLabels);

	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WSupervisedLearner::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WSupervisedLearner::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }
};

} // namespace WClasses

#endif // __GKNN_H__
