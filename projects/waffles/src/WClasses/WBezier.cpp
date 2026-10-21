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

#include "WBezier.h"
//#include "W3D.h"
#include "WError.h"
#include "WHolders.h"
#include "WRand.h"
#include <stdlib.h>
#include <memory>

using namespace WClasses;

namespace WClasses
{
struct WBezierPoint
{
	W3DVector point;
	double weight;

	inline void Multiply(double dMag)
	{
		point.multiply(dMag);
		weight *= dMag;
	}

	inline void Add(WBezierPoint* pPoint)
	{
		point.add(pPoint->point);
		weight += pPoint->weight;
	}
};
}

WBezier::WBezier(int nControlPoints)
{
	m_nControlPoints = nControlPoints;
	m_pPoints = new WBezierPoint[nControlPoints];
}

WBezier::WBezier(WBezier* pThat)
{
	m_nControlPoints = pThat->m_nControlPoints;
	m_pPoints = new WBezierPoint[m_nControlPoints];
	W3DVector pnt;
	double weight;
	int n;
	for(n = 0; n < m_nControlPoints; n++)
	{
		pThat->controlPoint(&pnt, &weight, n);
		setControlPoint(n, &pnt, weight);
	}
}

WBezier::~WBezier()
{
	delete[] m_pPoints;
}

WBezier* WBezier::copy()
{
	WBezier* pNewBezier = new WBezier(m_nControlPoints);
	W3DVector pnt;
	double weight;
	int n;
	for(n = 0; n < m_nControlPoints; n++)
	{
		controlPoint(&pnt, &weight, n);
		pNewBezier->setControlPoint(n, &pnt, weight);
	}
	return pNewBezier;
}

// Determine the point on the rational Bezier curve using the Horner algorithm
void WBezier::point(double t, W3DVector* pOutPoint)
{
	int n = m_nControlPoints - 1;
	if(t == 1.0)
	{
		// Special case because the calcualtion for "u" below can't handle the case where t is 1
		*pOutPoint = m_pPoints[n].point;
		return;
	}
	double fw = m_pPoints[n].weight;
	pOutPoint->m_vals[0] = m_pPoints[n].point.m_vals[0] * fw;
	pOutPoint->m_vals[1] = m_pPoints[n].point.m_vals[1] * fw;
	pOutPoint->m_vals[2] = m_pPoints[n].point.m_vals[2] * fw;
	double u = t / (1.0 - t); // t/(1-t) in "monomialization trick"
	double bc = 1.0; // binomial coefficient
	[[maybe_unused]] double den = 1.0; // denominator in P(t)/(1-t)^n
	double aux; // auxiliary variable - efficiency

	int i;
	for(i = n - 1; i >= 0; i--)
	{
		bc = (bc * (i + 1)) / (n - i); // new binomial coefficient
		aux = bc * m_pPoints[i].weight;
		pOutPoint->m_vals[0] = pOutPoint->m_vals[0] * u + m_pPoints[i].point.m_vals[0] * aux;
		pOutPoint->m_vals[1] = pOutPoint->m_vals[1] * u + m_pPoints[i].point.m_vals[1] * aux;
		pOutPoint->m_vals[2] = pOutPoint->m_vals[2] * u + m_pPoints[i].point.m_vals[2] * aux;
		fw = fw * u + aux;
		den *= (1.0 - t);
	}

	pOutPoint->m_vals[0] /= fw;
	pOutPoint->m_vals[1] /= fw;
	pOutPoint->m_vals[2] /= fw;
}

int WBezier::controlPointCount()
{
	return m_nControlPoints;
}

void WBezier::controlPoint(W3DVector* pOutPoint, double* pOutWeight, int n)
{
	WAssert(n >= 0 && n <= m_nControlPoints); // out of range
	*pOutPoint = m_pPoints[n].point;
	*pOutWeight = m_pPoints[n].weight;
}

