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

#include "WBayesianNetwork.h"
#include "WRand.h"
#include "WError.h"
#include "WMath.h"
#include "WHolders.h"
#include "WMatrix.h"
#include <stddef.h>
#include <cmath>

using namespace WClasses;
using std::vector;


#define MIN_LOG_PROB -1e300


WBNNode::WBNNode()
: m_observed(false)
{
}

// virtual
WBNNode::~WBNNode()
{
}





void WBNSum::addParent(WBNNode* pNode)
{
	if(m_gotChildren)
		throw Ex("WBNSum nodes require all parents to be added before any children are added");
	m_parents.push_back(pNode);
}

// virtual
double WBNSum::currentValue()
{
	double sum = 0.0;
	for(vector<WBNNode*>::iterator it = m_parents.begin(); it != m_parents.end(); it++)
		sum += (*it)->currentValue();
	return sum;
}

// virtual
void WBNSum::onNewChild(WBNVariable* pChild)
{
	m_gotChildren = true;
	for(vector<WBNNode*>::iterator it = m_parents.begin(); it != m_parents.end(); it++)
		(*it)->onNewChild(pChild);
}




void WBNProduct::addParent(WBNNode* pNode)
{
	if(m_gotChildren)
		throw Ex("WBNProduct nodes require all parents to be added before any children are added");
	m_parents.push_back(pNode);
}

// virtual
double WBNProduct::currentValue()
{
	double prod = 1.0;
	for(vector<WBNNode*>::iterator it = m_parents.begin(); it != m_parents.end(); it++)
		prod *= (*it)->currentValue();
	return prod;
}

// virtual
void WBNProduct::onNewChild(WBNVariable* pChild)
{
	m_gotChildren = true;
	for(vector<WBNNode*>::iterator it = m_parents.begin(); it != m_parents.end(); it++)
		(*it)->onNewChild(pChild);
}





// virtual
double WBNMath::currentValue()
{
	double val = m_parent->currentValue();
	switch(m_op)
	{
		case NEGATE: return -val;
		case RECIPROCAL: return 1.0 / val;
		case SQUARE_ROOT: return sqrt(val);
		case SQUARE: return val * val;
		case LOG_E: return log(val);
		case EXP: return exp(val);
		case TANH: return tanh(val);
		case GAMMA: return WMath::gamma(val);
		case ABS: return std::abs(val);
		default:
			throw Ex("Unexpected operator");
	}
	return 0.0;
}

// virtual
void WBNMath::onNewChild(WBNVariable* pChild)
{
	m_parent->onNewChild(pChild);
}






WBNVariable::WBNVariable()
: WBNNode()
{
}

// virtual
WBNVariable::~WBNVariable()
{
}

// virtual
void WBNVariable::onNewChild(WBNVariable* pChild)
{
	m_children.push_back(pChild);
}

size_t WBNVariable::catCount()
{
	size_t cats = 1;
	for(vector<WBNCategorical*>::iterator it = m_catParents.begin(); it != m_catParents.end(); it++)
		cats *= (*it)->categories();
	return cats;
}

size_t WBNVariable::currentCatIndex()
{
	size_t mult = 1;
	size_t ind = 0;
	for(vector<WBNCategorical*>::iterator it = m_catParents.begin(); it != m_catParents.end(); it++)
	{
		WBNCategorical* pPar = *it;
		size_t val = (size_t)pPar->currentValue();
		ind += mult * val;
		if(pPar->categories() == 0)
			throw Ex("Categorical parent has no categories. Perhaps addWeights was never called as it ought to have been");
		mult *= pPar->categories();
	}
	WAssert(ind < mult);
	return ind;
}








WBNCategorical::WBNCategorical(size_t categors, WBNNode* pDefaultWeight)
: WBNVariable(), m_categories(categors), m_val(0)
{
	if(categors < 2)
		throw Ex("Expected at least 2 categories. Got ", to_str(categors));
	m_weights.resize(categors, pDefaultWeight);
}

