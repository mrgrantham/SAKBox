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

#include "WManifold.h"
#include <stdio.h>
#include <math.h>
#include "WBits.h"
#include "WBitTable.h"
#include "WGraph.h"
#include "WHillClimber.h"
#include "WHeap.h"
#include "WImage.h"
#include "WKNN.h"
#include "WLinear.h"
#include "WMath.h"
#include "WNeighborFinder.h"
#include "WNeuralNet.h"
#include "WPlot.h"
#include "WSparseMatrix.h"
#include "WTime.h"
#include "WTransform.h"
#include "WDom.h"
#include "WVec.h"
#include "WHolders.h"
#include <deque>
#include <set>
#include <map>
#include <iostream>
#include <string>
#include <queue>
#include <memory>

namespace WClasses
{

using std::string;
using std::cout;
using std::vector;
using std::deque;
using std::set;
using std::map;
using std::make_pair;


#define USE_ANGLES
#define STEP_SIZE_PER_POINT

void WVec_copy(double* pDest, const double* pSource, size_t nDims)
{
	memcpy(pDest, pSource, sizeof(double) * nDims);
}

void WVec_add(double* pDest, const double* pSource, size_t nDims)
{
	for(size_t i = 0; i < nDims; i++)
	{
		*pDest += *pSource;
		pDest++;
		pSource++;
	}
}

void WVec_multiply(double* pVector, double dScalar, size_t nDims)
{
	for(size_t i = 0; i < nDims; i++)
	{
		*pVector *= dScalar;
		pVector++;
	}
}

void WVec_setAll(double* pVector, double value, size_t dims)
{
	for(size_t i = 0; i < dims; i++)
	{
		*pVector = value;
		pVector++;
	}
}

// static
void WManifold::computeNeighborWeights(const WMatrix* pData, size_t point, size_t k, const size_t* pNeighbors, double* pOutWeights)
{
	// Create a matrix of all the neighbors normalized around the origin
	size_t colCount = pData->cols();
	WMatrix z(pData->relation().clone());
	const WVec& row = pData->row(point);
	for(size_t i = 0; i < k; i++)
	{
		if(*pNeighbors < pData->rows())
		{
			WVec& target = z.newRow();
			target.copy(pData->row(*pNeighbors));
			target -= row;
			pNeighbors++;
		}
		else
		{
			WVec& target = z.newRow();
			target.fill(1e12);
		}
	}

	// Square it
	WMatrix* pSquare = WMatrix::multiply(z, z, false, true);
	std::unique_ptr<WMatrix> hSquare(pSquare);

	// if the number of neighbors is more than the number of dimensions then the
	// square matrix will not be full rank so we need to regularize it
	if(pSquare->rows() > (size_t)colCount)
	{
		double dReg = pSquare->trace() * 0.001;
		for(size_t i = 0; i < pSquare->rows(); i++)
			pSquare->row(i)[i] += dReg;
	}

	// Compute the weights the direct way (fails for non-invertible matrices)
	// 	for(size_t i = 0; i < pSquare->rows(); i++)
	// 		pOutWeights[i] = 1;
	// 	if(!pSquare->gaussianElimination(pOutWeights))
	// 		throw Ex("Failed to find a solution in computeNeighborWeights");

	// Compute the weights the SVD way
	WMatrix* pInv = pSquare->pseudoInverse();
	std::unique_ptr<WMatrix> hInv(pInv);
	for(size_t i = 0; i < pSquare->rows(); i++)
		pOutWeights[i] = pInv->row(i).sum();

	// Normalize the weights to sum to one
	WVecWrapper vw(pOutWeights, pSquare->rows());
	vw.sumToOne();
}

// static
WMatrix* WManifold::blendNeighborhoods(size_t index, WMatrix* pA, double ratio, WMatrix* pB, size_t neighborCount, WNeighborFinder* pNF)
{
	// Copy the two neighborhoods
	size_t colCount = pA->cols();
	WMatrix neighborhoodA(pA->relation().clone());
	neighborhoodA.newRow().copy(pA->row(index));
	WMatrix neighborhoodB(pB->relation().clone());
	neighborhoodB.newRow().copy(pB->row(index));
	for(size_t j = 0; j < neighborCount; j++)
	{
		neighborhoodA.newRow().copy(pA->row(pNF->neighbor(j)));
		neighborhoodB.newRow().copy(pB->row(pNF->neighbor(j)));
	}

	// Subtract the means
	WVec mean(colCount);
	neighborhoodA.centroid(mean);
	for(size_t i = 0; i < neighborhoodA.rows(); i++)
		neighborhoodA.row(i) -= mean;
	neighborhoodB.centroid(mean);
	for(size_t i = 0; i < neighborhoodB.rows(); i++)
		neighborhoodB.row(i) -= mean;

	// Use the kabsch algorithm to compute the optimal rotation
	WMatrix* pKabsch = WMatrix::kabsch(&neighborhoodA, &neighborhoodB);
	std::unique_ptr<WMatrix> hKabsch(pKabsch);
	WMatrix* pC = WMatrix::multiply(neighborhoodB, *pKabsch, false, false);
	for(size_t i = 0; i < pC->rows(); i++)
	{
		pC->row(i) *= ratio;
		pC->row(i).addScaled(1.0 - ratio, neighborhoodA.row(i));
		// todo: should we normalize the distance here?
	}
	return pC;
}

// static
WMatrix* WManifold::blendEmbeddings(WMatrix* pA, double* pRatios, WMatrix* pB, size_t neighborCount, WNeighborGraph* pNeighborTable, size_t seed)
{
	// Check params
	size_t rowCount = pA->rows();
	size_t colCount = pA->cols();
	if(pB->rows() != rowCount || pB->cols() != colCount)
		throw Ex("mismatching sizes");

	// Blend the seed neighborhood
	WMatrix* pC = new WMatrix(rowCount, colCount);
	deque<size_t> q;
	WBitTable visited(rowCount);
	WBitTable established(rowCount);
	{
		size_t nc = pNeighborTable->findNearest(neighborCount, seed);
		WMatrix* pAve = blendNeighborhoods(seed, pA, pRatios[seed], pB, neighborCount, pNeighborTable);
		std::unique_ptr<WMatrix> hAve(pAve);
		pC->row(seed).copy(pAve->row(0));
		visited.set(seed);
		established.set(seed);
		size_t i = 1;
		for(size_t j = 0; j < nc; j++)
		{
			size_t neigh = pNeighborTable->neighbor(j);
			pC->row(neigh).copy(pAve->row(i));
			visited.set(neigh);
			q.push_back(neigh);
			i++;
		}
	}

	// Align in a breadth-first manner
	WVec mean(colCount);
	while(q.size() > 0)
	{
		size_t par = q.front();
		q.pop_front();

		// Make a blended neighborhood
		size_t nc = pNeighborTable->findNearest(neighborCount, par);
		WMatrix* pD = blendNeighborhoods(par, pA, pRatios[par], pB, neighborCount, pNeighborTable);
		std::unique_ptr<WMatrix> hD(pD);

		// Make sub-neighborhoods that contain only tentatively-placed points
		WMatrix tentativeC(pC->relation().clone());
		WMatrix tentativeD(pD->relation().clone());
		WReleaseDataHolder hTentativeD(&tentativeD);
		tentativeC.newRow().copy(pC->row(par));
		tentativeD.takeRow(&pD->row(0));
		size_t ii = 1;
		for(size_t j = 0; j < nc; j++)
		{
			if(visited.bit(pNeighborTable->neighbor(j)))
			{
				tentativeC.newRow().copy(pC->row(pNeighborTable->neighbor(j)));
				tentativeD.takeRow(&pD->row(ii));
			}
			ii++;
		}

		// Subtract the means
		tentativeD.centroid(mean);
		for(size_t i = 0; i < pD->rows(); i++)
			pD->row(i) -= mean; // (This will affect tentativeD too b/c it refs the same rows)
		tentativeC.centroid(mean);
		for(size_t i = 0; i < tentativeC.rows(); i++)
			tentativeC.row(i) -= mean;

		// Compute the rotation to align the tentative neighborhoods
		WMatrix* pKabsch = WMatrix::kabsch(&tentativeC, &tentativeD);
		std::unique_ptr<WMatrix> hKabsch(pKabsch);

		// Compute an aligned version of pD that fits with pC
		WMatrix* pAligned = WMatrix::multiply(*pD, *pKabsch, false, false);
		std::unique_ptr<WMatrix> hAligned(pAligned);
		for(size_t i = 0; i < pAligned->rows(); i++)
			pAligned->row(i) += mean;

		// Accept the new points
		pC->row(par).copy(pAligned->row(0));
		established.set(par);
		ii = 1;
		for(size_t j = 0; j < nc; j++)
		{
			if(!established.bit(pNeighborTable->neighbor(j)))
				pC->row(pNeighborTable->neighbor(j)).copy(pAligned->row(ii));
			if(!visited.bit(pNeighborTable->neighbor(j)))
			{
				visited.set(pNeighborTable->neighbor(j));
				q.push_back(pNeighborTable->neighbor(j));
			}
			ii++;
		}
	}
	return pC;
}

// static
WMatrix* WManifold::multiDimensionalScaling(WMatrix* pDistances, size_t targetDims, WRand* pRand, bool useSquaredDistances)
{
	size_t n = pDistances->rows();
	if((size_t)pDistances->cols() != n)
		throw Ex("Expected a square and symmetric distance matrix");

	// Square every element in the distance matrix (unless it's already squared) and ensure symmetry
	WMatrix* pD = new WMatrix(pDistances->relation().clone());
	std::unique_ptr<WMatrix> hD(pD);
	pD->newRows(n);
	for(size_t i = 0; i < n; i++)
	{
		WVec& in = pDistances->row(i);
		WVec& out = pD->row(i);
		out[i] = 0.0;
		if(useSquaredDistances)
		{
			for(size_t j = i + 1; j < n; j++)
				out[j] = in[j];
		}
		else
		{
			for(size_t j = i + 1; j < n; j++)
				out[j] = (in[j] * in[j]);
		}
	}
	pD->mirrorTriangle(true);

	// Some algebra
	WVec rowsum(n);
	WVec pEigenVals(targetDims);
	for(size_t i = 0; i < n; i++)
		rowsum[i] = pD->row(i).sum();
	double z = 1.0 / n;
	double t = z * rowsum.sum();
	for(size_t i = 0; i < n; i++)
	{
		WVec& row = pD->row(i);
		for(size_t j = 0; j < n; j++)
			row[j] = -0.5 * (row[j] + (z * (t - rowsum[i] - rowsum[j])));
	}

	// Compute eigenvectors
	WMatrix* pEigs = pD->eigs(std::min(n, targetDims), pEigenVals, pRand, true);
	if(n < (size_t)targetDims)
	{
		throw Ex("targetDims cannot be larger than the number of rows or columns in the distance matrix");
		/*
		for(size_t i = n; i < targetDims; i++)
			pEigenVals[i] = 0.0;
		WMatrix* pEigs2 = new WMatrix(targetDims);
		pEigs2->newRows(targetDims);
		pEigs2->setAll(0.0);
		for(size_t y = 0; y < n; y++)
			WVec::copy(pEigs2->row(y), pEigs->row(y), n);
		delete(pEigs);
		pEigs = pEigs2;
*/
	}
	std::unique_ptr<WMatrix> hEigs(pEigs);
	for(size_t i = 0; i < targetDims; i++)
		pEigs->row(i) *= sqrt(std::max(0.0, pEigenVals[i]));
	WMatrix* pResults = pEigs->transpose();
	return pResults;
}

#define POINT_COUNT 9
void WManifold_testMultiDimensionalScaling()
{
	// Test MDS
	WRand prng(0);
	WMatrix foo(POINT_COUNT, 2);
	for(size_t i = 0; i < POINT_COUNT; i++)
		foo[i].fillUniform(prng);

	// Make distance matrix
	WMatrix dst(POINT_COUNT, POINT_COUNT);
	for(size_t i = 0; i < POINT_COUNT; i++)
	{
		WVec& row = dst.row(i);
		for(size_t j = i + 1; j < POINT_COUNT; j++)
			row[j] = foo.row(i).squaredDistance(foo.row(j));
	}

	// Do MDS
	WMatrix* pMDS = WManifold::multiDimensionalScaling(&dst, 2, &prng, true);
	std::unique_ptr<WMatrix> hMDS(pMDS);
	for(size_t i = 0; i < POINT_COUNT; i++)
	{
		for(size_t j = 0; j < POINT_COUNT; j++)
		{
			double expected = sqrt(foo.row(i).squaredDistance(foo.row(j)));
			double actual = sqrt(pMDS->row(i).squaredDistance(pMDS->row(j)));
			if(std::abs(expected - actual) > 1e-5)
				throw Ex("failed");
		}
	}
}

// static
void WManifold::test()
{
	WManifold_testMultiDimensionalScaling();
}







struct WManifoldSculptingNeighbor
{
	size_t m_nNeighbor;
	size_t m_nNeighborsNeighborSlot;
	double m_dCosTheta;
	double m_dDistance;
	double m_junkSquaredDist;
	double m_junkDotProd;
};

struct WManifoldSculptingStuff
{
	size_t m_nCycle;
	bool m_bAdjustable;
};

WManifoldSculpting::WManifoldSculpting(size_t nNeighbors, size_t targetDims, WRand* pRand)
: m_pRand(pRand), m_pNF(nullptr)
{
	m_pMetaData = nullptr;
	m_nDimensions = 0;
	m_nTargetDims = targetDims;
	m_pRelationAfter = new WUniformRelation(m_nTargetDims, 0);
	m_nNeighbors = nNeighbors;

	m_nStuffIndex = sizeof(struct WManifoldSculptingNeighbor) * m_nNeighbors;
	m_nRecordSize = m_nStuffIndex + sizeof(struct WManifoldSculptingStuff);

	m_dSquishingRate = .99;
	m_nPass = 0;
	m_scale = 1.0;
	m_dAveNeighborDist = 0;
	m_pData = nullptr;
	m_minNeighborDist = 0;
	m_maxNeighborDist = 1e20;
}

WManifoldSculpting::~WManifoldSculpting()
{
	delete(m_pData);
	delete[] m_pMetaData;
}

void WManifoldSculpting::setPreprocessedData(WMatrix* pData)
{
	delete(m_pData);
	m_pData = pData;
}

// virtual
WMatrix* WManifoldSculpting::reduce(const WMatrix& in)
{
	beginTransform(&in);

	// Do burn-in iterations
	while(m_scale > 0.01)
		squishPass((size_t)m_pRand->next(m_pData->rows()));

	// Squish until it doesn't improve for a while
	double dBestError = 1e308;
	for(size_t nItersSinceImprovement = 0; nItersSinceImprovement < 50; nItersSinceImprovement++)
	{
		//PlotData(4.0f);
		double dError = squishPass((size_t)m_pRand->next(m_pData->rows()));
		if(dError < dBestError)
		{
			dBestError = dError;
			nItersSinceImprovement = 0;
		}
	}

	// Produce the output data
	WMatrix* pDataOut = new WMatrix(relationAfter().clone());
	pDataOut->newRows(m_pData->rows());
	pDataOut->copyCols(*m_pData, 0, m_nTargetDims);
	delete(m_pData);
	m_pData = nullptr;
	return pDataOut;
}

double WManifoldSculpting_dotProduct(const double* pOriginA, const double* pTargetA, const double* pOriginB, const double* pTargetB, size_t nSize)
{
	double dVal = 0;
	for(size_t n = 0; n < nSize; n++)
	{
		dVal += (*pTargetA - *pOriginA) * (*pTargetB - *pOriginB);
		pTargetA++;
		pOriginA++;
		pTargetB++;
		pOriginB++;
	}
	return dVal;
}

double WMS_sqDist(const double* pA, const double* pB, size_t nDims)
{
	double dist = 0;
	double d;
	for(size_t n = 0; n < nDims; n++)
	{
		d = (*pA) - (*pB);
		dist += (d * d);
		pA++;
		pB++;
	}
	return dist;
}

void WManifoldSculpting::beginTransform(const WMatrix* pRealSpaceData)
{
	m_nDimensions = pRealSpaceData->cols();
	if(!pRealSpaceData->relation().areContinuous(0, m_nDimensions))
		throw Ex("Only continuous values are supported");

	// Calculate metadata
	calculateMetadata(pRealSpaceData);

	// Preprocess the data
	if(m_pData)
	{
		// Check the supplied pre-processed data
		if(m_pData->rows() != pRealSpaceData->rows())
			throw Ex("Preprocessed data has wrong number of points");
		if(m_pData->relation().size() < (size_t)m_nTargetDims)
			throw Ex("Preprocessed data has too few dimensions");
	}
	else
	{
		// Preprocess the data
		size_t preserveDims = m_nTargetDims * 6;
		preserveDims = std::max((size_t)30, preserveDims);
		preserveDims = std::min(pRealSpaceData->relation().size(), preserveDims);
		WPCA pca(preserveDims);
		pca.train(*pRealSpaceData);
		m_pData = pca.transformBatch(*pRealSpaceData);
	}

	// Calculate the junk
	m_q.clear();
	m_nDimensions = m_pData->relation().size();
	m_nPass = 0;
	m_scale = 1.0;
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		struct WManifoldSculptingNeighbor* pArrNeighbors = record(i);
		for(size_t j = 0; j < m_nNeighbors; j++)
		{
			size_t neighbor = pArrNeighbors[j].m_nNeighbor;
			if(neighbor < m_pData->rows())
			{
				pArrNeighbors[j].m_junkSquaredDist = WMS_sqDist(m_pData->row(i).data() + m_nTargetDims, m_pData->row(neighbor).data() + m_nTargetDims, m_nDimensions - m_nTargetDims);
				size_t slot = pArrNeighbors[j].m_nNeighborsNeighborSlot;
				struct WManifoldSculptingNeighbor* pArrNeighborsNeighbors = record(neighbor);
				size_t neighborsNeighbor = pArrNeighborsNeighbors[slot].m_nNeighbor;
				pArrNeighbors[j].m_junkDotProd = WManifoldSculpting_dotProduct(m_pData->row(neighbor).data() + m_nTargetDims, m_pData->row(i).data() + m_nTargetDims, m_pData->row(neighbor).data() + m_nTargetDims, m_pData->row(neighborsNeighbor).data() + m_nTargetDims, m_nDimensions - m_nTargetDims);
			}
		}
	}
}

