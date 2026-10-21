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

#ifndef __GDISTRIBUTION_H__
#define __GDISTRIBUTION_H__

#include <stddef.h>
#include <math.h>
#include <map>
#include "WVec.h"
#include "WError.h"

namespace WClasses
{

class WDomNode;
class WDom;
class WRand;
class WMatrix;


class WDistribution
{
};

/// This is the base class for univariate distributions.
class WUnivariateDistribution : public WDistribution
{
public:
	enum Type
	{
		categorical,
		normal,
		uniform,
		poisson,
		gamma,
		inverseGamma,
		beta,
		softImpulse,
	};

	WUnivariateDistribution() {}
	virtual ~WUnivariateDistribution() {}

	/// Returns the type of distribution
	virtual Type type() const = 0;

	/// Returns true iff the space of supported values for this distribution is finite
	virtual bool isDiscrete() const = 0;

	/// Returns true iff the specified value is supported in this distribution
	virtual bool isSupported(double x) const = 0;

	/// Returns the most likely value
	virtual double mode() const = 0;

	/// Returns the likelihood of the specified value
	virtual double likelihood(double x) = 0;

	/// Returns the log-likelihood of the specified value
	virtual double logLikelihood(double x) = 0;
};


/// This is a distribution that specifies a probability for each
/// value in a set of nominal values.
class WCategoricalDistribution : public WUnivariateDistribution
{
protected:
	size_t m_nValueCount;
	size_t m_nMode;
	WVec m_pValues;

public:
	WCategoricalDistribution() : WUnivariateDistribution()
	{
		m_nValueCount = 0;
	}

	~WCategoricalDistribution() override
	{
	}

	/// Load values from a text format
	void deserialize(WDomNode* pNode);

	/// Save values to a text format
	WDomNode* serialize(WDom* pDoc) const;

	/// Returns categorical
	Type type() const override
	{
		return categorical;
	}

	/// Returns true
	bool isDiscrete() const override
	{
		return true;
	}

	/// Returns true if x is within the range of support. If x is
	/// not an integer, it will round x to the nearest integer and
	/// then return true if that value is supported.
	bool isSupported(double x) const override;

	/// Returns the mode
	double mode() const override
	{
		return (double)m_nMode;
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override;

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override
	{
		size_t index = (size_t)x;
		if(index >= m_nValueCount)
			throw Ex("out of range");
		return m_pValues[index];
	}

	/// Returns the number of supported values
	size_t valueCount()
	{
		return m_nValueCount;
	}

	/// Resizes the vector of probabilities if it
	/// does not have nValueCount elements, and returns
	/// that vector
	WVec& values(size_t nValueCount)
	{
		if(m_nValueCount != nValueCount)
		{
			m_pValues.resize(nValueCount);
			m_nValueCount = nValueCount;
		}
		return m_pValues;
	}

	/// Computes the entropy of the values, normalized to fall between 0 and 1
	double normalizedEntropy()
	{
		double dEntropy = 0;
		for(size_t i = 0; i < m_nValueCount; i++)
		{
			if(m_pValues[i] > 0)
				dEntropy -= m_pValues[i] * log(m_pValues[i]);
		}
		return dEntropy / log((double)m_nValueCount);
	}

	/// Makes the values sum to 1, and finds the mode
	void normalize();

	/// Safely converts from log space, and then normalizes
	void normalizeFromLogSpace();

	/// Sets the specified values, and normalizes
	void setValues(size_t nValueCount, const double* pValues);

	/// Sets the specified values, except infers the last, and normalizes
	void setValuesInferLast(size_t nValueCount, const double* pValues);

	/// Set all uniform probabilities
	void setToUniform(size_t nValues)
	{
		values(nValues);
		for(size_t i = 0; i < m_nValueCount; i++)
			m_pValues[i] = 1.0 / m_nValueCount;
		m_nMode = 0;
	}

	/// (1-d) is the probability of 0, and d is the probability of 1
	void setBoolean(double d)
	{
		double vals[2];
		vals[0] = 1.0 - d;
		vals[1] = d;
		setValues(2, vals);
	}

	/// This is a hack for when you know the mode but
	/// you don't know the other values.
	void setSpike(size_t nValueCount, size_t nValue, size_t nDepth);

	/// Returns the entropy of the values
	double entropy();
};


/// This class is for efficiently drawing random values from
/// a categorical distribution with a large number of categories.
class WCategoricalSampler
{
protected:
	std::map<double, size_t> m_map;

public:
	/// categories specifies the number of categories.
	/// pDistribution should specify a probability value for each
	/// category. They should sum to 1.
	WCategoricalSampler(size_t categories, const double* pDistribution);
	~WCategoricalSampler() {}

