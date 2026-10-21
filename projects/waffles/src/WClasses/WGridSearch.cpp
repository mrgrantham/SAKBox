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

#include "WGridSearch.h"
#include <math.h>
#include <stdio.h>
#include "WVec.h"
#include "WRand.h"

using std::vector;

namespace WClasses
{

WGridSearch::WGridSearch(WTargetFunction* pCritic)
: WOptimizer(pCritic), m_pCandidate(pCritic->relation()->size()), m_pBestVector(pCritic->relation()->size())
{
	if(!pCritic->relation()->areContinuous(0, pCritic->relation()->size()))
		throw Ex("Discrete attributes are not supported");
	vector<size_t> ranges;
	ranges.resize(pCritic->relation()->size());
	for(size_t i = 0; i < (size_t)pCritic->relation()->size(); i++)
		ranges[i] = 0x4000001;
	m_pCvi = new WCoordVectorIterator(ranges);
}

// virtual
WGridSearch::~WGridSearch()
{
	delete(m_pCvi);
}

// virtual
double WGridSearch::iterate()
{
	size_t* pCur = m_pCvi->current();
	for(size_t i = 0; i < (size_t)m_pCritic->relation()->size(); i++)
		m_pCandidate[i] = (double)*(pCur++) / 0x4000001;
	double err = m_pCritic->computeError(m_pCandidate);
	if(err < m_bestError)
	{
		m_bestError = err;
		m_pCandidate.swapContents(m_pBestVector);
	}
	m_pCvi->advanceSampling();
	return m_bestError;
}

// virtual
const WVec& WGridSearch::currentVector()
{
	return m_pBestVector;
}









WRandomSearch::WRandomSearch(WTargetFunction* pCritic, WRand* pRand)
: WOptimizer(pCritic), m_pRand(pRand), m_pCandidate(pCritic->relation()->size()), m_pBestVector(pCritic->relation()->size())
{
	if(!pCritic->relation()->areContinuous(0, pCritic->relation()->size()))
		throw Ex("Discrete attributes are not supported");
}

// virtual
WRandomSearch::~WRandomSearch()
{
}

// virtual
double WRandomSearch::iterate()
{
	m_pCandidate.fillUniform(*m_pRand);
	double err = m_pCritic->computeError(m_pCandidate);
	if(err < m_bestError)
	{
		m_bestError = err;
		m_pCandidate.swapContents(m_pBestVector);
	}
	return m_bestError;
}

// virtual
const WVec& WRandomSearch::currentVector()
{
	return m_pBestVector;
}






WMinBinSearch::WMinBinSearch(WTargetFunction* pCritic)
: WOptimizer(pCritic), m_curDim(0), m_stepSize(0.25), m_pCurrent(m_pCritic->relation()->size())
{
	if(!pCritic->relation()->areContinuous(0, pCritic->relation()->size()))
		throw Ex("Discrete attributes are not supported");
	m_pCurrent.fill(0.5);
	m_curErr = m_pCritic->computeError(m_pCurrent);
}

// virtual
WMinBinSearch::~WMinBinSearch()
{
}

// virtual
double WMinBinSearch::iterate()
{
	m_pCurrent[m_curDim] += m_stepSize;
	double d = m_pCritic->computeError(m_pCurrent);
	if(d < m_curErr)
		m_curErr = d;
	else
	{
		m_pCurrent[m_curDim] -= 2.0 * m_stepSize;
		d = m_pCritic->computeError(m_pCurrent);
		if(d < m_curErr)
			m_curErr = d;
		else
			m_pCurrent[m_curDim] += m_stepSize;
	}
	if(++m_curDim >= m_pCritic->relation()->size())
	{
		m_curDim = 0;
		m_stepSize *= 0.65;
	}
	return m_curErr;
}







WProbeSearch::WProbeSearch(WTargetFunction* pCritic)
: WOptimizer(pCritic), m_rand(0), m_pMins(pCritic->relation()->size()), m_pMaxs(pCritic->relation()->size()), m_pVector(pCritic->relation()->size()), m_pBestYet(pCritic->relation()->size())
{
	if(!pCritic->relation()->areContinuous(0, pCritic->relation()->size()))
		throw Ex("Discrete attributes are not supported");
	m_nDimensions = pCritic->relation()->size();
	m_nStabDepth = m_nDimensions * 30;
	m_samples = 64;
	reset();
}

/*virtual*/ WProbeSearch::~WProbeSearch()
{
}

void WProbeSearch::reset()
{
	m_nMask[0] = 0;
	m_nMask[1] = 0;
	m_nMask[2] = 0;
	m_nMask[3] = 0;
	resetStab();
	m_nMask[0] = 0; // undo the increment that ResetStab() does
	m_nStabs = 0;
	m_bestError = 1e308;
}

void WProbeSearch::resetStab()
{
	m_nCurrentDim = 0;
	m_nDepth = 0;

	// Start at the global scope
	m_pMins.fill(0.0);
	m_pMaxs.fill(1.0);

	// Increment the mask
	size_t i = 0;
	while(++(m_nMask[i]) == 0)
		i++;
	m_nStabs++;
}

double WProbeSearch::sample(bool greater)
{
	double bestLocal = 1e300;
	m_rand.setSeed(0);
	for(size_t i = 0; i < m_samples; i++)
	{
		m_pVector.fillUniform(m_rand);
		for(size_t j = 0; j < m_nDimensions; j++)
		{
			m_pVector[j] *= (m_pMaxs[j] - m_pMins[j]);
			m_pVector[j] += m_pMins[j];
		}
		m_pVector[m_nCurrentDim] -= m_pMins[m_nCurrentDim];
		m_pVector[m_nCurrentDim] *= 0.5;
		m_pVector[m_nCurrentDim] += m_pMins[m_nCurrentDim];
		if(greater)
			m_pVector[m_nCurrentDim] += 0.5 * (m_pMaxs[m_nCurrentDim] - m_pMins[m_nCurrentDim]);
		double err = m_pCritic->computeError(m_pVector);
		bestLocal = std::min(bestLocal, err);
		if(err < m_bestError)
		{
			m_bestError = err;
			m_pBestYet.copy(m_pVector);
		}
	}
	return bestLocal;
}

/*virtual*/ double WProbeSearch::iterate()
{
	// Test the center of both halves
	double dError1 = sample(false);
	double dError2 = sample(true);

	// Zoom in on half of the search space
	if(m_nMask[std::min(m_nDepth, (size_t)127) / 32] & ((size_t)1 << (std::min(m_nDepth, (size_t)127) % 32))) // if the mask bit is non-zero
	{
		// Pick the worse half
		if(dError1 < dError2)
			m_pMins[m_nCurrentDim] = 0.5 * (m_pMins[m_nCurrentDim] + m_pMaxs[m_nCurrentDim]);
		else
			m_pMaxs[m_nCurrentDim] = 0.5 * (m_pMins[m_nCurrentDim] + m_pMaxs[m_nCurrentDim]);
	}
	else
	{
		// Pick the better half
		if(dError1 < dError2)
			m_pMaxs[m_nCurrentDim] = 0.5 * (m_pMins[m_nCurrentDim] + m_pMaxs[m_nCurrentDim]);
		else
			m_pMins[m_nCurrentDim] = 0.5 * (m_pMins[m_nCurrentDim] + m_pMaxs[m_nCurrentDim]);
	}

	// Advance
	if(++m_nCurrentDim >= m_nDimensions)
		m_nCurrentDim = 0;
	if(++m_nDepth > m_nStabDepth)
		resetStab();
	return m_bestError;
}


class WProbeSearchTestCritic : public WTargetFunction
{
public:
	WVec m_target;

	WProbeSearchTestCritic() : WTargetFunction(3), m_target(3)
	{
		m_target[0] = 0.7314;
		m_target[1] = 0.1833;
		m_target[2] = 0.3831;
	}

	~WProbeSearchTestCritic() override
	{
	}

	bool isStable() override { return true; }
	virtual bool isConstrained() { return false; }

protected:
	void initVector(WVec& pVector) override
	{
	}

	double computeError(const WVec& pVector) override
	{
		if(pVector[0] < 0.5)
			return 0.001;
		else if(pVector[1] >= 0.5)
			return 0.002;
		else
			return pVector.squaredDistance(m_target);
	}
};

// static
void WProbeSearch::test()
{
	WProbeSearchTestCritic critic;
	WProbeSearch search(&critic);
	size_t stabdepth = 30;
	search.setStabDepth(stabdepth);
	size_t i;
	for(i = 0; i < stabdepth * 3 * 4; i++) // 3 = number of dims, 4 = number of stabs that should find it
		search.iterate();
	double err = search.currentVector().squaredDistance(critic.m_target);
	if(err >= 1e-3)
		throw Ex("failed");
}

} // namespace WClasses