void WBezier::setControlPoint(int n, W3DVector* pPoint, double weight)
{
	WAssert(n >= 0 && n <= m_nControlPoints); // out of range
	m_pPoints[n].point = *pPoint;
	m_pPoints[n].weight = weight;
}

void WBezier::elevateDegree()
{
	// Allocate the new control points and set the first and last one
	WBezierPoint* pNewControlPoints = new WBezierPoint[m_nControlPoints + 1];
	pNewControlPoints[0] = m_pPoints[0];
	pNewControlPoints[m_nControlPoints] = m_pPoints[m_nControlPoints - 1];

	// Convert from weighted (rational) 3D control points to integral (non-rational) 4D control points
	int n;
	for(n = 0; n < m_nControlPoints; n++)
		m_pPoints[n].point.multiply(m_pPoints[n].weight);

	// Determine the new intermediate control points
	struct WBezierPoint p1;
	struct WBezierPoint p2;
	for(n = 1; n < m_nControlPoints; n++)
	{
		// Find the new 4D control point
		double dFac = (double)n / (m_nControlPoints);
		p1 = m_pPoints[n - 1];
		p1.Multiply(dFac);
		p2 = m_pPoints[n];
		p2.Multiply(1.0 - dFac);
		pNewControlPoints[n] = p1;
		pNewControlPoints[n].Add(&p2);

		// Convert back to a weighted (rational) 3D point
		pNewControlPoints[n].point.multiply(1.0 / pNewControlPoints[n].weight);
	}

	// Swap in the new control points
	delete[] m_pPoints;
	m_pPoints = pNewControlPoints;
	m_nControlPoints++;
}

// Returns a 2D triangular array of 4D control points determined by the deCasteljau algorithm
struct WBezierPoint* WBezier::deCasteljau(double t, bool bTail)
{
	// Convert from weighted (rational) 3D control points to integral (non-rational) 4D control points
	int n;
	for(n = 0; n < m_nControlPoints; n++)
		m_pPoints[n].point.multiply(m_pPoints[n].weight);

	// Populate the biggest row with the current control points
	WBezierPoint* pDeCasteljauPoints = (WBezierPoint*)alloca(sizeof(WBezierPoint) * m_nControlPoints * (m_nControlPoints + 1) / 2);
	int y = m_nControlPoints - 1;
	n = (y * (y + 1)) / 2;
	int x;
	for(x = 0; x < m_nControlPoints; x++)
		pDeCasteljauPoints[n + x] = m_pPoints[x];

	// Calculate the rest of the points
	struct WBezierPoint p1;
	struct WBezierPoint p2;
	for(y--; y >= 0; y--)
	{
		n = (y * (y + 1)) / 2;
		for(x = 0; x <= y; x++)
		{
			p1 = pDeCasteljauPoints[n + x + y + 1];
			p2 = pDeCasteljauPoints[n + x + y + 2];
			p1.Multiply(1.0 - t);
			p2.Multiply(t);
			pDeCasteljauPoints[n + x] = p1;
			pDeCasteljauPoints[n + x].Add(&p2);
		}
	}

	// Extract the segment control points and convert back to weighted (rational) 3D points
	WBezierPoint* pSegment = new WBezierPoint[m_nControlPoints];
	for(y = 0; y < m_nControlPoints; y++)
	{
		if(bTail)
			n = (y * (y + 1)) / 2 + y;
		else
			n = ((m_nControlPoints - 1 - y) * (m_nControlPoints - y)) / 2;
		pSegment[y] = pDeCasteljauPoints[n];
		pSegment[y].point.multiply(1 / pSegment[y].weight);
	}
	return pSegment;
}

void WBezier::segment(double t, bool bTail)
{
	WBezierPoint* pNewControlPoints = deCasteljau(t, bTail);
	delete[] m_pPoints;
	m_pPoints = pNewControlPoints;
}