void WManifoldSculpting::calculateMetadata(const WMatrix* pData)
{
	delete[] m_pMetaData;
	m_pMetaData = new unsigned char[m_nRecordSize * pData->rows()];

	// Compute the distance to each neighbor
	m_dAveNeighborDist = 0;
	size_t m_goodNeighbors = 0;
	{
		// Get the appropriate neighbor finder
		std::unique_ptr<WNeighborFinderGeneralizing> hNF;
		WNeighborFinderGeneralizing* pNF = m_pNF;
		if(pNF)
		{
			if(pNF->data() != pData)
				throw Ex("Data mismatch");
		}
		else
		{
			pNF = new WKdTree(pData, nullptr, true);
			hNF.reset(pNF);
		}

		// Set up some some data structures that store the neighbors and distances of each point (and some other stuff)
		for(size_t i = 0; i < pData->rows(); i++)
		{
			stuff(i)->m_bAdjustable = true;
			pNF->findNearest(m_nNeighbors, i);
			pNF->sortNeighbors();
			struct WManifoldSculptingNeighbor* pArrNeighbors = record(i);
			for(size_t j = 0; j < m_nNeighbors; j++)
			{
				pArrNeighbors[j].m_nNeighbor = pNF->neighbor(j);
				m_goodNeighbors++;
				pArrNeighbors[j].m_nNeighborsNeighborSlot = INVALID_INDEX;
				pArrNeighbors[j].m_dDistance = sqrt(pNF->distance(j));
				m_dAveNeighborDist += pArrNeighbors[j].m_dDistance;
			}
		}
	}
	m_dAveNeighborDist /= m_goodNeighbors;
	m_dLearningRate = m_dAveNeighborDist;

	// For each data point, find the most co-linear of each neighbor's neighbors
	struct WManifoldSculptingNeighbor* pPoint;
	struct WManifoldSculptingNeighbor* pVertex;
	double dCosTheta;
	for(size_t n = 0; n < pData->rows(); n++)
	{
		pPoint = record(n);
		stuff(n)->m_nCycle = INVALID_INDEX;
		for(size_t i = 0; i < m_nNeighbors; i++)
		{
			size_t nVertex = pPoint[i].m_nNeighbor;
			if(nVertex < pData->rows())
			{
				pVertex = record(nVertex);
				pPoint[i].m_nNeighborsNeighborSlot = INVALID_INDEX;
#ifdef USE_ANGLES
				pPoint[i].m_dCosTheta = -100.0;
#else
				pPoint[i].m_dCosTheta = 100.0;
#endif
				for(size_t j = 0; j < m_nNeighbors; j++)
				{
					size_t nCandidate = pVertex[j].m_nNeighbor;
					if(nCandidate < pData->rows())
					{
#ifdef USE_ANGLES
						dCosTheta = acos(vectorCorrelation(pData->row(n).data(), pData->row(nVertex).data(), pData->row(nCandidate).data())) / M_PI;
						if(dCosTheta > pPoint[i].m_dCosTheta)
#else
						dCosTheta = vectorCorrelation(pData->row(n).data(), pData->row(nVertex).data(), pData->row(nCandidate).data());
						if(dCosTheta < pPoint[i].m_dCosTheta)
#endif
						{
							pPoint[i].m_dCosTheta = dCosTheta;
							pPoint[i].m_nNeighborsNeighborSlot = j;
						}
					}
				}

				// todo: is this really helpful?
				if(m_nTargetDims < 2)
					pPoint[i].m_dCosTheta = WBits::sign(pPoint[i].m_dCosTheta);
			}
		}
	}
}