void WBNCategorical::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultWeight)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_weights.resize(m_categories * catCount(), pDefaultWeight);
}

void WBNCategorical::setWeights(size_t cat, WBNNode* pW1, WBNNode* pW2, WBNNode* pW3, WBNNode* pW4, WBNNode* pW5, WBNNode* pW6, WBNNode* pW7, WBNNode* pW8)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = cat * m_categories;
	m_weights[base] = pW1;
	pW1->onNewChild(this);
	m_weights[base + 1] = pW2;
	pW2->onNewChild(this);
	size_t given = 2;
	if(pW3)
	{
		m_weights[base + 2] = pW3;
		pW3->onNewChild(this);
		given++;
		if(pW4)
		{
			m_weights[base + 3] = pW4;
			pW4->onNewChild(this);
			given++;
			if(pW5)
			{
				m_weights[base + 4] = pW5;
				pW5->onNewChild(this);
				given++;
				if(pW6)
				{
					m_weights[base + 5] = pW6;
					pW6->onNewChild(this);
					given++;
					if(pW7)
					{
						m_weights[base + 6] = pW7;
						pW7->onNewChild(this);
						given++;
						if(pW8)
						{
							m_weights[base + 7] = pW8;
							pW8->onNewChild(this);
							given++;
						}
					}
				}
			}
		}
	}
	if(given != m_categories)
		throw Ex("Expected ", to_str(m_categories), " sequential non-NULL nodes. Got ", to_str(given));
}

// virtual
double WBNCategorical::currentValue()
{
	if(m_observed)
		return m_observedValue;
	else
		return (double)m_val;
}

// virtual
void WBNCategorical::sample(WRand* pRand)
{
	if(m_observed)
		return;

	// Compute the sum Markov-blanket probability over each category
	size_t base = m_categories * currentCatIndex();
	double sumProb = 0.0;
	for(size_t i = 0; i < m_categories; i++)
	{
		double catProb = m_weights[base + i]->currentValue();
		for(vector<WBNVariable*>::const_iterator it = children().begin(); it != children().end(); it++)
		{
			WBNVariable* pChildNode = *it;
			size_t oldVal = m_val;
			m_val = i;
			catProb *= pChildNode->likelihood(pChildNode->currentValue());
			m_val = oldVal;
		}
		sumProb += catProb;
	}

	// Pick a category at random according to Markov-blanket probabilities
	double uni = pRand->uniform();
	double sumProb2 = 0.0;
	for(size_t i = 0; i < m_categories; i++)
	{
		double catProb = m_weights[base + i]->currentValue();
		for(vector<WBNVariable*>::const_iterator it = children().begin(); it != children().end(); it++)
		{
			WBNVariable* pChildNode = *it;
			size_t oldVal = m_val;
			m_val = i;
			catProb *= pChildNode->likelihood(pChildNode->currentValue());
			m_val = oldVal;
		}
		m_val = i;
		sumProb2 += catProb / sumProb;
		if(sumProb2 >= uni)
			break;
	}
}

// virtual
double WBNCategorical::likelihood(double x)
{
	size_t base = m_categories * currentCatIndex();
	double sumWeight = 0.0;
	for(size_t i = 0; i < m_categories; i++)
		sumWeight += m_weights[base + i]->currentValue();
	size_t xx = (size_t)x;
	WAssert(xx >= 0 && xx < m_categories);
	WBNNode* pX = m_weights[base + xx];
	double num = pX->currentValue();
	if(num > 0.0 && sumWeight > 0.0)
		return num / sumWeight;
	else
		return 0.0;
}







#define DEV_SAMPLES 100
#define DEV_DECAY ((double)(DEV_SAMPLES - 1) / DEV_SAMPLES)


WBNMetropolisNode::WBNMetropolisNode()
: WBNVariable(), m_currentMean(UNKNOWN_REAL_VALUE), m_sumOfValues(UNKNOWN_REAL_VALUE), m_sumOfSquaredValues(UNKNOWN_REAL_VALUE)
{
}