void WBezier::derivativeAtZero(W3DVector* pOutPoint)
{
	WAssert(m_nControlPoints > 1); // undefined with only one point
	*pOutPoint = m_pPoints[1].point;
	pOutPoint->subtract(m_pPoints[0].point);
	double d = m_pPoints[1].weight * (m_nControlPoints - 1) / m_pPoints[0].weight;
	pOutPoint->multiply(d);
}

double WBezier::curvatureAtZero()
{
	if(m_nControlPoints < 3)
		return 0;

	W3DVector vec1 = m_pPoints[1].point;
	vec1.subtract(m_pPoints[0].point);
	double aSquared = vec1.squaredMag();
	vec1.multiply(1.0 / sqrt(aSquared));

	W3DVector vec2 = m_pPoints[2].point;
	vec2.subtract(m_pPoints[1].point);
	double dSquared = vec2.squaredMag();
	vec2.dotProduct(vec1);
	double h = sqrt(dSquared - vec2.squaredMag());

	return m_pPoints[0].weight * m_pPoints[2].weight / (m_pPoints[1].weight * m_pPoints[1].weight) * h / aSquared * (m_nControlPoints - 2) / (m_nControlPoints - 1);
}

double DoPolarEquation(int n, double* pPolarCoords, int nPolarCoords)
{
	WAssert(nPolarCoords >= n); // nPolarCoords must be bigger than n
	int* pPermutations = (int*)alloca(n * sizeof(int));
	int i;
	for(i = 0; i < n; i++)
		pPermutations[i] = n - 1 - i;
	int pos;
	double dSum = 0;
	int nPermutations = 0;
	while(true)
	{
		nPermutations++;
		double dTemp = 1;
		for(i = 0; i < n; i++)
		{
			WAssert(pPermutations[i] < nPolarCoords); // internal error
			dTemp *= pPolarCoords[pPermutations[i]];
		}
		dSum += dTemp;
		for(pos = 0; pos < n && pPermutations[pos] >= nPolarCoords - 1 - pos; pos++)
		{
		}
		if(pos >= n)
			break;
		pPermutations[pos]++;
		for(; pos > 0; pos--)
			pPermutations[pos - 1] = pPermutations[pos] + 1;
	}
	return dSum / nPermutations;
}

void CalculateBernsteinCoefficient(W3DVector* pOutPoint, double* pPolarCoords, W3DVector* pPolyCoeff, int nPolyCoeff)
{
	pOutPoint->m_vals[0] = 0;
	pOutPoint->m_vals[1] = 0;
	pOutPoint->m_vals[2] = 0;
	double tmp;
	int n;
	for(n = 0; n < nPolyCoeff; n++)
	{
		tmp = DoPolarEquation(n, pPolarCoords, nPolyCoeff - 1);
		pOutPoint->m_vals[0] += pPolyCoeff[n].m_vals[0] * tmp;
		pOutPoint->m_vals[1] += pPolyCoeff[n].m_vals[1] * tmp;
		pOutPoint->m_vals[2] += pPolyCoeff[n].m_vals[2] * tmp;
	}
}

/*static*/ WBezier* WBezier::fromPolynomial(W3DVector* pCoefficients, int nCoefficients)
{
	double* pPolarCoords = (double*)alloca((nCoefficients - 1) * sizeof(double));
	int n;
	for(n = 0; n < nCoefficients - 1; n++)
		pPolarCoords[n] = 0;
	WBezier* pNewBezier = new WBezier(nCoefficients);
	W3DVector point;
	CalculateBernsteinCoefficient(&point, pPolarCoords, pCoefficients, nCoefficients);
	pNewBezier->setControlPoint(0, &point, 1);
	for(n = 0; n < nCoefficients - 1; n++)
	{
		pPolarCoords[n] = 1;
		CalculateBernsteinCoefficient(&point, pPolarCoords, pCoefficients, nCoefficients);
		pNewBezier->setControlPoint(n + 1, &point, 1);
	}
	return pNewBezier;
}