void WManifoldSculpting::clampPoint(size_t n)
{
	stuff(n)->m_bAdjustable = false;
}

size_t WManifoldSculpting::countShortcuts(size_t nThreshold)
{
	size_t nShortcuts = 0;
	for(size_t n = 0; n < m_pData->rows(); n++)
	{
		struct WManifoldSculptingNeighbor* pPoint = record(n);
		for(size_t i = 0; i < m_nNeighbors; i++)
		{
			if(pPoint[i].m_nNeighbor < m_pData->rows() && std::abs((int)(pPoint[i].m_nNeighbor - n)) >= (int)nThreshold)
			{
				cout << "shortcut: " << n << "," << pPoint[i].m_nNeighbor << "\n";
				nShortcuts++;
			}
		}
	}
	return nShortcuts;
}

double WManifoldSculpting::vectorCorrelation(const double* pdA, const double* pdV, const double* pdB)
{
	double dDotProd = 0;
	double dMagA = 0;
	double dMagB = 0;
	double dA, dB;
	for(size_t n = 0; n < m_nDimensions; n++)
	{
		dA = pdA[n] - pdV[n];
		dB = pdB[n] - pdV[n];
		dDotProd += (dA * dB);
		dMagA += (dA * dA);
		dMagB += (dB * dB);
	}
	if(dDotProd == 0)
		return 0;
	double dCorrelation = dDotProd / (sqrt(dMagA) * sqrt(dMagB));
	WAssert(dCorrelation > -1.001 && dCorrelation < 1.001);
	return std::max(-1.0, std::min(1.0, dCorrelation));
}