double WBNMetropolisNode::markovBlanket(double x)
{
	double d;
	double lik = likelihood(x);
	if(lik <= 0)
	{
		if(lik < 0)
			throw Ex("Negative likelihood?");
		return MIN_LOG_PROB;
	}
	double logSum = log(lik);
	if(logSum >= MIN_LOG_PROB)
	{
		const vector<WBNVariable*>& kids = children();
		for(vector<WBNVariable*>::const_iterator it = kids.begin(); it != kids.end(); it++)
		{
			WBNVariable* pChildNode = *it;
			double oldVal = m_currentMean;
			m_currentMean = x;
			double cv = pChildNode->currentValue();
			lik = pChildNode->likelihood(cv);
			if(lik <= 0)
			{
				if(lik < 0)
					throw Ex("Negative likelihood?");
				return MIN_LOG_PROB;
			}
			d = log(lik);
			m_currentMean = oldVal;
			if(d >= MIN_LOG_PROB)
				logSum += d;
			else
				return MIN_LOG_PROB;
		}
		return logSum;
	}
	else
		return MIN_LOG_PROB;
}

void WBNMetropolisNode::metropolis(WRand* pRand)
{
	currentValue(); // (This needs to be called to potentially initialize m_sumOfValues and m_sumOfSquaredValues)
	double sampleMean = m_sumOfValues / DEV_SAMPLES;
	double sampleVariance = ((double)DEV_SAMPLES / (DEV_SAMPLES - 1)) * (m_sumOfSquaredValues / DEV_SAMPLES - (sampleMean * sampleMean));
	double dCandidateValue = pRand->normal() * sqrt(sampleVariance) + m_currentMean;
	if(isDiscrete())
		dCandidateValue = floor(dCandidateValue + 0.5);
	double cand = markovBlanket(dCandidateValue);
	if(cand > MIN_LOG_PROB)
	{
		if(log(pRand->uniform()) < cand - markovBlanket(m_currentMean))
			m_currentMean = dCandidateValue;
	}
}

void WBNMetropolisNode::sample(WRand* pRand)
{
	if(m_observed)
		return;
	metropolis(pRand);
	m_sumOfValues *= DEV_DECAY;
	m_sumOfSquaredValues *= DEV_DECAY;
	m_sumOfValues += m_currentMean;
	m_sumOfSquaredValues += (m_currentMean * m_currentMean);
}

// virtual
double WBNMetropolisNode::currentValue()
{
	if(m_observed)
		return m_observedValue;
	else if(m_currentMean == UNKNOWN_REAL_VALUE)
	{
		m_currentMean = initMean();
		m_sumOfValues = DEV_SAMPLES * m_currentMean;
		m_sumOfSquaredValues = DEV_SAMPLES * (m_currentMean * m_currentMean + 1.0);
		return m_currentMean;
	}
	else
		return m_currentMean;
}







#define SQRT_2PI 2.50662827463

WBNNormal::WBNNormal(WBNNode* pDefaultVal)
: WBNMetropolisNode(), m_devIsVariance(false)
{
	m_meanAndDev.resize(2, pDefaultVal);
}

void WBNNormal::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_meanAndDev.resize(2 * catCount(), pDefaultVal);
}

void WBNNormal::setMeanAndDev(size_t cat, WBNNode* pMean, WBNNode* pDeviation)
{
	if(cat >= catCount())
		throw Ex("out of range");
	m_devIsVariance = false;
	size_t base = 2 * cat;
	m_meanAndDev[base] = pMean;
	pMean->onNewChild(this);
	m_meanAndDev[base + 1] = pDeviation;
	pDeviation->onNewChild(this);
}

void WBNNormal::setMeanAndVariance(size_t cat, WBNNode* pMean, WBNNode* pVariance)
{
	if(cat >= catCount())
		throw Ex("out of range");
	m_devIsVariance = true;
	size_t base = 2 * cat;
	m_meanAndDev[base] = pMean;
	pMean->onNewChild(this);
	m_meanAndDev[base + 1] = pVariance;
	pVariance->onNewChild(this);
}