void WBezier::toPolynomial(W3DVector* pOutCoefficients)
{
	int n;
	for(n = 0; n < m_nControlPoints; n++)
	{
		pOutCoefficients[n] = m_pPoints[n].point;
		WAssert(m_pPoints[n].weight == m_pPoints[0].weight); // Rational (weighted) Bezier curves can't be represented as a polynomial--pretending all weights are the same
	}
	int i;
	for(i = 1; i < m_nControlPoints; i++)
	{
		for(n = m_nControlPoints - 1; n >= i; n--)
			pOutCoefficients[n].subtract(pOutCoefficients[n - 1]);
	}
	WTEMPBUF(size_t, pPascalsTriangle, m_nControlPoints);
	WMath::pascalsTriangle(pPascalsTriangle, m_nControlPoints - 1);
	for(n = 0; n < m_nControlPoints; n++)
		pOutCoefficients[n].multiply((double)pPascalsTriangle[n]);
}

// static
void WBezier::test()
{
	// Generate a random Bezier curve with seven control points
	WRand prng(0);
	W3DVector p1, p2, p3;
	WBezier* pOrig = new WBezier(7);
	std::unique_ptr<WBezier> hOrig(pOrig);
	int n;
	for(n = 0; n < pOrig->controlPointCount(); n++)
	{
		p1.m_vals[0] = (W3DReal)prng.next(1000000);
		p1.m_vals[1] = (W3DReal)prng.next(1000000);
		p1.m_vals[2] = (W3DReal)prng.next(1000000);
		pOrig->setControlPoint(n, &p1, 1);
	}

	// Test that elevating the degree doesn't change the curve
	WBezier* pCopy = pOrig->copy();
	std::unique_ptr<WBezier> hCopy(pCopy);
	pCopy->elevateDegree();
	if(pCopy->controlPointCount() != pOrig->controlPointCount() + 1)
		throw Ex("wrong answer");
	for(n = 0; n < 10; n++)
	{
		pOrig->point((double)n / 10, &p1);
		pCopy->point((double)n / 10, &p2);
		if(p1.squaredDist(p2) > .00001)
			throw Ex("wrong answer");
	}

	// Test that we can segment the curve properly
	WBezier* pCopy2 = pCopy->copy();
	std::unique_ptr<WBezier> hCopy2(pCopy2);
	pCopy->segment(.3, false);
	pCopy2->segment(.3, true);
	for(n = 0; n < 10; n++)
	{
		pOrig->point((double)n / 10, &p1);
		pCopy->point(((double)n / 10) / .3, &p2);
		pCopy2->point(((double)n / 10) / .7 - .3 / .7, &p3);
		if(p1.squaredDist(p2) > .00001)
			throw Ex("wrong answer");
		if(p1.squaredDist(p3) > .00001)
			throw Ex("wrong answer");
	}

	// Test that it round-trips through a polynomial
	W3DVector coeff[7];
	pOrig->toPolynomial(coeff);
	WBezier* pAnother = WBezier::fromPolynomial(coeff, 7);
	std::unique_ptr<WBezier> hAnother(pAnother);
	for(n = 0; n < 10; n++)
	{
		pOrig->point((double)n / 10, &p1);
		pAnother->point((double)n / 10, &p2);
		if(p1.squaredDist(p2) > .00001)
			throw Ex("wrong answer");
	}
}

// ---------------------------------------------------------------------------------------------------

namespace WClasses
{
struct WNurbsPoint
{
	W3DVector point;
	double weight;
	double knotInterval;
};
}

WNurbs::WNurbs(int nControlPoints, int nDegree, bool periodic)
{
	WAssert(m_nControlPoints > m_nDegree); // You must have more control points than the degree
	m_nControlPoints = nControlPoints;
	m_nDegree = nDegree;
	m_pPoints = new WNurbsPoint[nControlPoints];
	m_bPeriodic = periodic;
}

