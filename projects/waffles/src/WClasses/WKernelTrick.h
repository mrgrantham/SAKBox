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

#ifndef __GKERNELTRICK_H__
#define __GKERNELTRICK_H__

#include "WLearner.h"
#include "WVec.h"
#include "WDom.h"
#include <math.h>
#include <cmath>

namespace WClasses
{

class WRand;

/// The base class for kernel functions. Classes which implement this
/// must provide an "apply" method that applies the kernel to two
/// vectors. Kernels may be combined together to form a more complex
/// kernel, to which the kernel trick will still apply.
class WKernel
{
public:
	WKernel() {}
	virtual ~WKernel() {}

	/// Returns the name of this kernel.
	virtual const char* name() const = 0;

	/// Marshalls this object into a DOM.
	virtual WDomNode* serialize(WDom* pDoc) = 0;

	/// Applies the kernel to the two specified vectors.
	virtual double apply(const WVec& pA, const WVec& pB) = 0;

	/// Deserializes a kernel object
	static WKernel* deserialize(WDomNode* pNode);

	/// Returns a complex kernel made by combining several other kernels.
	/// This might be used to exercise kernel functionality or to test non-linear metrics.
	/// The caller is responsible to delete the object this returns.
	static WKernel* kernelComplex1();


protected:
	/// Helper method used by the serialize methods in child classes
	WDomNode* makeBaseNode(WDom* pDoc) const;
};

/// The identity kernel
class WKernelIdentity : public WKernel
{
public:
	WKernelIdentity() : WKernel() {}
	WKernelIdentity(WDomNode* pNode) : WKernel() {}
	~WKernelIdentity() override {}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = WKernel::makeBaseNode(pDoc);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "identity"; }

	/// Computes A*B
	double apply(const WVec& pA, const WVec& pB) override
	{
		return pA.dotProductIgnoringUnknowns(pB);
	}
};

/// Chi Squared kernel
class WKernelChiSquared : public WKernel
{
public:
	WKernelChiSquared() : WKernel() {}
	WKernelChiSquared(WDomNode* pNode) : WKernel() {}
	~WKernelChiSquared() override {}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "chisquared"; }

	/// Computes the sum over each element of 2 * a * b / (a + b)
	double apply(const WVec& pA, const WVec& pB) override
	{
		double d = 0.0;
		for(size_t i = 0; i < pA.size(); i++)
		{
			if(pA[i] != UNKNOWN_REAL_VALUE && pB[i] != UNKNOWN_REAL_VALUE)
				d += 2.0 * pA[i] * pB[i] / (pA[i] + pB[i]);
		}
		return d;
	}
};

/// A polynomial kernel
class WKernelPolynomial : public WKernel
{
protected:
	double m_offset;
	unsigned int m_order;

public:
	WKernelPolynomial(double offset, unsigned int order) : WKernel(), m_offset(std::abs(offset)), m_order(order) {}
	WKernelPolynomial(WDomNode* pNode) : WKernel(), m_offset(pNode->getDouble("offset")), m_order((unsigned int)pNode->getInt("order")) {}
	~WKernelPolynomial() override {}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "offset", m_offset);
		pObj->add(pDoc, "order", (long long)m_order);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "polynomial"; }

	/// Computes (A * B + offset)^order
	double apply(const WVec& pA, const WVec& pB) override
	{
		return pow(pA.dotProductIgnoringUnknowns(pB) + m_offset, (int)m_order);
	}
};

/// A Gaussian RBF kernel
class WKernelGaussianRBF : public WKernel
{
protected:
	double m_variance;

public:
	WKernelGaussianRBF(double variance) : WKernel(), m_variance(std::abs(variance)) {}
	WKernelGaussianRBF(WDomNode* pNode) : WKernel(), m_variance(pNode->getDouble("var")) {}
	~WKernelGaussianRBF() override {}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "var", m_variance);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "rbf"; }

	/// Computes e^(-0.5 * ||A - B||^2 / variance)
	double apply(const WVec& pA, const WVec& pB) override
	{
		return exp(-0.5 * pA.estimateSquaredDistanceWithUnknowns(pB) / m_variance);
	}
};

/// A translation kernel
class WKernelTranslate : public WKernel
{
protected:
	WKernel* m_pK;
	double m_value;

public:
	/// Takes ownership of pK
	WKernelTranslate(WKernel* pK, double value) : WKernel(), m_pK(pK), m_value(std::abs(value)) {}
	WKernelTranslate(WDomNode* pNode) : WKernel(), m_pK(WKernel::deserialize(pNode->get("k"))), m_value(pNode->getDouble("v")) {}
	~WKernelTranslate() override { delete(m_pK); }

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k", m_pK->serialize(pDoc));
		pObj->add(pDoc, "v", m_value);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "translate"; }

	/// Computes K(A, B) + value
	double apply(const WVec& pA, const WVec& pB) override
	{
		return m_pK->apply(pA, pB) + m_value;
	}
};

