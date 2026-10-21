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

#include "WDistribution.h"
#include "WDom.h"
#include "WVec.h"
#include "WRand.h"
#include "WMath.h"
#include "WMatrix.h"
#include "WHolders.h"
#include <cmath>
#include <memory>

using namespace WClasses;
using std::map;

void WCategoricalDistribution::deserialize(WDomNode* pNode)
{
	WDomListIterator it(pNode);
	m_nValueCount = it.remaining();
	m_pValues.resize(m_nValueCount);
	m_nMode = 0;
	for(size_t i = 0; i < m_nValueCount; i++)
	{
		m_pValues[i] = it.currentDouble();
		if(m_pValues[i] > m_pValues[m_nMode])
			m_nMode = i;
		it.advance();
	}
}

WDomNode* WCategoricalDistribution::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newList();
	for(size_t i = 0; i < m_nValueCount; i++)
		pNode->add(pDoc, m_pValues[i]);
	return pNode;
}

// virtual
bool WCategoricalDistribution::isSupported(double x) const
{
	size_t nIndex = (size_t)floor(x + 0.5);
	return (nIndex < m_nValueCount);
}

// virtual
double WCategoricalDistribution::logLikelihood(double x)
{
	return log(likelihood(x));
}

void WCategoricalDistribution::normalize()
{
	m_nMode = 0;
	if(m_pValues[0] < 0)
		m_pValues[0] = 0;
	double sum = m_pValues[0];
	for(size_t i = 1; i < m_nValueCount; i++)
	{
		if(m_pValues[i] < 0)
		{
			//WAssert(m_pValues[i] >= -1e-9); // Expected a non-negative value
			m_pValues[i] = 0;
		}
		else if(m_pValues[i] > m_pValues[m_nMode])
			m_nMode = i;
		sum += m_pValues[i];
	}
	if(sum > 0)
	{
		for(size_t i = 0; i < m_nValueCount; i++)
			m_pValues[i] /= sum;
	}
	else
		setToUniform(m_nValueCount);
}

void WCategoricalDistribution::normalizeFromLogSpace()
{
	// Adjust the average value to about 1, and then convert from log space
	double sum = 0;
	size_t count = 0;
	for(size_t i = 0; i < m_nValueCount; i++)
	{
		if(m_pValues[i] > -1e50)
		{
			sum += m_pValues[i];
			count++;
		}
	}
	double delta = 1.0 - sum / count;
	for(size_t i = 0; i < m_nValueCount; i++)
		m_pValues[i] = exp(m_pValues[i] + delta);
	normalize();
}

void WCategoricalDistribution::setValues(size_t nValueCount, const double* pValues)
{
	values(nValueCount); // Make sure the right amount of space is allocated
	m_pValues.copy(pValues, nValueCount);
	normalize();
}

void WCategoricalDistribution::setValuesInferLast(size_t nValueCount, const double* pValues)
{
	values(nValueCount + 1); // Make sure the right amount of space is allocated
	double* pV = m_pValues.data();
	double rem = 1.0;
	for(size_t i = 0; i < nValueCount; i++)
	{
		rem -= *pValues;
		*(pV++) = *(pValues++);
	}
	*pV = rem;
	normalize();
}

void WCategoricalDistribution::setSpike(size_t nValueCount, size_t nValue, size_t nDepth)
{
	WAssert(nDepth > 0); // nDepth should be at least 1
	values(nValueCount); // Make sure the right amount of space is allocated
	if(nValue >= nValueCount)
	{
		m_nMode = -1;
		for(size_t i = 0; i < nValueCount; i++)
			m_pValues[i] = 1;
	}
	else
	{
		m_nMode = nValue;
		double d = 1.0 - 1.0 / nDepth;
		for(size_t i = 0; i < nValueCount; i++)
			m_pValues[i] = d;
		m_pValues[nValue] = 1;
	}
	normalize();
}

double WCategoricalDistribution::entropy()
{
	double dEntropy = 0;
	for(size_t i = 0; i < m_nValueCount; i++)
		dEntropy -= (m_pValues[i] * log(m_pValues[i]) * M_LOG2E);
	return dEntropy;
}




WCategoricalSampler::WCategoricalSampler(size_t categories, const double* pDistribution)
{
	double sum = 0.0;
	for(size_t i = 0; i < categories; i++)
	{
		if(*pDistribution <= 0)
		{
			if(*pDistribution < 0)
				throw Ex("Negative probabilities are not allowed");
			continue;
		}
		sum += *pDistribution;
		m_map.insert(std::pair<double, size_t>(sum, i));
	}
	if(std::abs(sum - 1.0) > 1e-8)
		throw Ex("The probabilities should sum to 1");
}

size_t WCategoricalSampler::draw(double d)
{
	map<double, size_t>::iterator it = m_map.upper_bound(d);
	if(it == m_map.end())
		return 0;
	return it->second;
}





WCategoricalSamplerBatch::WCategoricalSamplerBatch(size_t categories, const WVec& distribution, WRand& rand)
: m_categories(categories), m_distribution(distribution), m_ii(m_categories, rand)
{
}

WCategoricalSamplerBatch::~WCategoricalSamplerBatch()
{
}

void WCategoricalSamplerBatch::draw(size_t samples, size_t* pOutBatch)
{
	double probRemaining = 1.0;
	m_ii.reset();
	size_t index;
	size_t* pOut = pOutBatch;
	size_t n = samples;
	while(m_ii.next(index))
	{
		double prob = m_distribution[index];
		size_t k = m_ii.rand().binomial_approx(n, prob / probRemaining);
		WAssert(k <= n);
		for(size_t j = 0; j < k; j++)
		{
			*pOut = index;
			pOut++;
		}
		n -= k;
		probRemaining -= prob;
	}
	WAssert(n == 0);
	WAssert(std::abs(probRemaining) < 1e-6);
	WIndexVec::shuffle(pOutBatch, samples, &m_ii.rand());
}

