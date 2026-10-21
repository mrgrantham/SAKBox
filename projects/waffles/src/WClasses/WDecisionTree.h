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

#ifndef __GDECISIONTREE_H__
#define __GDECISIONTREE_H__

#include "WLearner.h"
#include <vector>

namespace WClasses
{

class WDecisionTreeNode;
class WRegressionTreeNode;
class WRand;
class WMeanMarginsTreeNode;
class WDecisionTreeLeafNode;
class WBag;


/// This is an efficient learning algorithm. It divides
/// on the attributes that reduce entropy the most, or alternatively
/// can make random divisions.
class WDecisionTree : public WSupervisedLearner
{
public:
	enum DivisionAlgorithm
	{
		MINIMIZE_ENTROPY,
		RANDOM,
	};

protected:
	WDecisionTreeNode* m_pRoot;
	DivisionAlgorithm m_eAlg;
	size_t m_leafThresh;
	size_t m_randomDraws;
	size_t m_maxLevels;
	bool m_binaryDivisions;

public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WDecisionTree();

	/// Loads from a DOM.
	WDecisionTree(const WDomNode* pNode);

	~WDecisionTree() override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Specifies for this decision tree to use random divisions (instead of
	/// divisions that reduce entropy). Random divisions make the algorithm
	/// train somewhat faster, and also increase model variance, so it is better
	/// suited for ensembles, but random divisions also make the decision tree
	/// vulnerable to problems with irrelevant attributes.
	void useRandomDivisions(size_t randomDraws = 1)
	{
		m_eAlg = RANDOM;
		m_randomDraws = randomDraws;
	}

	/// Returns the leaf threshold.
	size_t leafThresh() { return m_leafThresh; }

	/// Specify to only use binary divisions.
	void useBinaryDivisions();

	/// Returns true iff useBinaryDivisions was called.
	bool isBinary() { return m_binaryDivisions; }

	/// Sets the leaf threshold. When the number of samples is <= this value,
	/// it will no longer try to divide the data, but will create a leaf node.
	/// The default value is 1. For noisy data, a larger value may be advantageous.
	void setLeafThresh(size_t n) { m_leafThresh = n; }

	/// Sets the max levels.  When a path from the root to the
	/// current node contains n nodes (including the root), it
	/// will no longer try to divide the data, but will create a
	/// leaf node.  If set to 0, then there is no maximum.  0 is
	/// the default.
	void setMaxLevels(size_t n) { m_maxLevels = n; }

	/// Frees the model
	void clear() override;

	/// Returns the number of nodes in this tree
	size_t treeSize();

	/// Prints an ascii representation of the decision tree to the specified stream.
	/// pRelation is an optional relation that can be supplied in order to provide
	/// better meta-data to make the print-out richer.
	void print(std::ostream& stream, WArffRelation* pFeatureRel = nullptr, WArffRelation* pLabelRel = nullptr);

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& pIn, WVec& pOut) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& pIn, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// Finds the leaf node that corresponds with the specified feature vector
	WDecisionTreeLeafNode* findLeaf(const WVec& pIn, size_t* pDepth);

	/// A recursive helper method used to construct the decision tree
	WDecisionTreeNode* buildBranch(WMatrix& features, WMatrix& labels, std::vector<size_t>& attrPool, size_t nDepth, size_t tolerance);

	/// InfoGain is defined as the difference in entropy in the data
	/// before and after dividing it based on the specified attribute. For
	/// continuous attributes it uses the difference between the original
	/// variance and the sum of the variances of the two parts after
	/// dividing at the point the maximizes this value.
	double measureInfoGain(WMatrix* pData, size_t nAttribute, double* pPivot);

	size_t pickDivision(WMatrix& features, WMatrix& labels, double* pPivot, std::vector<size_t>& attrPool, size_t nDepth);
};



/// A WMeanMarginsTree is an oblique decision tree specified in
/// Gashler, Michael S. and Giraud-Carrier, Christophe and Martinez, Tony.
/// Decision Tree Ensemble: Small Heterogeneous Is Better Than Large
/// Homogeneous. In The Seventh International Conference on Machine
/// Learning and Applications, Pages 900 - 905, ICMLA '08. 2008.
/// It divides features as follows:
/// It finds the mean and principle component of the output vectors.
/// It divides all the vectors into two groups, one that has a
/// positive dot-product with the principle component (after subtracting
/// the mean) and one that has a negative dot-product with the
/// principle component (after subtracting the mean). Next it finds the
/// average input vector for each of the two groups. Then it finds
/// the mean and principle component of those two vectors. The dividing
/// criteria for this node is to subtract the mean and then see whether
/// the dot-product with the principle component is positive or negative.
class WMeanMarginsTree : public WSupervisedLearner
{
protected:
	size_t m_internalFeatureDims, m_internalLabelDims;
	WMeanMarginsTreeNode* m_pRoot;

public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WMeanMarginsTree();

	/// Load from a DOM.
	WMeanMarginsTree(const WDomNode* pNode);

	~WMeanMarginsTree() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& pIn, WVec& pOut) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& pIn, WPrediction* pOut) override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// This model has no parameters to tune, so this method is a noop.
	void autoTune(WMatrix& features, WMatrix& labels);

protected:
	WMeanMarginsTreeNode* buildNode(WMatrix& features, WMatrix& labels, size_t* pBuf2);

	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }
};



class WRandomForest : public WSupervisedLearner
{
protected:
	WBag* m_pEnsemble;

public:
	WRandomForest(size_t trees, size_t samples = 1);
	WRandomForest(const WDomNode* pNode, WLearnerLoader& ll);
	~WRandomForest() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Prints an ascii representation of the random forest to the specified stream.
	/// pRelation is an optional relation that can be supplied in order to provide
	/// better meta-data to make the print-out richer.
	void print(std::ostream& stream, WArffRelation* pFeatureRel = nullptr, WArffRelation* pLabelRel = nullptr);

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& pIn, WVec& pOut) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& pIn, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;
};

} // namespace WClasses

#endif // __GDECISIONTREE_H__
