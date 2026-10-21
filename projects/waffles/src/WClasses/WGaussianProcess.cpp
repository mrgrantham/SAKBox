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

#include "WGaussianProcess.h"
#include "WRand.h"
#include "WDom.h"
#include "WVec.h"
#include "WDistribution.h"
#include "WKernelTrick.h"
#include "WHolders.h"
#include <cmath>
#include <memory>

namespace WClasses
{

WRunningCovariance::WRunningCovariance(size_t dims) : m_counts(dims, dims), m_sums(dims + 1, dims)
{
	m_counts.fill(0.0);
	m_sums.fill(0.0);
}

WRunningCovariance::~WRunningCovariance()
{
}

double WRunningCovariance::element(size_t row, size_t col)
{
	if(col < row)
		std::swap(row, col);
	double n = m_counts[row][col];
	if(n <= 1.0)
		return UNKNOWN_REAL_VALUE;
	return n / (n - 1) * (m_sums[row][col] / m_counts[row][col] - (m_sums[m_counts.rows()][row] * m_sums[m_counts.rows()][col]) / (m_counts[row][row] * m_counts[col][col]));
}

void WRunningCovariance::add(const WVec& vec)
{
	for(size_t i = 0; i < m_counts.rows(); i++)
	{
		if(vec[i] != UNKNOWN_REAL_VALUE)
		{
			m_sums[m_counts.rows()][i] += vec[i];
			for(size_t j = i; j < m_counts.rows(); j++)
			{
				if(vec[j] != UNKNOWN_REAL_VALUE)
				{
					m_counts[i][j]++;
					m_sums[i][j] += vec[i] * vec[j];
				}
			}
		}
	}
}

void WRunningCovariance::decay(double gamma)
{
	m_counts.multiply(gamma);
	m_sums.multiply(gamma);
}

// static
void WRunningCovariance::test()
{
	WRand rand(0);
	WMatrix m(13, 7);
	for(size_t i = 0; i < m.rows(); i++)
		m[i].fillUniform(rand);
	WMatrix* pCov1 = m.covarianceMatrix();
	std::unique_ptr<WMatrix> hCov1(pCov1);
	m.centerMeanAtOrigin();
	WRunningCovariance rc(m.cols());
	for(size_t i = 0; i < m.rows(); i++)
		rc.add(m[i]);
	for(size_t i = 0; i < m.cols(); i++)
	{
		for(size_t j = 0; j < m.cols(); j++)
		{
			if(std::abs(rc.element(i, j) - pCov1->row(i)[j]) > 1e-12)
				throw Ex("failed");
		}
	}
}









WGaussianProcess::WGaussianProcess()
: WSupervisedLearner(), m_noiseVar(1.0), m_weightsPriorVar(1024.0), m_maxSamples(350), m_pLInv(nullptr), m_pAlpha(nullptr), m_pStoredFeatures(nullptr), m_pBuf(nullptr)
{
	m_pKernel = new WKernelIdentity();
}

WGaussianProcess::WGaussianProcess(const WDomNode* pNode)
: WSupervisedLearner(pNode), m_pBuf(nullptr)
{
	m_weightsPriorVar = pNode->getDouble("wv");
	m_noiseVar = pNode->getDouble("nv");
	m_maxSamples = (size_t)pNode->getInt("ms");
	m_pLInv = new WMatrix(pNode->get("l"));
	m_pAlpha = new WMatrix(pNode->get("a"));
	m_pStoredFeatures = new WMatrix(pNode->get("feat"));
	m_pKernel = WKernel::deserialize(pNode->get("kernel"));
}

// virtual
WGaussianProcess::~WGaussianProcess()
{
	clear();
	delete(m_pKernel);
}

// static
void WGaussianProcess::test()
{
	WAutoFilter af(new WGaussianProcess());
	af.basicTest(0.693, 0.94);
	af.clear();
	WGaussianProcess* pGP = new WGaussianProcess();
	pGP->setKernel(new WKernelGaussianRBF(0.2));
	WAutoFilter af2(pGP);
	af2.basicTest(0.67, 0.92);
}

// virtual
WDomNode* WGaussianProcess::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WGaussianProcess");
	pNode->add(pDoc, "wv", m_weightsPriorVar);
	pNode->add(pDoc, "nv", m_noiseVar);
	pNode->add(pDoc, "ms", m_maxSamples);
	pNode->add(pDoc, "l", m_pLInv->serialize(pDoc));
	pNode->add(pDoc, "a", m_pAlpha->serialize(pDoc));
	pNode->add(pDoc, "feat", m_pStoredFeatures->serialize(pDoc));
	pNode->add(pDoc, "kernel", m_pKernel->serialize(pDoc));
	return pNode;
}