#define SAMPLES 10000
// static
void WCategoricalSamplerBatch::test()
{
	WVec probs(3);
	probs[0] = 0.2;
	probs[1] = 0.5;
	probs[2] = 0.3;
	WRand rand(0);
	WCategoricalSamplerBatch csb(3, probs, rand);
	size_t* pResults = new size_t[SAMPLES];
	std::unique_ptr<size_t[]> hResults(pResults);
	csb.draw(SAMPLES, pResults);
	size_t counts[3];
	counts[0] = 0;
	counts[1] = 0;
	counts[2] = 0;
	for(size_t i = 0; i < SAMPLES; i++)
	{
		size_t n = pResults[i];
		if(n > 2)
			throw Ex("out of range");
		counts[n]++;
	}
	if(std::abs(0.2 - double(counts[0]) / SAMPLES) >= 0.02)
		throw Ex("failed");
	if(std::abs(0.5 - double(counts[1]) / SAMPLES) >= 0.02)
		throw Ex("failed");
	if(std::abs(0.3 - double(counts[2]) / SAMPLES) >= 0.02)
		throw Ex("failed");
}




void WNormalDistribution::precompute()
{
	m_height = 1.0 / sqrt(2.0 * M_PI * m_variance);
}

// virtual
double WPoissonDistribution::logLikelihood(double x)
{
	return -m_rate + x * log(m_rate) - WMath::logFactorial((int)floor(x + .5));
}

// virtual
double WPoissonDistribution::likelihood(double x)
{
	int k = (int)floor(x);
	double d = exp(-m_rate) * pow(m_rate, k);
	while(k > 1)
		d /= k--;
	return d;
}

// virtual
double WGammaDistribution::logLikelihood(double x)
{
	return (m_shape - 1) * log(x) - (x / m_scale) - m_shape * log(m_scale) - WMath::logGamma(m_shape);
}

// virtual
double WGammaDistribution::likelihood(double x)
{
	return exp(logLikelihood(x));
}

// virtual
double WInverseGammaDistribution::likelihood(double x)
{
	return exp(logLikelihood(x));
}

// virtual
double WBetaDistribution::logLikelihood(double x)
{
	return (m_alpha - 1) * log(x) + (m_beta - 1) * log(1.0 - x) + WMath::logGamma(m_alpha + m_beta) - WMath::logGamma(m_alpha) - WMath::logGamma(m_beta);
}

// virtual
double WBetaDistribution::likelihood(double x)
{
	return exp(logLikelihood(x));
}

// virtual
double WSoftImpulseDistribution::likelihood(double x)
{
	double t = (1.0 / x - 1.0);
	double u = pow(t, m_steepness - 1.0);
	double v = u * t + 1.0;
	return m_steepness * u / (v * v * x * x);
}

// virtual
double WSoftImpulseDistribution::logLikelihood(double x)
{
	return log(likelihood(x));
}

double WSoftImpulseDistribution::cdf(double x) const
{
	return WMath::softStep(x, m_steepness);
}


WMultivariateNormalDistribution::WMultivariateNormalDistribution(const WVec& mean, WMatrix* pCovariance)
: WDistribution()
{
	WAssert(pCovariance->rows() == (size_t)pCovariance->cols()); // pCovariance should be a square matrix
	m_nDims = pCovariance->rows();
	m_mean.resize(m_nDims);
	m_vector1.resize(m_nDims);
	m_vector2.resize(m_nDims);
	m_mean.copy(mean);
	precompute(pCovariance);
}

WMultivariateNormalDistribution::WMultivariateNormalDistribution(WMatrix* pData, size_t nDims)
{
	m_nDims = nDims;
	m_mean.resize(m_nDims);
	m_vector1.resize(m_nDims);
	m_vector2.resize(m_nDims);
	for(size_t i = 0; i < nDims; i++)
		m_mean[i] = pData->columnMean(i);
	WMatrix* pCov = pData->covarianceMatrix();
	std::unique_ptr<WMatrix> hCov(pCov);
	precompute(pCov);
}

WMultivariateNormalDistribution::~WMultivariateNormalDistribution()
{
	delete(m_pInverseCovariance);
	delete(m_pCholesky);
}

double WMultivariateNormalDistribution::likelihood(const WVec& x)
{
	m_vector1.copy(x);
	m_vector1 -= m_mean;
	m_pInverseCovariance->multiply(m_vector1 /*in*/, m_vector2 /*out*/, false);
	return m_dScale * exp(-0.5 * m_vector1.dotProduct(m_vector2));
}

void WMultivariateNormalDistribution::randomVector(WRand* pRand, WVec& out)
{
	for(size_t i = 0; i < m_nDims; i++)
		m_vector1[i] = pRand->normal();
	m_pCholesky->multiply(m_vector1, out, false);
	out += m_mean;
}

void WMultivariateNormalDistribution::precompute(WMatrix* pCovariance)
{
	m_dScale = 1.0 / sqrt(pow(2.0 * M_PI, (double)m_nDims) * pCovariance->determinant());

	//m_pInverseCovariance = pCovariance->clone();
	//m_pInverseCovariance->invert();
	m_pInverseCovariance->pseudoInverse();

	m_pCholesky = pCovariance->cholesky();
}

