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

#ifndef __GPOLYNOMIAL_H__
#define __GPOLYNOMIAL_H__

#include "WLearner.h"
#include <vector>

namespace WClasses
{

class WPolynomialSingleLabel;


/// This regresses a multi-dimensional polynomial to fit the data
class WPolynomial : public WSupervisedLearner
{
protected:
	size_t m_controlPoints;
	std::vector<WPolynomialSingleLabel*> m_polys;

public:
	/// It will have the same number of control points in every feature dimension
	WPolynomial();

	/// Load from a DOM.
	WPolynomial(const WDomNode* pNode);

	~WPolynomial() override;

	/// Set the number of control points in the Bezier representation of the
	/// polynomial (which is one more than the polynomial order). The default
	/// is 3.
	void setControlPoints(size_t n);

	/// Returns the number of control points.
	size_t controlPoints();

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for WTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }
};


} // namespace WClasses

#endif // __GPOLYNOMIAL_H__
