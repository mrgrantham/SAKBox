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

#include "WKernelTrick.h"
#include "WHillClimber.h"
#include "WDistribution.h"
#include "WMath.h"

using namespace WClasses;

WDomNode* WKernel::makeBaseNode(WDom* pDoc) const
{
	WDomNode* pObj = pDoc->newObj();
	pObj->add(pDoc, "name", name());
	return pObj;
}

// static
WKernel* WKernel::deserialize(WDomNode* pNode)
{
	const char* szName = pNode->getString("name");
	if(strcmp(szName, "identity") == 0)
		return new WKernelIdentity(pNode);
	else if(strcmp(szName, "chisquared") == 0)
		return new WKernelChiSquared(pNode);
	else if(strcmp(szName, "polynomial") == 0)
		return new WKernelPolynomial(pNode);
	else if(strcmp(szName, "rbf") == 0)
		return new WKernelGaussianRBF(pNode);
	else if(strcmp(szName, "translate") == 0)
		return new WKernelTranslate(pNode);
	else if(strcmp(szName, "scale") == 0)
		return new WKernelScale(pNode);
	else if(strcmp(szName, "add") == 0)
		return new WKernelAdd(pNode);
	else if(strcmp(szName, "multiply") == 0)
		return new WKernelMultiply(pNode);
	else if(strcmp(szName, "pow") == 0)
		return new WKernelPow(pNode);
	else if(strcmp(szName, "exp") == 0)
		return new WKernelExp(pNode);
	else if(strcmp(szName, "normalize") == 0)
		return new WKernelNormalize(pNode);
	else
		throw Ex("Unrecognized activation function: ", szName);
	return nullptr;
}

WKernel* WKernel::kernelComplex1()
{
	//return new WKernelIdentity();
	//return new WKernelPolynomial(1, 7);

	WKernel* pK1 = new WKernelPolynomial(0, 3);
	WKernel* pK2 = new WKernelPolynomial(1, 7);
	WKernel* pK3 = new WKernelAdd(pK1, pK2);
	WKernel* pK4 = new WKernelNormalize(pK3);

	WKernel* pK5 = new WKernelGaussianRBF(0.01);
	WKernel* pK6 = new WKernelGaussianRBF(0.1);
	WKernel* pK7 = new WKernelAdd(pK5, pK6);
	WKernel* pK8 = new WKernelNormalize(pK7);

	WKernel* pK9 = new WKernelGaussianRBF(1.0);
	WKernel* pK10 = new WKernelGaussianRBF(10.0);
	WKernel* pK11 = new WKernelMultiply(pK9, pK10);
	WKernel* pK12 = new WKernelNormalize(pK11);

	WKernel* pK13 = new WKernelAdd(pK8, pK12);
	WKernel* pK14 = new WKernelAdd(pK4, pK13);
	return pK14;
}