double WManifoldSculpting::vectorCorrelation2(double squaredScale, size_t a, size_t vertex, struct WManifoldSculptingNeighbor* pNeighborRec)
{
	size_t slot = pNeighborRec->m_nNeighborsNeighborSlot;
	if(slot >= m_pData->rows())
		return 0.0;
	struct WManifoldSculptingNeighbor* pNeighborsNeighbors = record(vertex);
	size_t b = pNeighborsNeighbors[slot].m_nNeighbor;
	if(b >= m_pData->rows())
		return 0.0;
	double* pdA = m_pData->row(a).data();
	double* pdV = m_pData->row(vertex).data();
	double* pdB = m_pData->row(b).data();
	double dDotProd = 0;
	double dMagA = 0;
	double dMagB = 0;
	double dA, dB;
	for(size_t n = 0; n < m_nTargetDims; n++)
	{
		dA = pdA[n] - pdV[n];
		dB = pdB[n] - pdV[n];
		dDotProd += (dA * dB);
		dMagA += (dA * dA);
		dMagB += (dB * dB);
	}
	dDotProd += squaredScale * pNeighborRec->m_junkDotProd;
	dMagA += squaredScale * pNeighborRec->m_junkSquaredDist;
	dMagB += squaredScale * pNeighborsNeighbors[slot].m_junkSquaredDist;
	if(dDotProd == 0)
		return 0;
	double dCorrelation = dDotProd / (sqrt(dMagA) * sqrt(dMagB));
	WAssert(dCorrelation > -1.001 && dCorrelation < 1.001);
	return std::max(-1.0, std::min(1.0, dCorrelation));
}

double WManifoldSculpting::averageNeighborDistance(size_t nDims)
{
	double dSum = 0;
	size_t goodNeighbors = 0;
	for(size_t nPoint = 0; nPoint < m_pData->rows(); nPoint++)
	{
		struct WManifoldSculptingNeighbor* pPoint = record(nPoint);
		for(size_t n = 0; n < m_nNeighbors; n++)
		{
			if(pPoint[n].m_nNeighbor < m_pData->rows())
			{
				goodNeighbors++;
				dSum += sqrt(WMS_sqDist(m_pData->row(nPoint).data(), m_pData->row(pPoint[n].m_nNeighbor).data(), m_nDimensions));
			}
		}
	}
	return dSum / goodNeighbors;
}

double WManifoldSculpting::computeError(size_t nPoint)
{
	double dError = 0;
	double dDist;
	double dTheta;
	struct WManifoldSculptingNeighbor* pPoint = record(nPoint);
	struct WManifoldSculptingStuff* pNeighborStuff;
	size_t n;
	double squaredScale = m_scale * m_scale;
	size_t accepted = 0;
	for(n = 0; n < m_nNeighbors; n++)
	{
		if(pPoint[n].m_dDistance < m_minNeighborDist)
			continue;
		if(pPoint[n].m_dDistance > m_maxNeighborDist && accepted >= m_nTargetDims)
			break;
		accepted++;

		// Angles
		size_t nNeighbor = pPoint[n].m_nNeighbor;
		if(nNeighbor < m_pData->rows())
		{
#ifdef USE_ANGLES
			dTheta = acos(vectorCorrelation2(squaredScale, nPoint, nNeighbor, &pPoint[n])) / M_PI;
			dTheta -= pPoint[n].m_dCosTheta;
			if(dTheta > 0)
				dTheta = 0;
#else
			dTheta = vectorCorrelation2(squaredScale, nPoint, nNeighbor, &pPoint[n]);
			dTheta -= pPoint[n].m_dCosTheta;
			if(dTheta < 0)
				dTheta = 0;
#endif
			pNeighborStuff = stuff(nNeighbor);
			if(pNeighborStuff->m_nCycle != m_nPass && pNeighborStuff->m_bAdjustable)
				dTheta *= 0.4;

			// Distances
			dDist = sqrt(WMS_sqDist(m_pData->row(nPoint).data(), m_pData->row(nNeighbor).data(), m_nTargetDims) + squaredScale * pPoint[n].m_junkSquaredDist);
			dDist -= pPoint[n].m_dDistance;
			dDist /= std::max(m_dAveNeighborDist, 1e-10);
			if(pNeighborStuff->m_nCycle != m_nPass && pNeighborStuff->m_bAdjustable)
				dDist *= 0.4;
			dError += dDist * dDist + dTheta * dTheta;
		}
	}

	return dError + supervisedError(nPoint);
}

size_t WManifoldSculpting::adjustDataPoint(size_t nPoint, double* pError)
{
	bool bMadeProgress = true;
	double* pValues = m_pData->row(nPoint).data();
	double dErrorBase = computeError(nPoint);
	double dError = 0;
	double dStepSize = m_dLearningRate * (m_pRand->uniform() * .4 + .6); // We multiply the learning rate by a random value so that the points can get away from each other
	size_t nSteps;
	for(nSteps = 0; bMadeProgress && nSteps < 30; nSteps++)
	{
		bMadeProgress = false;
		for(size_t n = 0; n < m_nTargetDims; n++)
		{
			pValues[n] += dStepSize;
			dError = computeError(nPoint);
			if(dError >= dErrorBase)
			{
				pValues[n] -= (dStepSize + dStepSize);
				dError = computeError(nPoint);
			}
			if(dError >= dErrorBase)
				pValues[n] += dStepSize;
			else
			{
				dErrorBase = dError;
				bMadeProgress = true;
			}
		}
	}
	*pError = dError;
	return nSteps - 1; // the -1 is to undo the last incrementor
}

void WManifoldSculpting::moveMeanToOrigin()
{
	WTEMPBUF(double, mean, m_nTargetDims);
	WVec_setAll(mean, 0.0, m_nTargetDims);
	for(size_t i = 0; i < m_pData->rows(); i++)
		WVec_add(mean, m_pData->row(i).data(), m_nTargetDims);
	WVec_multiply(mean, -1.0 / m_pData->rows(), m_nTargetDims);
	for(size_t i = 0; i < m_pData->rows(); i++)
		WVec_add(m_pData->row(i).data(), mean, m_nTargetDims);
}

double WManifoldSculpting::squishPass(size_t nSeedDataPoint)
{
	if(!m_pMetaData)
		throw Ex("You must call BeginTransform before calling this method");
	struct WManifoldSculptingNeighbor* pPoint;
	struct WManifoldSculptingStuff* pStuff;

	// Squish the extra dimensions
	if(m_scale > 0.001)
	{
		m_scale *= m_dSquishingRate;
		if(m_scale > 0.001)
		{
			for(size_t n = 0; n < m_pData->rows(); n++)
				WVec_multiply(m_pData->row(n).data() + m_nTargetDims, m_dSquishingRate, m_nDimensions - m_nTargetDims);
		}
		else
		{
			for(size_t n = 0; n < m_pData->rows(); n++)
				WVec_setAll(m_pData->row(n).data() + m_nTargetDims, 0.0, m_nDimensions - m_nTargetDims);
			m_scale = 0;
		}
		while(averageNeighborDistance(m_nDimensions) < m_dAveNeighborDist)
		{
			for(size_t n = 0; n < m_pData->rows(); n++)
				WVec_multiply(m_pData->row(n).data(), 1.0 / m_dSquishingRate, m_nTargetDims);
		}
	}

	// Start at the seed point and correct outward in a breadth-first mannner
	m_q.push_back(nSeedDataPoint);
	[[maybe_unused]] size_t nVisitedNodes = 0;
	size_t nSteps = 0;
	double dError = 0;
	double dTotalError = 0;
	while(m_q.size() > 0)
	{
		// Check if this one has already been done
		size_t nPoint = m_q.front();
		m_q.pop_front();
		pPoint = record(nPoint);
		pStuff = stuff(nPoint);
		if(pStuff->m_nCycle == m_nPass)
			continue;
		pStuff->m_nCycle = m_nPass;
		nVisitedNodes++;

		// Push all neighbors into the queue
		for(size_t n = 0; n < m_nNeighbors; n++)
		{
			if(pPoint[n].m_nNeighbor < m_pData->rows())
				m_q.push_back(pPoint[n].m_nNeighbor);
		}

		// Adjust this data point
		if(pStuff->m_bAdjustable && nPoint != nSeedDataPoint)
		{
			nSteps += adjustDataPoint(nPoint, &dError);
			dTotalError += dError;
		}
	}
	if(nSteps < m_pData->rows())
		m_dLearningRate *= .87;
	else
		m_dLearningRate /= .91;
	//	cout << "[Learning Rate: " << m_dLearningRate << "]\n";

	if(m_nPass % 20 == 0)
		moveMeanToOrigin();

	m_nPass++;
	return dTotalError;
}