// virtual
double WBNNormal::likelihood(double x)
{
	size_t base = 2 * currentCatIndex();
	double mean = m_meanAndDev[base]->currentValue();
	double dev = m_meanAndDev[base + 1]->currentValue();
	if(dev < 0)
		throw Ex("Negative deviation?");
	double t = x - mean;
	if(m_devIsVariance)
		return 1.0 / (sqrt(dev * 2.0 * M_PI)) * exp(-(t * t) / (2.0 * dev));
	else
		return 1.0 / (dev * SQRT_2PI) * exp(-(t * t) / (2.0 * dev * dev));
}

double WBNNormal::initMean()
{
	return m_meanAndDev[0]->currentValue();
}







WBNLogNormal::WBNLogNormal(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_meanAndDev.resize(2, pDefaultVal);
}

void WBNLogNormal::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_meanAndDev.resize(2 * catCount(), pDefaultVal);
}

void WBNLogNormal::setMeanAndDev(size_t cat, WBNNode* pMean, WBNNode* pDeviation)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_meanAndDev[base] = pMean;
	pMean->onNewChild(this);
	m_meanAndDev[base + 1] = pDeviation;
	pDeviation->onNewChild(this);
}

// virtual
double WBNLogNormal::likelihood(double x)
{
	size_t base = 2 * currentCatIndex();
	double mean = m_meanAndDev[base]->currentValue();
	double dev = m_meanAndDev[base + 1]->currentValue();
	double t = log(x - mean);
	return 1.0 / (x * dev * SQRT_2PI) * exp(-(t * t) / (2.0 * dev * dev));
}

double WBNLogNormal::initMean()
{
	return exp(m_meanAndDev[0]->currentValue());
}








WBNPareto::WBNPareto(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_alphaAndM.resize(2, pDefaultVal);
}

void WBNPareto::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_alphaAndM.resize(2 * catCount(), pDefaultVal);
}

void WBNPareto::setAlphaAndM(size_t cat, WBNNode* pAlpha, WBNNode* pM)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_alphaAndM[base] = pAlpha;
	pAlpha->onNewChild(this);
	m_alphaAndM[base + 1] = pM;
	pM->onNewChild(this);
}

// virtual
double WBNPareto::likelihood(double x)
{
	size_t base = 2 * currentCatIndex();
	double alpha = m_alphaAndM[base]->currentValue();
	double m = m_alphaAndM[base + 1]->currentValue();
	if(x < m)
		return 0;
	return alpha * pow(m, alpha) / pow(x, alpha + 1.0);
}

double WBNPareto::initMean()
{
	return m_alphaAndM[1]->currentValue();
}








WBNUniformDiscrete::WBNUniformDiscrete(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_minAndMax.resize(2, pDefaultVal);
}

void WBNUniformDiscrete::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_minAndMax.resize(2 * catCount(), pDefaultVal);
}

void WBNUniformDiscrete::setMinAndMax(size_t cat, WBNNode* pMin, WBNNode* pMax)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_minAndMax[base] = pMin;
	pMin->onNewChild(this);
	m_minAndMax[base + 1] = pMax;
	pMax->onNewChild(this);
}

// virtual
double WBNUniformDiscrete::likelihood(double x)
{
	size_t base = 2 * currentCatIndex();
	double a = std::ceil(m_minAndMax[base]->currentValue());
	double b = std::floor(m_minAndMax[base + 1]->currentValue());
	if(x < a)
		return 0.0;
	if(x > b)
		return 0.0;
	return 1.0 / (b - a + 1.0);
}

double WBNUniformDiscrete::initMean()
{
	return std::floor(0.5 * (m_minAndMax[0]->currentValue() + m_minAndMax[1]->currentValue()));
}








WBNUniformContinuous::WBNUniformContinuous(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_minAndMax.resize(2, pDefaultVal);
}

void WBNUniformContinuous::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_minAndMax.resize(2 * catCount(), pDefaultVal);
}