WNurbs::WNurbs(WNurbs* pThat)
{
	m_pPoints = new WNurbsPoint[pThat->m_nControlPoints];
	m_nDegree = pThat->m_nDegree;
	m_nControlPoints = pThat->m_nControlPoints;
	int n;
	for(n = 0; n < m_nControlPoints; n++)
		m_pPoints[n] = pThat->m_pPoints[n];
	m_bPeriodic = pThat->m_bPeriodic;
}

WNurbs::~WNurbs()
{
	delete[] m_pPoints;
}

void WNurbs::controlPoint(W3DVector* pOutPoint, double* pOutWeight, int n)
{
	*pOutPoint = m_pPoints[n].point;
	*pOutWeight = m_pPoints[n].weight;
}

void WNurbs::setControlPoint(int n, W3DVector* pPoint, double weight)
{
	m_pPoints[n].point = *pPoint;
	m_pPoints[n].weight = weight;
}

double WNurbs::knotInterval(int n)
{
	return m_pPoints[n].knotInterval;
}

void WNurbs::setKnotInterval(int n, double dInterval)
{
	m_pPoints[n].knotInterval = dInterval;
}

// This maps both positive and negative numbers to a positive modulus of n
inline int WrapAround(int i, int n)
{
	return ((i % n) + n) % n;
}

void WNurbs::pointCircular(struct WBezierPoint* pOutPoint, int n)
{
	int nControlPoint = WrapAround(n, m_nControlPoints);
	pOutPoint->point = m_pPoints[nControlPoint].point;
	pOutPoint->weight = m_pPoints[nControlPoint].weight;
}

double WNurbs::knotValue(int n)
{
	if(m_bPeriodic)
	{
		if(n < 0)
		{
			double dSum = 0;
			int i;
			for(i = -1; i >= n; i--)
			{
				int nKnotIndex = WrapAround(i, m_nControlPoints);
				dSum -= m_pPoints[nKnotIndex].knotInterval;
			}
			return dSum;
		}
		else if(n > 0)
		{
			double dSum = 0;
			int i;
			for(i = 0; i < n; i++)
				dSum += m_pPoints[i % m_nControlPoints].knotInterval;
			return dSum;
		}
		else
			return 0;
	}
	else
	{
		if(n <= 0)
			return 0;
		if(n > m_nControlPoints)
			n = m_nControlPoints;
		double dSum = 0;
		int i;
		for(i = 0; i < n; i++)
			dSum += m_pPoints[i].knotInterval;
		return dSum;
	}
}

void MixPoints(struct WBezierPoint* pA, const struct WBezierPoint* pB, double dA, double dTarget, double dB)
{
	struct WBezierPoint temp = *pB;
	temp.Multiply(dTarget - dA);
	pA->Multiply(dB - dTarget);
	pA->Add(&temp);
	pA->Multiply(1.0 / (dB - dA));
}

void WNurbs::calculateBezierControlPoint(struct WBezierPoint* pOutPoint, int nInterval, int nBezierControlPoint)
{
	WAssert(nBezierControlPoint < m_nDegree - 1); // This method won't get the last control point in the Bezier curve.  Use the first control point of the next interval instead (since they're the same point).
	struct WBezierPoint* pPoints = (struct WBezierPoint*)alloca(m_nDegree * sizeof(struct WBezierPoint));
	int nStartPoint = nInterval - (m_nDegree >> 1);
	int n;
	for(n = 0; n < m_nDegree; n++)
		pointCircular(&pPoints[n], nStartPoint + n);
	int nStartKnotPos = nStartPoint - (m_nDegree >> 1);
	int nTargetKnotPos = nInterval;
	int nEndKnotPos = nStartKnotPos + m_nDegree;
	for(n = m_nDegree - 1; n > 0; n--)
	{
		if(nBezierControlPoint >= n)
			nTargetKnotPos = nInterval + 1;
		double dTargetKnot = knotValue(nTargetKnotPos);
		int i;
		for(i = 0; i < n; i++)
		{
			double dAKnot = knotValue(nStartKnotPos + i);
			double dBKnot = knotValue(nEndKnotPos + i);
			WAssert(dAKnot <= dTargetKnot && dTargetKnot <= dBKnot);
			MixPoints(&pPoints[i], &pPoints[i + 1], dAKnot, dTargetKnot, dBKnot);
		}
		nStartKnotPos++;
	}
	*pOutPoint = pPoints[0];
}


