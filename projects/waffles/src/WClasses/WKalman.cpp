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

#include "WKalman.h"
#include "WMatrix.h"
#include "WVec.h"
#include "WHolders.h"
#include <memory>

using namespace WClasses;

WExtendedKalmanFilter::WExtendedKalmanFilter(int stateDims, int observationDims, int controlDims)
: m_stateDims(stateDims), m_obsDims(observationDims)
{
	m_x.resize(stateDims);
	m_z.resize(observationDims);
	m_zz.resize(stateDims);
	m_x.fill(0.0);
	m_pP = new WMatrix(stateDims, stateDims);
	m_pP->makeIdentity();
	for(size_t i = 0; i < m_pP->rows(); i++)
		m_pP->row(i)[i] *= 1000.0;
}

WExtendedKalmanFilter::~WExtendedKalmanFilter()
{
	delete(m_pP);
}

void WExtendedKalmanFilter::advance(const WVec& control, WMatrix* pA)
{
	// Check values
	WAssert(pA->rows() == m_stateDims && pA->cols() == m_stateDims); // transition Jacobian wrong size

	// Compute uncorrected next estimated state
	transition(m_x, control);

	// Compute uncorrected next estimated covariance of state
	WMatrix* pTemp = WMatrix::multiply(*m_pP, *pA, false, true);
	std::unique_ptr<WMatrix> hTemp(pTemp);
	delete(m_pP);
	m_pP = WMatrix::multiply(*pA, *pTemp, false, false);
	addTransitionNoise(m_pP);
}

void WExtendedKalmanFilter::correct(const WVec& obs, WMatrix* pH)
{
	// Check values
	WAssert(pH->rows() == m_obsDims && pH->cols() == m_stateDims); // observation Jacobian wrong size

	// Compute the Kalman gain
	WMatrix* pK;
	{
		WMatrix* pTemp = WMatrix::multiply(*m_pP, *pH, false, true);
		std::unique_ptr<WMatrix> hTemp(pTemp);
		WMatrix* pTemp2 = WMatrix::multiply(*pH, *pTemp, false, false);
		std::unique_ptr<WMatrix> hTemp2(pTemp2);
		addObservationNoise(pTemp2);
		WMatrix* pTemp3 = pTemp2->pseudoInverse();
		std::unique_ptr<WMatrix> hTemp3(pTemp3);
		pK = WMatrix::multiply(*pTemp, *pTemp3, false, false);
	}
	std::unique_ptr<WMatrix> hK(pK);

	// Correct the estimated state
	observation(m_z, m_x);
	m_z *= -1.0;
	m_z += obs;
	pK->multiply(m_z, m_zz, false);
	m_x += m_zz;

	// Correct the estimated covariance of state
	{
		WMatrix* pTemp = WMatrix::multiply(*pK, *pH, false, false);
		std::unique_ptr<WMatrix> hTemp(pTemp);
		pTemp->multiply(-1.0);
		for(size_t i = 0; i < m_stateDims; i++)
			pTemp->row(i)[i] += 1.0;
		WMatrix* pTemp2 = WMatrix::multiply(*pTemp, *m_pP, false, false);
		delete(m_pP);
		m_pP = pTemp2;
	}
}
