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

#ifndef __GLINEAR_H__
#define __GLINEAR_H__

#include "WLearner.h"
#include <vector>

namespace WClasses
{

class WPCA;

/// A linear regression model. Let f be a feature vector of real values, and let l be a label vector of real values,
/// then this model estimates l=Bf+e, where B is a matrix of real values, and e is a
/// vector of real values. (In the Wikipedia article on linear regression, B is called
/// "beta", and e is called "epsilon". The approach used by this model to compute
/// beta and epsilon, however, is much more efficient than the approach currently
/// described in that article.)
class WLinearRegressor : public WSupervisedLearner
{
protected:
	WMatrix* m_pBeta;
	WVec m_epsilon;

public:
	WLinearRegressor();

	/// Load from a text-format
	WLinearRegressor(const WDomNode* pNode);

	~WLinearRegressor() override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Saves the model to a text file. (This doesn't save the short-term
	/// memory used for incremental learning, so if you're doing "incremental"
	/// learning, it will wake up with amnesia when you load it again.)
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Returns the matrix that represents the linear transformation.
	WMatrix* beta() { return m_pBeta; }

	/// Returns the vector that is added to the results after the linear transformation is applied.
	WVec& epsilon() { return m_epsilon; }

	/// Performs on-line gradient descent to refine the model
	void refine(const WMatrix& features, const WMatrix& labels, double learningRate, size_t epochs, double learningRateDecayFactor);

	/// This model has no parameters to tune, so this method is a noop.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& pIn, WVec& pOut) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& pIn, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleMissingFeatures
	bool canImplicitlyHandleMissingFeatures() override { return false; }
};



/// A linear regression model that predicts a distribution. (This algorithm also differs from WLinearRegressor
/// in that it computes its model in closed form instead of using a numerical approach to find it. In general,
/// WLinearRegressor seems to be a bit more accurate. Perhaps the method used in this algorithm is not very
/// numerically stable.)
class WLinearDistribution : public WSupervisedLearner
{
protected:
	double m_noiseDev;
	WMatrix* m_pAInv;
	WMatrix* m_pWBar;
	WVec m_buf;

public:
	/// General-purpose constructor
	WLinearDistribution();

	/// Deserialization constructor
	WLinearDistribution(const WDomNode* pNode);

	/// Destructor
	~WLinearDistribution() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Specify the prior expected deviation of the noise (The default is 1.0.)
	void setNoiseDeviation(double d) { m_noiseDev = d; }

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& pIn, WVec& pOut) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& pIn, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleMissingFeatures
	bool canImplicitlyHandleMissingFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }
};





class WLinearProgramming
{
public:
	/// Compute x that maximizes c*x, subject to Ax<=b, x>=0.
	/// The size of pB is the number of rows in pA.
	/// The size of pC is the number of columns in pA.
	/// leConstraints specifies the number of <= constraints. (These must come first in order.)
	/// geConstraints specifies the number of >= constraints. (These come next.)
	/// The remaining constraints are assumed to be = constraints.
	/// The answer is put in pOutX, which is the same size as pC.
	/// Returns false if there is no solution, and true if it finds a solution.
	static bool simplexMethod(WMatrix* pA, const double* pB, int leConstraints, int geConstraints, const double* pC, double* pOutX);

	/// Perform unit tests for this class. Throws an exception if any tests fail. Returns if they all pass.
	static void test();
};




} // namespace WClasses

#endif // __GLINEAR_H__