WIsomap::WIsomap(size_t neighborCount, size_t targetDims, WRand* pRand) : m_neighborCount(neighborCount), m_targetDims(targetDims), m_pNF(nullptr), m_pRand(pRand), m_dropDisconnectedPoints(false)
{
}

WIsomap::WIsomap(WDomNode* pNode)
: WTransform(pNode)
{
	m_targetDims = (size_t)pNode->getInt("targetDims");
}

// virtual
WIsomap::~WIsomap()
{
}

WDomNode* WIsomap::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WIsomap");
	pNode->add(pDoc, "targetDims", m_targetDims);
	return pNode;
}

void WIsomap::setNeighborFinder(WNeighborFinder* pNF)
{
	m_pNF = pNF;
}

// virtual
WMatrix* WIsomap::reduce(const WMatrix& in)
{
	WNeighborFinder* pNF = m_pNF;
	std::unique_ptr<WNeighborFinder> hNF;
	if(!pNF)
	{
		pNF = new WKdTree(&in, nullptr, true);
		hNF.reset(pNF);
	}

	// Compute the distance matrix using the Floyd Warshall algorithm
	WFloydWarshall graph(in.rows());
	for(size_t i = 0; i < in.rows(); i++)
	{
		size_t nc = pNF->findNearest(m_neighborCount, i);
		for(size_t j = 0; j < nc; j++)
		{
			double d = sqrt(pNF->distance(j));
			graph.addDirectedEdge(i, pNF->neighbor(j), d);
		}
	}
	graph.compute();
	if(!graph.isConnected())
	{
		if(!m_dropDisconnectedPoints)
			throw Ex("The local neighborhoods do not form a connected graph. Increasing the neighbor count may be a good solution. Another solution is to specify to dropDisconnectedPoints.");
		WMatrix* pCM = graph.costMatrix();
		size_t c = pCM->cols();
		while(true)
		{
			size_t worstRow = 0;
			size_t missing_count = 0;
			for(size_t i = 0; i < pCM->rows(); i++)
			{
				double* pRow = pCM->row(i).data();
				size_t count = 0;
				for(size_t j = 0; j < c; j++)
				{
					if(*(pRow++) >= 1e200)
						count++;
				}
				if(count > missing_count)
				{
					missing_count = count;
					worstRow = i;
				}
			}
			if(missing_count > 0)
			{
				pCM->deleteRow(worstRow);
				pCM->deleteColumns(worstRow, 1);
			}
			else
				break;
		}
	}

	// Do classic MDS on the distance matrix
	return WManifold::multiDimensionalScaling(graph.costMatrix(), m_targetDims, m_pRand, false);
}














#define SPARSE

// Locally Linear Embedding
class WLLEHelper
{
protected:
	const WMatrix* m_pInputData;
	WMatrix* m_pOutputData;
	size_t m_nInputDims;
	size_t m_nTargetDims;
	size_t m_nNeighbors;
	size_t* m_pNeighbors;
#ifdef SPARSE
	WSparseMatrix* m_pWeights;
#else
	WMatrix* m_pWeights;
#endif
	WRand* m_pRand;

	WLLEHelper(const WMatrix* pData, size_t nTargetDims, size_t nNeighbors, WRand* pRand);
public:
	~WLLEHelper();

	// Uses LLE to compute the reduced dimensional embedding of the data
	// associated with pNF.
	static WMatrix* doLLE(WNeighborFinder* pNF, size_t nTargetDims, size_t neighbors, WRand* pRand);

protected:
	void findNeighbors(WNeighborFinder* pNF);
	void findNeighborsTheSlowWay();
	void computeWeights();
	void computeEmbedding();
	WMatrix* releaseOutputData();
};

WLLEHelper::WLLEHelper(const WMatrix* pData, size_t nTargetDims, size_t nNeighbors, WRand* pRand)
: m_pRand(pRand)
{
	m_pInputData = pData;
	m_nTargetDims = nTargetDims;
	m_nNeighbors = nNeighbors;
	m_pNeighbors = nullptr;
	m_pWeights = nullptr;
	m_pOutputData = nullptr;
}

WLLEHelper::~WLLEHelper()
{
	delete(m_pNeighbors);
	delete(m_pWeights);
	delete(m_pOutputData);
}

WMatrix* WLLEHelper::releaseOutputData()
{
	WMatrix* pData = m_pOutputData;
	m_pOutputData = nullptr;
	return pData;
}

// static
WMatrix* WLLEHelper::doLLE(WNeighborFinder* pNF, size_t nTargetDims, size_t neighbors, WRand* pRand)
{
	WLLEHelper lle(pNF->data(), nTargetDims, neighbors, pRand);
	lle.findNeighbors(pNF);
	lle.computeWeights();
	lle.computeEmbedding();
	return lle.releaseOutputData();
}

void WLLEHelper::findNeighbors(WNeighborFinder* pNF)
{
	delete(m_pNeighbors);
	m_pNeighbors = new size_t[m_nNeighbors * m_pInputData->rows()];
	size_t* pHood = m_pNeighbors;
	for(size_t i = 0; i < m_pInputData->rows(); i++)
	{
		size_t nc = pNF->findNearest(m_nNeighbors, i);
		for(size_t j = 0; j < nc; j++)
			pHood[j] = pNF->neighbor(j);
		for(size_t j = nc; j < m_nNeighbors; j++)
			pHood[j] = INVALID_INDEX;
		pHood += m_nNeighbors;
	}
}

void WLLEHelper::computeWeights()
{
	size_t nRowCount = m_pInputData->rows();
	WTEMPBUF(double, pVec, m_nNeighbors);
#ifdef SPARSE
	m_pWeights = new WSparseMatrix(nRowCount, nRowCount);
#else
	m_pWeights = new WMatrix(nRowCount, nRowCount);
#endif
	for(size_t n = 0; n < nRowCount; n++)
	{
		WManifold::computeNeighborWeights(m_pInputData, n, m_nNeighbors, m_pNeighbors + n * m_nNeighbors, pVec);
		size_t pos = 0;
		size_t* pHood = m_pNeighbors + n * m_nNeighbors;
#ifdef SPARSE
		for(size_t i = 0; i < m_nNeighbors; i++)
		{
			if(pHood[i] < nRowCount)
				m_pWeights->set(n, pHood[i], pVec[pos++]);
			else
				pos++;
		}
#else
		WVec_setAll(m_pWeights->row(n), 0.0, nRowCount);
		for(size_t i = 0; i < m_nNeighbors; i++)
		{
			if(pHood[i] < nRowCount)
				m_pWeights->row(n)[pHood[i]] = pVec[pos++];
			else
				pos++;
		}
#endif
	}
}