void WBNUniformContinuous::setMinAndMax(size_t cat, WBNNode* pMin, WBNNode* pMax)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_minAndMax[base] = pMin;
	pMin->onNewChild(this);
	m_minAndMax[base + 1] = pMax;
	pMax->onNewChild(this);
}

// virtual
double WBNUniformContinuous::likelihood(double x)
{
	size_t base = 2 * currentCatIndex();
	double a = m_minAndMax[base]->currentValue();
	double b = m_minAndMax[base + 1]->currentValue();
	if(x < a)
		return 0.0;
	if(x > b)
		return 0.0;
	return 1.0 / (b - a);
}

double WBNUniformContinuous::initMean()
{
	return 0.5 * (m_minAndMax[0]->currentValue() + m_minAndMax[1]->currentValue());
}








WBNPoisson::WBNPoisson(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_lambda.resize(1, pDefaultVal);
}

void WBNPoisson::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_lambda.resize(1 * catCount(), pDefaultVal);
}

void WBNPoisson::setLambda(size_t cat, WBNNode* pLambda)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 1 * cat;
	m_lambda[base] = pLambda;
	pLambda->onNewChild(this);
}

// virtual
double WBNPoisson::likelihood(double x)
{
	size_t base = 1 * currentCatIndex();
	double l = m_lambda[base]->currentValue();
	if(x < 0)
		return 0.0;
	return pow(l, x) * exp(-l) / WMath::gamma(x + 1);
}

double WBNPoisson::initMean()
{
	return m_lambda[0]->currentValue();
}








WBNExponential::WBNExponential(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_lambda.resize(1, pDefaultVal);
}

void WBNExponential::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_lambda.resize(1 * catCount(), pDefaultVal);
}

void WBNExponential::setLambda(size_t cat, WBNNode* pLambda)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 1 * cat;
	m_lambda[base] = pLambda;
	pLambda->onNewChild(this);
}

// virtual
double WBNExponential::likelihood(double x)
{
	size_t base = 1 * currentCatIndex();
	double l = m_lambda[base]->currentValue();
	if(x < 0)
		return 0.0;
	return l * exp(-l * x);
}

double WBNExponential::initMean()
{
	return 0.0;
}








WBNBeta::WBNBeta(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_alphaAndBeta.resize(2, pDefaultVal);
}

void WBNBeta::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_alphaAndBeta.resize(2 * catCount(), pDefaultVal);
}

void WBNBeta::setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_alphaAndBeta[base] = pAlpha;
	pAlpha->onNewChild(this);
	m_alphaAndBeta[base + 1] = pBeta;
	pBeta->onNewChild(this);
}

// virtual
double WBNBeta::likelihood(double x)
{
	if(x < 0.0 || x > 1.0)
		return 0.0;
	size_t base = 2 * currentCatIndex();
	double alpha = m_alphaAndBeta[base]->currentValue();
	double beta = m_alphaAndBeta[base + 1]->currentValue();
	double denom = WMath::gamma(alpha) * WMath::gamma(beta);
	if(std::abs(denom) < 1e-15)
		denom = (denom < 0 ? -1e-15 : 1e-15);
	return WMath::gamma(alpha + beta) / denom * pow(x, alpha - 1.0) * pow(1.0 - x, beta - 1.0);
}

double WBNBeta::initMean()
{
	double alpha = m_alphaAndBeta[0]->currentValue();
	double beta = m_alphaAndBeta[1]->currentValue();
	return std::max(0.0, std::min(1.0, alpha / std::max(0.01, alpha + beta)));
}








WBNGamma::WBNGamma(WBNNode* pDefaultVal)
: WBNMetropolisNode(), m_betaIsScaleInsteadOfRate(false)
{
	m_alphaAndBeta.resize(2, pDefaultVal);
}

void WBNGamma::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_alphaAndBeta.resize(2 * catCount(), pDefaultVal);
}