	/// d should be a random uniform value from 0 to 1. The corresponding zero-based
	/// category index is returned. This method will take log(categories) time.
	size_t draw(double d);
};


class WCategoricalSamplerBatch
{
protected:
	size_t m_categories;
	const WVec& m_distribution;
	WRandomIndexIterator m_ii;

public:
	/// categories specifies the number of categories.
	/// distribution should specify a probability value for each category. They should sum to 1.
	/// pDistribution is expected to remain valid for the duration of this object.
	WCategoricalSamplerBatch(size_t categories, const WVec& distribution, WRand& rand);
	~WCategoricalSamplerBatch();

	/// This will draw a batch of samples from the categorical distribution.
	/// This method is implemented efficiently, such that it will draw them in O(samples + categories) time.
	/// (This is significantly faster than O(samples * log(categories)) time, which is what you get if
	/// you use WCategoricalSampler.)
	void draw(size_t samples, size_t* pOutBatch);

	/// Run unit tests for this class.
	static void test();
};


/// This is the Normal (a.k.a. Gaussian) distribution
class WNormalDistribution : public WUnivariateDistribution
{
protected:
	double m_mean, m_variance;
	double m_height;

public:
	WNormalDistribution() : WUnivariateDistribution()
	{
		m_height = 0;
	}

	~WNormalDistribution() override
	{
	}

	Type type() const override
	{
		return normal;
	}

	/// Returns false
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true for all values
	bool isSupported(double x) const override
	{
		return true;
	}

	/// Returns the mode (which is also the mean)
	double mode() const override
	{
		return mean();
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override
	{
		return -0.5 * (log(m_variance) + (x - m_mean) * (x - m_mean) / m_variance);
	}

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override
	{
		if(m_height == 0)
			precompute();
		x -= m_mean;
		return m_height * exp(-0.5 * (x * x) / m_variance);
	}

	/// Sets the mean and variance of this distribution
	void setMeanAndVariance(double newmean, double newvariance)
	{
		WAssert(newmean > -1e308 && newvariance > -1e-5);
		m_mean = newmean;
		m_variance = newvariance;
		m_height = 0;
	}

	/// Returns the probability density (height) of the mode (mode=mean for a normal distribution)
	double modeLikelihood()
	{
		if(m_height == 0)
			precompute();
		return m_height;
	}

	/// Returns the mean
	double mean() const
	{
		return m_mean;
	}

	/// Returns the variance
	double variance() const
	{
		return m_variance;
	}

	/// Multiplies this by another Normal distribution
	void multiply(WNormalDistribution* pOther)
	{
		double b = m_variance / (m_variance + pOther->m_variance);
		double newMean = m_mean * (1.0 - b) + pOther->m_mean * b;
		m_variance = m_variance * pOther->m_variance / (m_variance + pOther->m_variance);
		m_mean = newMean;
		m_height = 0;
	}

protected:
	void precompute();
};


/// This is a continuous uniform distribution.
class WUniformDistribution : public WUnivariateDistribution
{
protected:
	double m_a, m_b;

public:
	WUniformDistribution() : WUnivariateDistribution()
	{
	}

	~WUniformDistribution() override
	{
	}

	Type type() const override
	{
		return uniform;
	}

	/// Returns false
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true iff a <= x < b
	bool isSupported(double x) const override
	{
		if(x >= m_a && x < m_b)
			return true;
		else
			return false;
	}

	/// Returns the middle value (which is also the mean and median)
	double mode() const override
	{
		return mean();
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override
	{
		return log(likelihood(x));
	}

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override
	{
		return 1.0 / (m_a - m_b);
	}

	/// Returns the middle value (which is also the median and one of the modes)
	double mean() const
	{
		return (m_a + m_b) / 2;
	}

	/// Set the parameters of this distribution
	void setParams(double a, double b)
	{
		m_a = a;
		m_b = b;
	}
};


/// The Poisson distribution
class WPoissonDistribution : public WUnivariateDistribution
{
protected:
	double m_rate;

public:
	WPoissonDistribution() : WUnivariateDistribution()
	{
	}

	~WPoissonDistribution() override
	{
	}

	Type type() const override
	{
		return poisson;
	}

	/// Returns true
	bool isDiscrete() const override
	{
		return true;
	}

	/// Returns true iff x rounds to a non-negative value
	bool isSupported(double x) const override
	{
		int n = (int)floor(x + 0.5);
		return (n >= 0);
	}

	/// Returns the mode (which is also the mean)
	double mode() const override
	{
		return mean();
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override;

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override;

	/// Sets the parameters of this distribution
	void setParams(double rate)
	{
		m_rate = rate;
	}

	/// Returns the mean
	double mean() const
	{
		return m_rate;
	}

	/// Returns the variance
	double variance() const
	{
		return m_rate;
	}
};


/// The Gamma distribution
class WGammaDistribution : public WUnivariateDistribution
{
protected:
	double m_shape, m_scale;

public:
	WGammaDistribution() : WUnivariateDistribution()
	{
	}