void WLLEHelper::computeEmbedding()
{
	//  Subtract the weights from the identity matrix
	size_t row, col;
	m_pWeights->multiply(-1.0);
	size_t nRowCount = m_pInputData->rows();
#ifdef SPARSE
	for(row = 0; row < nRowCount; row++)
		m_pWeights->set(row, row, m_pWeights->get(row, row) + 1.0);
#else
	for(row = 0; row < nRowCount; row++)
		m_pWeights->row(row)[row] += 1.0;
#endif

	// Compute the smallest (m_nTargetDims+1) eigenvectors of (A^T)A, where A is m_pWeights
#ifdef SPARSE
	// The sparse matrix SVD way
	WSparseMatrix* pU;
	double* diag;
	WSparseMatrix* pV;
	m_pWeights->singularValueDecomposition(&pU, &diag, &pV);
	std::unique_ptr<WSparseMatrix> hU(pU);
	std::unique_ptr<double[]> hDiag(diag);
	std::unique_ptr<WSparseMatrix> hV(pV);
	WMatrix* pEigVecs = new WMatrix(m_nTargetDims + 1, pV->cols());
	std::unique_ptr<WMatrix> hEigVecs(pEigVecs);
	for(size_t i = 1; i <= m_nTargetDims; i++)
	{
		size_t rowIn = pV->rows() - 1 - i;
		double* pRow = pEigVecs->row(i).data();
		for(size_t j = 0; j < pV->cols(); j++)
			pRow[j] = pV->get(rowIn, j);
	}
#else
	/*
	// The brute-force way (slow and not very precise)
	WMatrix* pTmp = m_pWeights->clone();
	std::unique_ptr<WMatrix> hTmp(pTmp);
	WMatrix* pEigVecs = new WMatrix(nRowCount);
	std::unique_ptr<WMatrix> hEigVecs(pEigVecs);
	pEigVecs->newRows(m_nTargetDims + 1);
	WTEMPBUF(double, mean, nRowCount);
	pTmp->meanVector(mean);
	for(size_t i = 0; i < nRowCount; i++)
	{
		size_t r = std::min(m_nTargetDims, nRowCount - 1 - i);
		pTmp->principalComponent(pEigVecs->row(r), nRowCount, mean, m_pRand);
		pTmp->removeComponent(mean, pEigVecs->row(r), nRowCount);
	}
*/

	// The SVD way (seems to be the fastest and most accurate)
	WMatrix* pU;
	double* diag;
	WMatrix* pV;
	m_pWeights->singularValueDecomposition(&pU, &diag, &pV);
	std::unique_ptr<WMatrix> hU(pU);
	std::unique_ptr<double[]> hDiag(diag);
	std::unique_ptr<WMatrix> hV(pV);
	WMatrix* pEigVecs = new WMatrix(pV->relation(), pV->heap());
	std::unique_ptr<WMatrix> hEigVecs(pEigVecs);
	for(size_t i = 0; i <= m_nTargetDims; i++)
		pEigVecs->takeRow(pV->releaseRow(pV->rows() - 1));

/*
	// The standard way
	WMatrix* m = WMatrix::multiply(*m_pWeights, *m_pWeights, true, false);
	std::unique_ptr<WMatrix> hM(m);
	WMatrix* pEigVecs = m->eigs(m_nTargetDims + 1, NULL, m_pRand, false);
	std::unique_ptr<WMatrix> hEigVecs(pEigVecs);
*/
#endif

	// Make the output data
	m_pOutputData = new WMatrix(nRowCount, m_nTargetDims);
	double d = sqrt((double)nRowCount);
	for(row = 0; row < nRowCount; row++)
	{
		double* pRow = m_pOutputData->row(row).data();
		for(col = 0; col < m_nTargetDims; col++)
			pRow[col] = pEigVecs->row(col + 1)[row] * d;
	}
}


WLLE::WLLE(size_t neighborCount, size_t targetDims, WRand* pRand) : m_neighborCount(neighborCount), m_targetDims(targetDims), m_pNF(nullptr), m_pRand(pRand)
{
}

WLLE::WLLE(WDomNode* pNode)
: WTransform(pNode)
{
	m_targetDims = (size_t)pNode->getInt("targetDims");
}

// virtual
WLLE::~WLLE()
{
}

WDomNode* WLLE::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WLLE");
	pNode->add(pDoc, "targetDims", m_targetDims);
	return pNode;
}

void WLLE::setNeighborFinder(WNeighborFinder* pNF)
{
	m_pNF = pNF;
}

// virtual
WMatrix* WLLE::reduce(const WMatrix& in)
{
	WNeighborFinder* pNF = m_pNF;
	std::unique_ptr<WNeighborFinder> hNF;
	if(!pNF)
	{
		pNF = new WKdTree(&in, nullptr, true);
		hNF.reset(pNF);
	}
	return WLLEHelper::doLLE(pNF, m_targetDims, m_neighborCount, m_pRand);
}













WBreadthFirstUnfolding::WBreadthFirstUnfolding(size_t reps, size_t neighborCount, size_t targetDims)
: m_reps(reps), m_neighborCount(neighborCount), m_targetDims(targetDims), m_pNF(nullptr), m_useMds(true), m_rand(0)
{
}

WBreadthFirstUnfolding::WBreadthFirstUnfolding(WDomNode* pNode)
: m_reps((size_t)pNode->getInt("reps")), m_neighborCount((size_t)pNode->getInt("neighbors")), m_targetDims((size_t)pNode->getInt("targetDims")), m_pNF(nullptr), m_useMds(pNode->getBool("useMds")), m_rand(0)
{
}

// virtual
WBreadthFirstUnfolding::~WBreadthFirstUnfolding()
{
}

WDomNode* WBreadthFirstUnfolding::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "reps", m_reps);
	pNode->add(pDoc, "neighbors", m_neighborCount);
	pNode->add(pDoc, "targetDims", m_targetDims);
	pNode->add(pDoc, "useMds", m_useMds);
	if(m_pNF)
		throw Ex("sorry, serializing a neighbor finder is not yet implemented");
	return pNode;
}

void WBreadthFirstUnfolding::setNeighborFinder(WNeighborFinder* pNF)
{
	m_pNF = pNF;
}

void WVec_pairwiseDivide(double* pDest, double* pOther, size_t dims)
{
	while(dims > 0)
	{
		*(pDest++) /= *(pOther++);
		dims--;
	}
}

void WVec_subtract(double* pDest, const double* pSource, size_t nDims)
{
	for(size_t i = 0; i < nDims; i++)
	{
		*pDest -= *pSource;
		pDest++;
		pSource++;
	}
}

double WVec_squaredMagnitude(const double* pVector, size_t nSize)
{
	double dMag = 0;
	while(nSize > 0)
	{
		dMag += ((*pVector) * (*pVector));
		pVector++;
		nSize--;
	}
	return dMag;
}

void WVec_normalize(double* pVector, size_t nSize)
{
	double dMag = WVec_squaredMagnitude(pVector, nSize);
	if(dMag <= 0)
		pVector[0] = 1.0;
	else
		WVec_multiply(pVector, 1.0 / sqrt(dMag), nSize);
}

// virtual
WMatrix* WBreadthFirstUnfolding::reduce(const WMatrix& in)
{
	// Obtain the neighbor finder
	WNeighborFinder* pNF = m_pNF;
	std::unique_ptr<WNeighborFinder> hNF;
	if(!pNF)
	{
		pNF = new WKdTree(&in, nullptr, true);
		hNF.reset(pNF);
	}

	// Make sure the neighbor finder is cached
	std::unique_ptr<WNeighborGraph> hNF2;
	if(!pNF->isCached())
	{
		WNeighborGraph* pNF2 = new WNeighborGraph(pNF, false, m_neighborCount);
		hNF2.reset(pNF2);
		pNF = pNF2;
	}
	WNeighborGraph* pCachedNF = (WNeighborGraph*)pNF;
	const WMatrix* pData = pNF->data();

	// Learn the manifold
	double* pGlobalWeights = new double[in.rows() * 2];
	std::unique_ptr<double[]> hGlobalWeights(pGlobalWeights);
	double* pLocalWeights = pGlobalWeights + in.rows();
	WVec_setAll(pGlobalWeights, 0.0, in.rows());
	std::unique_ptr<WMatrix> hFinal;
	for(size_t i = 0; i < m_reps; i++)
	{
		WMatrix* pRep = unfold(pData, pCachedNF, (size_t)m_rand.next(pData->rows()), pLocalWeights);
		if(hFinal.get())
		{
			WVec_add(pGlobalWeights, pLocalWeights, in.rows());
			WVec_pairwiseDivide(pLocalWeights, pGlobalWeights, in.rows());
			std::unique_ptr<WMatrix> hRep(pRep);
			hFinal.reset(WManifold::blendEmbeddings(pRep, pLocalWeights, hFinal.get(), m_neighborCount, pCachedNF, (size_t)m_rand.next(pData->rows())));
		}
		else
		{
			hFinal.reset(pRep);
			std::swap(pGlobalWeights, pLocalWeights);
		}
	}
	return hFinal.release();
}