WBezier* WNurbs::bezier(int nInterval)
{
	WBezier* pBez = new WBezier(m_nDegree + 1);
	struct WBezierPoint point;
	int n;
	for(n = 0; n < m_nDegree; n++)
	{
		calculateBezierControlPoint(&point, nInterval, n);
		pBez->setControlPoint(n, &point.point, point.weight);
	}
	calculateBezierControlPoint(&point, (nInterval + 1) % m_nControlPoints, 0);
	pBez->setControlPoint(m_nDegree, &point.point, point.weight);
	return pBez;
}

// This will mess up the value at pB and will return the answer in pA
void WNurbs::newKnotPeriodic(struct WBezierPoint* pA, struct WBezierPoint* pB, int n, int nControlPoint, double dRatio)
{
	double dSum = 0;
	double dMin = 0;
	double dMax = 0;
	int i;
	nControlPoint -= ((m_nDegree - 1) >> 1);
	for(i = 0; i < m_nDegree; i++)
	{
		if(i == m_nDegree - 1 - n)
			dMin = dSum;
		dSum += knotInterval(WrapAround(nControlPoint + i, m_nControlPoints));
		if(i == m_nDegree - 1 - n)
			dMax = dSum;
	}
	dMin /= dSum;
	dMax /= dSum;
	dRatio = dMin * (1.0 - dRatio) + dMax * dRatio;
	pA->Multiply(1.0 - dRatio);
	pB->Multiply(dRatio);
	pA->Add(pB);
}

void WNurbs::insertKnotPeriodic(int nInterval, double dRatio)
{
	if(!m_bPeriodic)
		throw Ex("This function is not implemented for non-periodic NURBS");
	struct WNurbsPoint* pNewPoints = new struct WNurbsPoint[m_nControlPoints + 1];
	int nStart = nInterval - (m_nDegree >> 1);
	struct WBezierPoint a;
	struct WBezierPoint b;
	double dNewKnotInterval;
	int n;
	for(n = 0; n < m_nDegree; n++)
	{
		int nControlPoint = WrapAround(n + nStart, m_nControlPoints);
		int nControlPoint2 = WrapAround(n + nStart + 1, m_nControlPoints);
		controlPoint(&a.point, &a.weight, nControlPoint);
		controlPoint(&b.point, &b.weight, nControlPoint2);
		newKnotPeriodic(&a, &b, n, nControlPoint, dRatio);
		if(n < (m_nDegree >> 1) - 1)
			dNewKnotInterval = knotInterval(nControlPoint2);
		else if(n > (m_nDegree >> 1))
			dNewKnotInterval = knotInterval(nControlPoint);
		else if(n == (m_nDegree >> 1) - 1)
			dNewKnotInterval = dRatio * knotInterval(nControlPoint2);
		else
			dNewKnotInterval = (1.0 - dRatio) * knotInterval(nControlPoint2);
		pNewPoints[n].point = a.point;
		pNewPoints[n].weight = a.weight;
		pNewPoints[n].knotInterval = dNewKnotInterval;
	}
	for(; n < m_nControlPoints + 1; n++)
		pNewPoints[n] = m_pPoints[WrapAround(nStart + n, m_nControlPoints)];
	delete[] m_pPoints;
	m_pPoints = pNewPoints;
	m_nControlPoints++;
}