void WBNGamma::setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta)
{
	if(cat >= catCount())
		throw Ex("out of range");
	m_betaIsScaleInsteadOfRate = false;
	size_t base = 2 * cat;
	m_alphaAndBeta[base] = pAlpha;
	pAlpha->onNewChild(this);
	m_alphaAndBeta[base + 1] = pBeta;
	pBeta->onNewChild(this);
}

void WBNGamma::setShapeAndScale(size_t cat, WBNNode* pK, WBNNode* pTheta)
{
	if(cat >= catCount())
		throw Ex("out of range");
	m_betaIsScaleInsteadOfRate = true;
	size_t base = 2 * cat;
	m_alphaAndBeta[base] = pK;
	pK->onNewChild(this);
	m_alphaAndBeta[base + 1] = pTheta;
	pTheta->onNewChild(this);
}

// virtual
double WBNGamma::likelihood(double x)
{
	if(x < 0.0)
		return 0.0;
	size_t base = 2 * currentCatIndex();
	double alpha = m_alphaAndBeta[base]->currentValue();
	double beta = m_alphaAndBeta[base + 1]->currentValue();
	if(m_betaIsScaleInsteadOfRate)
		beta = 1.0 / beta;
	return pow(beta, alpha) * pow(x, alpha - 1.0) * exp(-beta * x) / WMath::gamma(alpha);
}

double WBNGamma::initMean()
{
	double alpha = m_alphaAndBeta[0]->currentValue();
	double beta = m_alphaAndBeta[1]->currentValue();
	if(m_betaIsScaleInsteadOfRate)
		beta = 1.0 / beta;
	return std::max(0.0, alpha / std::max(.01, beta));
}








WBNInverseGamma::WBNInverseGamma(WBNNode* pDefaultVal)
: WBNMetropolisNode()
{
	m_alphaAndBeta.resize(2, pDefaultVal);
}

void WBNInverseGamma::addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal)
{
	m_catParents.push_back(pNode);
	pNode->onNewChild(this);
	m_alphaAndBeta.resize(2 * catCount(), pDefaultVal);
}

void WBNInverseGamma::setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta)
{
	if(cat >= catCount())
		throw Ex("out of range");
	size_t base = 2 * cat;
	m_alphaAndBeta[base] = pAlpha;
	pAlpha->onNewChild(this);
	m_alphaAndBeta[base + 1] = pBeta;
	pBeta->onNewChild(this);
}

// virtual
double WBNInverseGamma::likelihood(double x)
{
	if(x < 0.0)
		return 0.0;
	size_t base = 2 * currentCatIndex();
	double alpha = m_alphaAndBeta[base]->currentValue();
	double beta = m_alphaAndBeta[base + 1]->currentValue();
	return pow(beta, alpha) / WMath::gamma(alpha) * pow(x, -alpha - 1.0) * exp(-beta / x);
}

double WBNInverseGamma::initMean()
{
	double alpha = m_alphaAndBeta[0]->currentValue();
	double beta = m_alphaAndBeta[1]->currentValue();
	double mean = std::max(0.0, beta / (std::max(0.5, alpha - 1.0)));
	if(mean < 0.0)
		throw Ex("Only positive values are supported");
	return mean;
}







WBayesNet::WBayesNet(size_t seed)
: m_heap(2048), m_rand(seed)
{
	m_pConstOne = newConst(1.0);
}

WBayesNet::~WBayesNet()
{
	for(size_t i = 0; i < m_nodes.size(); i++)
		m_nodes[i]->~WBNNode();
}

WBNConstant* WBayesNet::newConst(double val)
{
	char* pDest = m_heap.allocAligned(sizeof(WBNConstant));
	return new(pDest) WBNConstant(val);
}

WBNSum* WBayesNet::newSum()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNSum));
	return new(pDest) WBNSum();
}

WBNProduct* WBayesNet::newProduct()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNProduct));
	WBNProduct* pNode = new(pDest) WBNProduct();
	m_nodes.push_back(pNode);
	return pNode;
}

