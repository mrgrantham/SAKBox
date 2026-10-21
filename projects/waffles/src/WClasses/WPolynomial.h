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

#include "GLearner.h"
#include <vector>

namespace GClasses
{

class GPolynomialSingleLabel;


/// This regresses a multi-dimensional polynomial to fit the data
class GPolynomial : public GSupervisedLearner
{
protected:
	size_t m_controlPoints;
	std::vector<GPolynomialSingleLabel*> m_polys;

public:
	/// It will have the same number of control points in every feature dimension
	GPolynomial();

	/// Load from a DOM.
	GPolynomial(const GDomNode* pNode);

	~GPolynomial() override;

	/// Set the number of control points in the Bezier representation of the
	/// polynomial (which is one more than the polynomial order). The default
	/// is 3.
	void setControlPoints(size_t n);

	/// Returns the number of control points.
	size_t controlPoints();

	/// Uses cross-validation to find a set of parameters that works well with
	/// the provided data.
	void autoTune(GMatrix& features, GMatrix& labels);

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	GDomNode* serialize(GDom* pDoc) const override;

	/// See the comment for GSupervisedLearner::clear
	void clear() override;

protected:
	/// See the comment for GSupervisedLearner::trainInner
	void trainInner(const GMatrix& features, const GMatrix& labels) override;

	/// See the comment for GSupervisedLearner::predict
	void predict(const GVec& in, GVec& out) override;

	/// See the comment for GSupervisedLearner::predictDistribution
	void predictDistribution(const GVec& in, GPrediction* pOut) override;

	/// See the comment for GTransducer::canImplicitlyHandleNominalFeatures
	bool canImplicitlyHandleNominalFeatures() override { return false; }

	/// See the comment for GTransducer::canImplicitlyHandleNominalLabels
	bool canImplicitlyHandleNominalLabels() override { return false; }
};


} // namespace GClasses

#endif // __GPOLYNOMIAL_H__