// static
double WBreadthFirstUnfolding::refinePoint(double* pPoint, double* pNeighbor, size_t dims, double distance, double learningRate, WRand* pRand)
{
	WTEMPBUF(double, buf, dims);
	WVec_copy(buf, pPoint, dims);
	WVec_subtract(buf, pNeighbor, dims);
	double mag = WVec_squaredMagnitude(buf, dims);
	WVec_normalize(buf, dims);
	WVec_multiply(buf, distance, dims);
	WVec_add(buf, pNeighbor, dims);
	WVec_subtract(buf, pPoint, dims);
	WVec_multiply(buf, learningRate, dims);
	WVec_add(pPoint, buf, dims);
	return mag;
}

void WBreadthFirstUnfolding::refineNeighborhood(WMatrix* pLocal, size_t rootIndex, WNeighborGraph* pNeighborGraph)
{
	// Determine the index of every row in pLocal
	WTEMPBUF(size_t, indexes, pLocal->rows())
	size_t nc = pNeighborGraph->findNearest(m_neighborCount, rootIndex);
	indexes[0] = rootIndex;
	size_t pos = 1;
	for(size_t i = 0; i < nc; i++)
	{
		indexes[pos++] = pNeighborGraph->neighbor(i);
	}

	// Make a pair-wise distance table
	WTEMPBUF(double, distTable, pLocal->rows() * pLocal->rows());
	double* pTablePos = distTable;
	for(size_t i = 0; i < pLocal->rows(); i++)
	{
		// Make a map to the indexes of point i's neighbors
		size_t indexI = indexes[i];
		nc = pNeighborGraph->findNearest(m_neighborCount, indexI);
		map<size_t, size_t> indexMap;
		for(size_t j = 0; j < nc; j++)
		{
			indexMap.insert(make_pair(pNeighborGraph->neighbor(j), j));
		}
		for(size_t j = 0; j < pLocal->rows(); j++)
		{
			size_t indexJ = indexes[j];
			map<size_t, size_t>::iterator it = indexMap.find(indexJ);
			if(it != indexMap.end())
				*pTablePos = sqrt(pNeighborGraph->distance(it->second));
			else
				*pTablePos = UNKNOWN_REAL_VALUE;
			pTablePos++;
		}
	}

	// Fill holes with symmetric values
	pTablePos = distTable;
	for(size_t i = 0; i < pLocal->rows(); i++)
	{
		for(size_t j = 0; j < pLocal->rows(); j++)
		{
			if(*pTablePos == UNKNOWN_REAL_VALUE)
			{
				if(distTable[pLocal->rows() * j + i] != UNKNOWN_REAL_VALUE)
					*pTablePos = distTable[pLocal->rows() * j + i];
			}
			pTablePos++;
		}
	}

	// Refine the points
	double firstErr = 0;
	for(size_t iters = 0; iters < 30; iters++)
	{
		double err = 0;
		pTablePos = distTable;
		for(size_t j = 0; j < pLocal->rows(); j++)
		{
			for(size_t i = 0; i < pLocal->rows(); i++)
			{
				if(*pTablePos != UNKNOWN_REAL_VALUE)
					err += refinePoint(pLocal->row(i).data(), pLocal->row(j).data(), m_targetDims, *pTablePos, 0.1, &m_rand);
				pTablePos++;
			}
		}
		if(iters == 0)
			firstErr = err;
		else if(iters == 29 && err > firstErr)
			throw Ex("made it worse");
	}
}

WMatrix* WBreadthFirstUnfolding::reduceNeighborhood(const WMatrix* pIn, size_t index, WNeighborGraph* pNeighborGraph)
{
	WMatrix* pReducedNeighborhood = nullptr;
	if(m_useMds)
	{
		// Build index tables
		WTEMPBUF(size_t, indexes, m_neighborCount + 1);
		size_t localSize = 0;
		map<size_t, size_t> revIndexes;
		map<size_t, size_t>::iterator it;
		revIndexes.insert(make_pair(index, localSize));
		indexes[localSize++] = index;
		size_t nc = pNeighborGraph->findNearest(m_neighborCount, index);
		for(size_t j = 0; j < nc; j++)
		{
			revIndexes.insert(make_pair(pNeighborGraph->neighbor(j), localSize));
			indexes[localSize++] = pNeighborGraph->neighbor(j);
		}

		// Build a distance matrix
		WFloydWarshall graph(localSize);
		for(size_t i = 0; i < localSize; i++)
		{
			size_t from = indexes[i];
			nc = pNeighborGraph->findNearest(m_neighborCount, from);
			for(size_t j = 0; j < m_neighborCount; j++)
			{
				size_t to = pNeighborGraph->neighbor(j);
				it = revIndexes.find(to);
				if(it == revIndexes.end())
					continue;
				double d = sqrt(pNeighborGraph->distance(j));
				graph.addDirectedEdge(i, it->second, d);
				graph.addDirectedEdge(it->second, i, d);
			}
		}
		graph.compute();
		WAssert(graph.isConnected());

		// Use MDS to reduce the neighborhood
		pReducedNeighborhood = WManifold::multiDimensionalScaling(graph.costMatrix(), m_targetDims, &m_rand, false);
	}
	else
	{
		// Make a local neighborhood
		WMatrix local(pIn->relation().clone());
		WReleaseDataHolder hLocal(&local);
		local.takeRow((WVec*)&pIn->row(index));
		size_t nc = pNeighborGraph->findNearest(m_neighborCount, index);
		for(size_t j = 0; j < nc; j++)
		{
			local.takeRow((WVec*)&pIn->row(pNeighborGraph->neighbor(j)));
		}

		// Use PCA to reduce the neighborhood
		WPCA pca(m_targetDims);
		pca.train(local);
		pReducedNeighborhood = pca.transformBatch(local);
	}

	return pReducedNeighborhood;
}