/// A scalar kernel
class WKernelScale : public WKernel
{
protected:
	WKernel* m_pK;
	double m_value;

public:
	/// Takes ownership of pK
	WKernelScale(WKernel* pK, double value) : WKernel(), m_pK(pK), m_value(std::abs(value)) {}
	WKernelScale(WDomNode* pNode) : WKernel(), m_pK(WKernel::deserialize(pNode->get("k"))), m_value(pNode->getDouble("v")) {}
	~WKernelScale() override { delete(m_pK); }

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k", m_pK->serialize(pDoc));
		pObj->add(pDoc, "v", m_value);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "scale"; }

	/// Computes K(A, B) * value
	double apply(const WVec& pA, const WVec& pB) override
	{
		return m_pK->apply(pA, pB) * m_value;
	}
};

/// An addition kernel
class WKernelAdd : public WKernel
{
protected:
	WKernel* m_pK1;
	WKernel* m_pK2;

public:
	/// Takes ownership of pK1 and pK2
	WKernelAdd(WKernel* pK1, WKernel* pK2) : WKernel(), m_pK1(pK1), m_pK2(pK2) {}
	WKernelAdd(WDomNode* pNode) : WKernel(), m_pK1(WKernel::deserialize(pNode->get("k1"))), m_pK2(WKernel::deserialize(pNode->get("k2"))) {}
	~WKernelAdd() override
	{
		delete(m_pK1);
		delete(m_pK2);
	}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k1", m_pK1->serialize(pDoc));
		pObj->add(pDoc, "k2", m_pK2->serialize(pDoc));
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "add"; }

	/// Computes K1(A, B) + K2(A, B)
	double apply(const WVec& pA, const WVec& pB) override
	{
		return m_pK1->apply(pA, pB) + m_pK2->apply(pA, pB);
	}
};

/// A multiplication kernel
class WKernelMultiply : public WKernel
{
protected:
	WKernel* m_pK1;
	WKernel* m_pK2;

public:
	/// Takes ownership of pK1 and pK2
	WKernelMultiply(WKernel* pK1, WKernel* pK2) : WKernel(), m_pK1(pK1), m_pK2(pK2) {}
	WKernelMultiply(WDomNode* pNode) : WKernel(), m_pK1(WKernel::deserialize(pNode->get("k1"))), m_pK2(WKernel::deserialize(pNode->get("k2"))) {}
	~WKernelMultiply() override
	{
		delete(m_pK1);
		delete(m_pK2);
	}

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k1", m_pK1->serialize(pDoc));
		pObj->add(pDoc, "k2", m_pK2->serialize(pDoc));
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "multiply"; }

	/// Computes K1(A, B) * K2(A, B)
	double apply(const WVec& pA, const WVec& pB) override
	{
		return m_pK1->apply(pA, pB) * m_pK2->apply(pA, pB);
	}
};

/// A power kernel
class WKernelPow : public WKernel
{
protected:
	WKernel* m_pK;
	double m_value;

public:
	/// Takes ownership of pK
	WKernelPow(WKernel* pK, unsigned int value) : WKernel(), m_pK(pK), m_value(value) {}
	WKernelPow(WDomNode* pNode) : WKernel(), m_pK(WKernel::deserialize(pNode->get("k"))), m_value(pNode->getDouble("v")) {}
	~WKernelPow() override { delete(m_pK); }

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k", m_pK->serialize(pDoc));
		pObj->add(pDoc, "v", m_value);
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "pow"; }

	/// Computes K(A, B)^value
	double apply(const WVec& pA, const WVec& pB) override
	{
		return pow(m_pK->apply(pA, pB), m_value);
	}
};

/// The Exponential kernel
class WKernelExp : public WKernel
{
protected:
	WKernel* m_pK;

public:
	/// Takes ownership of pK
	WKernelExp(WKernel* pK) : WKernel(), m_pK(pK) {}
	WKernelExp(WDomNode* pNode) : WKernel(), m_pK(WKernel::deserialize(pNode->get("k"))) {}
	~WKernelExp() override { delete(m_pK); }

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k", m_pK->serialize(pDoc));
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "exp"; }

	/// Computes e^K(A, B)
	double apply(const WVec& pA, const WVec& pB) override
	{
		return exp(m_pK->apply(pA, pB));
	}
};

/// A Normalizing kernel
class WKernelNormalize : public WKernel
{
protected:
	WKernel* m_pK;

public:
	/// Takes ownership of pK
	WKernelNormalize(WKernel* pK) : WKernel(), m_pK(pK) {}
	WKernelNormalize(WDomNode* pNode) : WKernel(), m_pK(WKernel::deserialize(pNode->get("k"))) {}
	~WKernelNormalize() override { delete(m_pK); }

	/// Marshalls this object into a DOM.
	WDomNode* serialize(WDom* pDoc) override
	{
		WDomNode* pObj = makeBaseNode(pDoc);
		pObj->add(pDoc, "k", m_pK->serialize(pDoc));
		return pObj;
	}

	/// Returns the name of this kernel
	const char* name() const override { return "normalize"; }

	/// Computes K(A, B) / sqrt(K(A, A) * K(B, B))
	double apply(const WVec& pA, const WVec& pB) override
	{
		return m_pK->apply(pA, pB) / sqrt(m_pK->apply(pA, pA) * m_pK->apply(pB, pB));
	}
};


} // namespace WClasses

#endif // __GKERNELTRICK_H__