	~WGammaDistribution() override
	{
	}

	Type type() const override
	{
		return gamma;
	}

	/// Returns false
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true iff x is non-negative
	bool isSupported(double x) const override
	{
		return (x >= 0);
	}

	/// Returns the mode
	double mode() const override
	{
		if(m_shape >= 1)
			return (m_shape - 1) * m_scale;
		else
			return 0;
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override;

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override;

	/// Sets the parameters of this distribution
	void setParams(double shape, double scale)
	{
		m_shape = shape;
		m_scale = scale;
	}

	/// Returns the mean
	double mean() const
	{
		return m_shape * m_scale;
	}
};


/// The inverse Gamma distribution
class WInverseGammaDistribution : public WUnivariateDistribution
{
protected:
	double m_shape, m_scale;

public:
	WInverseGammaDistribution() : WUnivariateDistribution()
	{
	}

	~WInverseGammaDistribution() override
	{
	}

	Type type() const override
	{
		return inverseGamma;
	}

	/// Returns false
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true iff x is positive
	bool isSupported(double x) const override
	{
		return (x > 0);
	}

	/// Returns the mode
	double mode() const override
	{
		return m_scale / (m_shape + 1);
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override
	{
		return -(m_shape + 1.0) * log(x) - (m_scale / x);
	}

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override;

	/// Sets the parameters of this distribution
	void setParams(double shape, double scale)
	{
		m_shape = shape;
		m_scale = scale;
	}

	/// Returns the mean
	double mean() const
	{
		return m_scale / (m_shape - 1);
	}
};


/// The Beta distribution
class WBetaDistribution : public WUnivariateDistribution
{
protected:
	double m_alpha, m_beta;

public:
	WBetaDistribution() : WUnivariateDistribution()
	{
	}

	~WBetaDistribution() override
	{
	}

	Type type() const override
	{
		return beta;
	}

	/// Returns false
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true iff 0 <= x <= 1
	bool isSupported(double x) const override
	{
		return (x >= 0 && x <= 1);
	}

	/// Returns the mode
	double mode() const override
	{
		if(m_alpha > 1 && m_beta > 1)
			return (m_alpha - 1) / (m_alpha + m_beta - 2);
		else if(m_alpha >= m_beta)
			return 1;
		else
			return 0;
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double logLikelihood(double x) override;

	/// See the comment for WUnivariateDistribution::likelihood
	double likelihood(double x) override;

	/// Sets the parameters of this distribution
	void setParams(double newalpha, double newbeta)
	{
		m_alpha = newalpha;
		m_beta = newbeta;
	}

	/// Returns the mean
	double mean() const
	{
		return m_alpha / (m_alpha + m_beta);
	}
};



class WSoftImpulseDistribution : public WUnivariateDistribution
{
protected:
	double m_steepness;

public:
	WSoftImpulseDistribution() : WUnivariateDistribution()
	{
	}

	~WSoftImpulseDistribution() override
	{
	}

	Type type() const override
	{
		return softImpulse;
	}

	/// Returns true
	bool isDiscrete() const override
	{
		return false;
	}

	/// Returns true iff 0 <= x <= 1
	bool isSupported(double x) const override
	{
		return (x >= 0 && x <= 1);
	}

	/// Returns the mode
	double mode() const override
	{
		return 0.5;
	}

	/// See the comment for WUnivariateDistribution::logLikelihood
	double likelihood(double x) override;

	/// See the comment for WUnivariateDistribution::likelihood
	double logLikelihood(double x) override;

	/// Sets the parameter of this distribution
	void setParams(double steepness)
	{
		m_steepness = steepness;
	}

	/// Returns the mean
	double mean() const
	{
		return 0.5;
	}

	/// Returns the cumulative distribution of this distribution up to x
	double cdf(double x) const;
};


/// A multivariate Normal distribution. It can compute the likelihood of
/// a specified vector, and can also generate random vectors from the distribution.
class WMultivariateNormalDistribution : public WDistribution
{
protected:
	size_t m_nDims;
	double m_dScale;
	WVec m_mean;
	WVec m_vector1;
	WVec m_vector2;
	WMatrix* m_pInverseCovariance;
	WMatrix* m_pCholesky;

public:
	WMultivariateNormalDistribution(const WVec& mean, WMatrix* pCovariance);
	WMultivariateNormalDistribution(WMatrix* pData, size_t nDims);
	~WMultivariateNormalDistribution();

	/// Compute the likelihood of the specified vector (which is assumed
	/// to be the same size as the number of columns or rows in the covariance
	/// matrix).
	double likelihood(const WVec& x);

	/// Generates a random vector from this multivariate Normal distribution.
	void randomVector(WRand* pRand, WVec& out);

protected:
	void precompute(WMatrix* pCovariance);
};

} // namespace WClasses

#endif // __GDISTRIBUTION_H__