WMatrix* WBreadthFirstUnfolding::unfold(const WMatrix* pIn, WNeighborGraph* pNeighborTable, size_t seed, double* pOutWeights)
{
	// Reduce the seed neighborhood
	WMatrix* pOut = new WMatrix(pIn->rows(), m_targetDims);
	std::unique_ptr<WMatrix> hOut(pOut);
	deque<size_t> q;
	WBitTable visited(pIn->rows());
	WBitTable established(pIn->rows());
	{
		WMatrix* pLocal = reduceNeighborhood(pIn, seed, pNeighborTable);
		std::unique_ptr<WMatrix> hLocal(pLocal);
		WVec_copy(pOut->row(seed).data(), pLocal->row(0).data(), m_targetDims);
		visited.set(seed);
		established.set(seed);
		size_t nc = pNeighborTable->findNearest(m_neighborCount, seed);
		size_t i = 1;
		for(size_t j = 0; j < nc; j++)
		{
			size_t neigh = pNeighborTable->neighbor(j);
			WVec_copy(pOut->row(neigh).data(), pLocal->row(i).data(), m_targetDims);
			visited.set(neigh);
			q.push_back(neigh);
			q.push_back(1);
			i++;
		}
	}
	pOutWeights[seed] = 8.0;

	// Reduce in a breadth-first manner
	WVec mean(m_targetDims);
	while(q.size() > 0)
	{
		size_t par = q.front();
		q.pop_front();
		size_t depth = q.front();
		q.pop_front();
		pOutWeights[par] = 1.0 / (double)depth;

		// Make a blended neighborhood
		WMatrix* pLocal = reduceNeighborhood(pIn, par, pNeighborTable);
		std::unique_ptr<WMatrix> hLocal(pLocal);

		// Make sub-neighborhoods that contain only tentatively-placed points
		WMatrix tentativeC(pOut->relation().clone());
		WMatrix tentativeD(pLocal->relation().clone());
		WReleaseDataHolder hTentativeD(&tentativeD);
		WVec_copy(tentativeC.newRow().data(), pOut->row(par).data(), m_targetDims);
		tentativeD.takeRow(&pLocal->row(0));
		size_t nc = pNeighborTable->findNearest(m_neighborCount, par);
		size_t ii = 1;
		for(size_t j = 0; j < nc; j++)
		{
			if(visited.bit(pNeighborTable->neighbor(j)))
			{
				WVec_copy(tentativeC.newRow().data(), pOut->row(pNeighborTable->neighbor(j)).data(), m_targetDims);
				tentativeD.takeRow(&pLocal->row(ii));
			}
			ii++;
		}

		// Subtract the means
		tentativeD.centroid(mean);
		for(size_t i = 0; i < pLocal->rows(); i++)
			WVec_subtract(pLocal->row(i).data(), mean.data(), m_targetDims); // (This will affect tentativeD too b/c it refs the same rows)
		tentativeC.centroid(mean);
		for(size_t i = 0; i < tentativeC.rows(); i++)
			WVec_subtract(tentativeC.row(i).data(), mean.data(), m_targetDims);

		// Compute the rotation to align the tentative neighborhoods
		WMatrix* pKabsch = WMatrix::kabsch(&tentativeC, &tentativeD);
		std::unique_ptr<WMatrix> hKabsch(pKabsch);

		// Compute an aligned version of pLocal that fits with pOut
		WMatrix* pAligned = WMatrix::multiply(*pLocal, *pKabsch, false, false);
		std::unique_ptr<WMatrix> hAligned(pAligned);
		for(size_t i = 0; i < pAligned->rows(); i++)
			WVec_add(pAligned->row(i).data(), mean.data(), m_targetDims);

		// Accept the new points
		WVec_copy(pOut->row(par).data(), pAligned->row(0).data(), m_targetDims);
		established.set(par);
		ii = 1;
		for(size_t j = 0; j < nc; j++)
		{
			if(!established.bit(pNeighborTable->neighbor(j)))
				WVec_copy(pOut->row(pNeighborTable->neighbor(j)).data(), pAligned->row(ii).data(), m_targetDims);
			if(!visited.bit(pNeighborTable->neighbor(j)))
			{
				visited.set(pNeighborTable->neighbor(j));
				q.push_back(pNeighborTable->neighbor(j));
				q.push_back(depth + 1);
			}
			ii++;
		}
	}
	return hOut.release();
}
















WScalingUnfolder::WScalingUnfolder()
: m_neighborCount(14),
  m_targetDims(2),
  m_passes(50),
  m_refines_per_scale(100),
  m_scaleRate(0.9),
  m_rand(0)
{
}

WScalingUnfolder::WScalingUnfolder(WDomNode* pNode)
: WTransform(pNode),
  m_rand(0)
{
	throw Ex("Sorry, this method is not implemented yet");
}

// virtual
WScalingUnfolder::~WScalingUnfolder()
{
}

void WScalingUnfolder_adjustPoints(double* pA, double* pB, size_t dims, double curSqDist, double tarSqDist, WRand& rand)
{
	if(curSqDist == 0.0)
	{
		if(tarSqDist > 0.0)
		{
			// Perturb A and B by a small random amount to separate them
			double d = 0.01 * sqrt(tarSqDist);
			for(size_t i = 0; i < dims; i++)
			{
				*(pA++) += d * rand.normal();
				*(pB++) += d * rand.normal();
			}
		}
		return; // It is easier to just return than to recalculate curSqDist. This edge will probably be visited again soon anyway.
	}
	double scal = std::max(0.5, std::min(2.0, sqrt(tarSqDist) / sqrt(curSqDist)));
	for(size_t i = 0; i < dims; i++)
	{
		double t = 0.5 * (*pB * (1.0 + scal) + *pA * (1.0 - scal)) - *pB;
		*(pA++) -= t;
		*(pB++) += t;
	}
}

// static
void WScalingUnfolder::restore_local_distances_pass(WMatrix& intrinsic, WNeighborGraph& ng, size_t neighborCount, WRand& rand)
{

	// Do it in breadth-first order
	size_t dims = intrinsic.cols();
	std::queue<size_t> q;
	WBitTable used(ng.data()->rows());
	size_t random_row = rand.next(intrinsic.rows());
	size_t random_neigh = rand.next(ng.findNearest(neighborCount, random_row));
	size_t seed = random_row * neighborCount + random_neigh;
	q.push(seed);
	while(q.size() > 0)
	{
		// Get the next edge from the queue
		size_t edge = q.front();
		q.pop();
		size_t a = edge / neighborCount;
		ng.findNearest(neighborCount, a);
		size_t neighbor_index = edge % neighborCount;
		size_t b = ng.neighbor(neighbor_index);

		// add all edges that connect to either end of this edge
		if(!used.bit(a))
		{
			used.set(a);
			size_t ed = neighborCount * a;
			for(size_t i = 0; i < neighborCount; i++)
				q.push(ed++);
		}
		if(!used.bit(b))
		{
			used.set(b);
			size_t ed = neighborCount * b;
			for(size_t i = 0; i < neighborCount; i++)
				q.push(ed++);
		}

		double dTarget = ng.distance(neighbor_index);
		WVec& aa = intrinsic.row(a);
		WVec& bb = intrinsic.row(b);
		double dCur = aa.squaredDistance(bb);
		WScalingUnfolder_adjustPoints(aa.data(), bb.data(), dims, dCur, dTarget, rand);
	}
}

void WScalingUnfolder::unfold(WMatrix& intrinsic, WNeighborGraph& nf, size_t encoderTrainIters, WNeuralNetLearner* pEncoder, WNeuralNetLearner* pDecoder, const WMatrix* pVisible)
{
	WRandomIndexIterator* ii = pEncoder ? new WRandomIndexIterator(intrinsic.rows(), m_rand) : nullptr;
	std::unique_ptr<WRandomIndexIterator> hII2(ii);
	for(size_t pass = 0; pass < m_passes; pass++)
	{
		// Scale up the data
		intrinsic.multiply(1.0 / m_scaleRate);

		for(size_t i = 0; i < m_refines_per_scale; i++)
			restore_local_distances_pass(intrinsic, nf, m_neighborCount, m_rand);

		// Train the encoder
		if(pVisible)
		{
			intrinsic.centerMeanAtOrigin();
			for(size_t i = 0; i < encoderTrainIters; i++)
			{
				ii->reset();
				size_t ind;
				while(ii->next(ind))
				{
					pEncoder->trainIncremental(pVisible->row(ind), intrinsic[ind]);
					pDecoder->trainIncremental(intrinsic[ind], pVisible->row(ind));
				}
			}
		}
	}
}

// static
size_t WScalingUnfolder::unfold_iter(WMatrix& intrinsic, WRand& rand, size_t neighborCount, double scaleFactor, size_t refinements)
{
	while(true)
	{
		// Find neighbors
		WKdTree kdtree(&intrinsic, nullptr, false);
		WNeighborGraph ng(&kdtree, false, neighborCount);
		if(!ng.isConnected())
		{
			if(neighborCount < 1 || neighborCount >= intrinsic.rows() - 1)
				throw Ex("Invalid neighborCount");
			neighborCount = std::min(intrinsic.rows() - 1, std::max(neighborCount + 1, neighborCount * 3 / 2));
			continue;
		}

		// Scale up the data
		intrinsic.multiply(scaleFactor);

		// Refine the points to restore distances in local neighborhoods
		for(size_t i = 0; i < refinements; i++)
			restore_local_distances_pass(intrinsic, ng, neighborCount, rand);
		return neighborCount;
	}
}

// virtual
WMatrix* WScalingUnfolder::reduce(const WMatrix& in)
{
	// Find neighbors
	WKdTree kdtree(&in, nullptr, false);
	WNeighborGraph nf(&kdtree, false, m_neighborCount);

	// Make a copy of the data
	WMatrix intrinsic(in);
	unfold(intrinsic, nf);

	// Shift the variance into the first few dimensions
	WPCA pca(m_targetDims);
	pca.train(intrinsic);
	return pca.transformBatch(intrinsic);
}





} // namespace WClasses