// virtual
void WGaussianProcess::clear()
{
	delete(m_pLInv);
	m_pLInv = nullptr;
	delete(m_pAlpha);
	m_pAlpha = nullptr;
	delete(m_pStoredFeatures);
	m_pStoredFeatures = nullptr;
	delete(m_pBuf);
	m_pBuf = nullptr;
}

// virtual
void WGaussianProcess::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(!features.relation().areContinuous())
		throw Ex("WGaussianProcess only supports continuous features. Perhaps you should wrap it in a WAutoFilter.");
	if(!labels.relation().areContinuous())
		throw Ex("WGaussianProcess only supports continuous labels. Perhaps you should wrap it in a WAutoFilter.");
	if(features.rows() <= m_maxSamples)
	{
		trainInnerInner(features, labels);
		return;
	}
	WMatrix f(features.relation().clone());
	WReleaseDataHolder hF(&f);
	WMatrix l(labels.relation().clone());
	WReleaseDataHolder hL(&l);
	for(size_t i = 0; i < features.rows(); i++)
	{
		f.takeRow((WVec*)&features[i]);
		l.takeRow((WVec*)&labels[i]);
	}
	while(f.rows() > m_maxSamples)
	{
		size_t i = (size_t)m_rand.next(f.rows());
		f.releaseRow(i);
		l.releaseRow(i);
	}
	trainInnerInner(f, l);
}

void WGaussianProcess::trainInnerInner(const WMatrix& features, const WMatrix& labels)
{
	clear();
	WMatrix* pL;
	{
		// Compute the kernel matrix
		WMatrix k(features.rows(), features.rows());
		for(size_t i = 0; i < features.rows(); i++)
		{
			WVec& row = k[i];
			const WVec& a = features[i];
			for(size_t j = 0; j < features.rows(); j++)
			{
				const WVec& b = features[j];
				row[j] = m_weightsPriorVar * m_pKernel->apply(a, b);
			}
		}

		// Add the noise variance to the diagonal of the kernel matrix
		for(size_t i = 0; i < features.rows(); i++)
			k[i][i] += m_noiseVar;

		// Compute L
		pL = k.cholesky(true);
	}
	std::unique_ptr<WMatrix> hL(pL);

	// Compute the model
	m_pLInv = pL->pseudoInverse();
	WMatrix* pTmp = WMatrix::multiply(*m_pLInv, labels, false, false);
	std::unique_ptr<WMatrix> hTmp(pTmp);
	WMatrix* pLTrans = pL->transpose();
	std::unique_ptr<WMatrix> hLTrans(pLTrans);
	WMatrix* pLTransInv = pLTrans->pseudoInverse();
	std::unique_ptr<WMatrix> hLTransInv(pLTransInv);
	m_pAlpha = WMatrix::multiply(*pLTransInv, *pTmp, false, false);
	WAssert(m_pAlpha->rows() == features.rows());
	WAssert(m_pAlpha->cols() == labels.cols());
	m_pStoredFeatures = new WMatrix();
	m_pStoredFeatures->copy(features);
}

// virtual
void WGaussianProcess::predict(const WVec& in, WVec& out)
{
	if(!m_pBuf)
		m_pBuf = new WMatrix(1, m_pStoredFeatures->rows());

	// Compute k*
	WVec& k = m_pBuf->row(0);
	for(size_t i = 0; i < m_pStoredFeatures->rows(); i++)
		k[i] = m_weightsPriorVar * m_pKernel->apply(m_pStoredFeatures->row(i), in);

	// Compute the prediction
	m_pAlpha->multiply(m_pBuf->row(0), out, true);
}

// virtual
void WGaussianProcess::predictDistribution(const WVec& in, WPrediction* out)
{
	if(!m_pBuf)
		m_pBuf = new WMatrix(2, m_pStoredFeatures->rows());
	else if(m_pBuf->rows() < 2)
		m_pBuf->newRow();

	// Compute k*
	WVec& k = m_pBuf->row(0);
	for(size_t i = 0; i < m_pStoredFeatures->rows(); i++)
		k[i] = m_weightsPriorVar * m_pKernel->apply(m_pStoredFeatures->row(i), in);

	// Compute the prediction
	WVec pred;
	m_pAlpha->multiply(m_pBuf->row(0), pred, true);

	// Compute the variance
	WVec& v = m_pBuf->row(1);
	m_pLInv->multiply(k, v);
	double variance = m_pKernel->apply(k, k) - v.squaredMagnitude();

	// Store the results
	for(size_t i = 0; i < m_pAlpha->cols(); i++)
	{
		WNormalDistribution* pNorm = out->makeNormal();
		pNorm->setMeanAndVariance(pred[i], variance);
	}
}

void WGaussianProcess::setKernel(WKernel* pKernel)
{
	delete(m_pKernel);
	m_pKernel = pKernel;
}


} // namespace WClasses