WBNMath* WBayesNet::newMath(WBNNode* pParent, WBNMath::math_op operation)
{
	char* pDest = m_heap.allocAligned(sizeof(WBNMath));
	WBNMath* pNode = new(pDest) WBNMath(pParent, operation);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNCategorical* WBayesNet::newCat(size_t categories)
{
	char* pDest = m_heap.allocAligned(sizeof(WBNCategorical));
	WBNCategorical* pNode = new(pDest) WBNCategorical(categories, m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNNormal* WBayesNet::newNormal()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNNormal));
	WBNNormal* pNode = new(pDest) WBNNormal(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNLogNormal* WBayesNet::newLogNormal()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNLogNormal));
	WBNLogNormal* pNode = new(pDest) WBNLogNormal(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNPareto* WBayesNet::newPareto()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNPareto));
	WBNPareto* pNode = new(pDest) WBNPareto(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNUniformDiscrete* WBayesNet::newUniformDiscrete()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNUniformDiscrete));
	WBNUniformDiscrete* pNode = new(pDest) WBNUniformDiscrete(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNUniformContinuous* WBayesNet::newUniformContinuous()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNUniformContinuous));
	WBNUniformContinuous* pNode = new(pDest) WBNUniformContinuous(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNPoisson* WBayesNet::newPoisson()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNPoisson));
	WBNPoisson* pNode = new(pDest) WBNPoisson(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNExponential* WBayesNet::newExponential()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNExponential));
	WBNExponential* pNode = new(pDest) WBNExponential(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNBeta* WBayesNet::newBeta()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNBeta));
	WBNBeta* pNode = new(pDest) WBNBeta(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNGamma* WBayesNet::newGamma()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNGamma));
	WBNGamma* pNode = new(pDest) WBNGamma(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

WBNInverseGamma* WBayesNet::newInverseGamma()
{
	char* pDest = m_heap.allocAligned(sizeof(WBNInverseGamma));
	WBNInverseGamma* pNode = new(pDest) WBNInverseGamma(m_pConstOne);
	m_sampleNodes.push_back(pNode);
	m_nodes.push_back(pNode);
	return pNode;
}

void WBayesNet::sample()
{
	// Shuffle the order of the nodes
	for(size_t i = m_sampleNodes.size(); i > 1; i--)
		std::swap(m_sampleNodes[i - 1], m_sampleNodes[(size_t)m_rand.next(i)]);

	// Sample each node
	for(vector<WBNVariable*>::iterator it = m_sampleNodes.begin(); it != m_sampleNodes.end(); it++)
		(*it)->sample(&m_rand);
}

void WBayesNet_simpleTest()
{
	WBayesNet bn;
	WBNCategorical* pPar = bn.newCat(2);
	pPar->setWeights(0, bn.newConst(0.4), bn.newConst(0.6));

	WBNNormal* pChild = bn.newNormal();
	pChild->addCatParent(pPar, bn.def());
	pChild->setMeanAndDev(0, bn.newConst(0.0), bn.newConst(1.0));
	pChild->setMeanAndDev(1, bn.newConst(3.0), bn.newConst(2.0));

	pChild->setObserved(1.0);

	for(size_t burnin = 0; burnin < 10000; burnin++)
		bn.sample();
	size_t parCount = 0;
	size_t sampleCount = 0;
	for(size_t sample = 0; sample < 50000; sample++)
	{
		bn.sample();
		if(pPar->currentValue() == 0.0)
			parCount++;
		sampleCount++;
	}

	if(std::abs((double)parCount / sampleCount - 0.5714286) > 0.003)
		throw Ex("Not close enough");
}

void WBayesNet_threeTest()
{
	WBayesNet bn;
	WBNCategorical* pA = bn.newCat(2);
	pA->setWeights(0, bn.newConst(2.0 / 5.0), bn.newConst(3.0 / 5.0));

	WBNCategorical* pB = bn.newCat(2);
	pB->addCatParent(pA, bn.def());
	pB->setWeights(0, bn.newConst(2.0 / 3.0), bn.newConst(1.0 / 3.0));
	pB->setWeights(1, bn.newConst(3.0 / 7.0), bn.newConst(4.0 / 7.0));

	WBNCategorical* pC = bn.newCat(2);
	pC->addCatParent(pB, bn.def());
	pC->setWeights(0, bn.newConst(1.0 / 2.0), bn.newConst(1.0 / 2.0));
	pC->setWeights(1, bn.newConst(1.0 / 3.0), bn.newConst(2.0 / 3.0));

	pA->setObserved(0.0);
	pC->setObserved(0.0);

	for(size_t burnin = 0; burnin < 10000; burnin++)
		bn.sample();
	size_t bCount = 0;
	size_t sampleCount = 0;
	for(size_t sample = 0; sample < 50000; sample++)
	{
		bn.sample();
		if(pB->currentValue() == 0.0)
			bCount++;
		sampleCount++;
	}

	if(std::abs((double)bCount / sampleCount - 0.75) > 0.005)
		throw Ex("Not close enough");
}

void WBayesNet_alarmTest()
{
	// This example is given in Russell and Norvig page 504. (See also http://www.d.umn.edu/~rmaclin/cs8751/Notes/chapter14a.pdf)
	WBayesNet bn;
	WBNCategorical* pBurglary = bn.newCat(2);
	pBurglary->setWeights(0, bn.newConst(0.001), bn.newConst(0.999));

	WBNCategorical* pEarthquake = bn.newCat(2);
	pEarthquake->setWeights(0, bn.newConst(0.002), bn.newConst(0.998));

	WBNCategorical* pAlarm = bn.newCat(2);
	pAlarm->addCatParent(pBurglary, bn.def());
	pAlarm->addCatParent(pEarthquake, bn.def());
	pAlarm->setWeights(0, bn.newConst(0.95), bn.newConst(0.05));
	pAlarm->setWeights(1, bn.newConst(0.29), bn.newConst(0.71));
	pAlarm->setWeights(2, bn.newConst(0.94), bn.newConst(0.06));
	pAlarm->setWeights(3, bn.newConst(0.001), bn.newConst(0.999));

	WBNCategorical* pJohnCalls = bn.newCat(2);
	pJohnCalls->addCatParent(pAlarm, bn.def());
	pJohnCalls->setWeights(0, bn.newConst(0.9), bn.newConst(0.1));
	pJohnCalls->setWeights(1, bn.newConst(0.05), bn.newConst(0.95));

	WBNCategorical* pMaryCalls = bn.newCat(2);
	pMaryCalls->addCatParent(pAlarm, bn.def());
	pMaryCalls->setWeights(0, bn.newConst(0.7), bn.newConst(0.3));
	pMaryCalls->setWeights(1, bn.newConst(0.01), bn.newConst(0.99));

	pJohnCalls->setObserved(0.0);
	pMaryCalls->setObserved(0.0);

	WRand rand(0);
	for(size_t burnin = 0; burnin < 10000; burnin++)
		bn.sample();
	size_t sampleCount = 0;
	vector<double> probs;
	probs.resize(3);
	for(size_t sample = 0; sample < 50000; sample++)
	{
		bn.sample();
		sampleCount++;
		probs[0] *= (1.0 - 1.0 / sampleCount);
		if(pBurglary->currentValue() == 0.0)
			probs[0] += (1.0 / sampleCount);
		probs[1] *= (1.0 - 1.0 / sampleCount);
		if(pEarthquake->currentValue() == 0.0)
			probs[1] += (1.0 / sampleCount);
		probs[2] *= (1.0 - 1.0 / sampleCount);
		if(pAlarm->currentValue() == 0.0)
			probs[2] += (1.0 / sampleCount);
	}

	if(std::abs(probs[0] - 0.284) > 0.005)
		throw Ex("Not close enough");
	if(std::abs(probs[1] - 0.176) > 0.005)
		throw Ex("Not close enough");
	if(std::abs(probs[2] - 0.761) > 0.005)
		throw Ex("Not close enough");
}

void WBayesNet::test()
{
	WBayesNet_simpleTest();
	WBayesNet_threeTest();
	WBayesNet_alarmTest();
}

