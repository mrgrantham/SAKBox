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

#include "WNeighborFinder.h"
#include "WVec.h"
#include "WRand.h"
#include "WPlot.h"
#include <stdlib.h>
#include <vector>
#include <queue>
#include <set>
#include "WOptimizer.h"
#include "WHillClimber.h"
#include <string.h>
#include "WGraph.h"
#include "WBitTable.h"
#include <deque>
#include "WDom.h"
#include "WKNN.h"
#include "WHolders.h"
#include "WTransform.h"
#include <sstream>
#include <string>
#include <iostream>
#include "WDistance.h"
#include <cmath>
#include <map>
#include "WPriorityQueue.h"
#include <memory>
#include "WSparseMatrix.h"


//using std::cerr;
using std::vector;
using std::priority_queue;
using std::set;
using std::deque;
using std::make_pair;
using std::pair;
using std::string;
using std::map;
using std::multimap;

namespace WClasses
{


WNeighborGraph::WNeighborGraph(WNeighborFinder* pNF, bool own, size_t neighbors)
: WNeighborFinder(pNF->data()), m_pNF(pNF), m_own(own)
{
	m_neighs.resize(m_pData->rows());
	m_dists.resize(m_pData->rows());
	fillCacheNearest(neighbors);
}

WNeighborGraph::WNeighborGraph(double squaredRadius, WNeighborFinder* pNF, bool own)
: WNeighborFinder(pNF->data()), m_pNF(pNF), m_own(own)
{
	m_neighs.resize(m_pData->rows());
	m_dists.resize(m_pData->rows());
	fillCacheRadius(squaredRadius);
}

WNeighborGraph::WNeighborGraph(bool own, WNeighborFinderGeneralizing* pNF)
: WNeighborFinder(pNF->data()), m_pNF(pNF), m_own(own)
{
	m_neighs.resize(m_pData->rows());
	m_dists.resize(m_pData->rows());
	WDistanceMetric* pMetric = pNF->metric();
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		double prev = 0.0;
		if(i > 0)
			prev = pMetric->squaredDistance(pNF->data()->row(i), pNF->data()->row(i - 1));
		double next = 0.0;
		if(i + 1 < m_pData->rows())
			next = pMetric->squaredDistance(pNF->data()->row(i), pNF->data()->row(i + 1));
		double maxSqRad = std::max(prev, next);
		size_t neigh_count = m_pNF->findWithinRadius(maxSqRad, i);
		m_neighs[i].clear();
		m_dists[i].clear();
		for(size_t j = 0; j < neigh_count; j++)
		{
			m_neighs[i].push_back(m_pNF->neighbor(j));
			m_dists[i].push_back(m_pNF->distance(j));
		}
	}
}

// virtual
WNeighborGraph::~WNeighborGraph()
{
	if(m_own)
		delete(m_pNF);
}

void WNeighborGraph::fillCacheNearest(size_t k)
{
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		size_t neigh_count = m_pNF->findNearest(k, i);
		m_neighs[i].clear();
		m_dists[i].clear();
		for(size_t j = 0; j < neigh_count; j++)
		{
			m_neighs[i].push_back(m_pNF->neighbor(j));
			m_dists[i].push_back(m_pNF->distance(j));
		}
	}
}

void WNeighborGraph::fillCacheRadius(double squaredRadius)
{
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		size_t neigh_count = m_pNF->findWithinRadius(squaredRadius, i);
		m_neighs[i].clear();
		m_dists[i].clear();
		for(size_t j = 0; j < neigh_count; j++)
		{
			m_neighs[i].push_back(m_pNF->neighbor(j));
			m_dists[i].push_back(m_pNF->distance(j));
		}
	}
}

void WNeighborGraph::recomputeDistances(WDistanceMetric* pMetric)
{
	pMetric->init(&m_pData->relation(), false);
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		vector<size_t>& neighs = m_neighs[i];
		vector<double>& dists = m_dists[i];
		for(size_t j = 0; j < neighs.size(); j++)
		{
			dists[j] = pMetric->squaredDistance(m_pData->row(i), m_pData->row(neighs[j]));
		}
	}
}

void WNeighborGraph::swapInData(const WMatrix* pNewData)
{
	m_pData = pNewData;
}

size_t WNeighborGraph::cutShortcuts(size_t cycleLen)
{
	WCycleCut cc(this, m_pData, m_neighs[0].size());
	cc.setCycleThreshold(cycleLen);
	return cc.cut();
}

bool WNeighborGraph::isConnected()
{
	// Make a table containing bi-directional neighbor connections
	vector< vector<size_t> > bidirTable;
	bidirTable.resize(m_pData->rows());
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		size_t nc = findNearest(0, i);
		vector<size_t>& row = bidirTable[i];
		for(size_t j = 0; j < nc; j++)
		{
			row.push_back(neighbor(j));
			bidirTable[neighbor(j)].push_back(i);
		}
	}

	// Use a breadth-first search to determine if the graph is fully connected
	WBitTable bt(m_pData->rows());
	deque<size_t> q;
	bt.set(0);
	q.push_back(0);
	size_t count = 1;
	while(q.size() > 0)
	{
		size_t n = q.front();
		q.pop_front();
		vector<size_t>& hood = bidirTable[n];
		for(vector<size_t>::iterator it = hood.begin(); it != hood.end(); it++)
		{
			if(!bt.bit(*it))
			{
				bt.set(*it);
				count++;
				if(count >= m_pData->rows())
					return true;
				q.push_back(*it);
			}
		}
	}
	return false;
}

void WNeighborGraph::set(size_t point, size_t neighbor_number, size_t neigh)
{
	m_neighs[point][neighbor_number] = neigh;
}

void WNeighborGraph::dropInvalidNeighbors()
{
	for(size_t i = 0; i < m_neighs.size(); i++)
	{
		for(size_t j = m_neighs[i].size() - 1; j < m_neighs[i].size(); j--)
		{
			if(m_neighs[i][j] == INVALID_INDEX)
			{
				m_neighs[i].erase(m_neighs[i].begin() + j);
				m_dists[i].erase(m_dists[i].begin() + j);
			}
		}
	}
}

// --------------------------------------------------------------------

/// This helper class keeps neighbors sorted as a binary heap, such that the most dissimilar
/// of the k-current-neighbors is always at the front of the heap.
class WClosestNeighborFindingHelper
{
protected:
	size_t m_neighbors;
	std::vector<size_t>& m_neighs;
	std::vector<double>& m_dists;

public:
	WClosestNeighborFindingHelper(size_t neighbors, std::vector<size_t>& neighs, std::vector<double>& dists)
	: m_neighbors(neighbors), m_neighs(neighs), m_dists(dists)
	{
		WAssert(m_neighbors >= 1);
		neighs.clear();
		dists.clear();
	}

	~WClosestNeighborFindingHelper()
	{
	}

	// Adds a point to the set of current neighbors if it is closer than the
	// most dissimilar of the k-current-neighbors
	void TryPoint(size_t index, double distance)
	{
		size_t heapPos;
		if(m_neighs.size() < m_neighbors)
		{
			m_neighs.push_back(index);
			m_dists.push_back(distance);
			heapPos = m_neighs.size();
		}
		else
		{
			// Compare with the front of the heap, which holds the most dissimilar of the k-current-neighbors
			if(distance >= m_dists[0])
				return;

			// Release the most dissimilar of the k-current neighbors
			heapPos = 1;
			while(2 * heapPos <= m_neighbors)
			{
				if(2 * heapPos == m_neighbors || m_dists[2 * heapPos - 1] > m_dists[2 * heapPos])
				{
					m_dists[heapPos - 1] = m_dists[2 * heapPos - 1];
					m_neighs[heapPos - 1] = m_neighs[2 * heapPos - 1];
					heapPos = 2 * heapPos;
				}
				else
				{
					m_dists[heapPos - 1] = m_dists[2 * heapPos];
					m_neighs[heapPos - 1] = m_neighs[2 * heapPos];
					heapPos = 2 * heapPos + 1;
				}
			}
			m_dists[heapPos - 1] = distance;
			m_neighs[heapPos - 1] = index;
		}

		// Insert into heap
		while(heapPos > 1 && m_dists[heapPos / 2 - 1] < m_dists[heapPos - 1])
		{
			std::swap(m_dists[heapPos / 2 - 1], m_dists[heapPos - 1]);
			std::swap(m_neighs[heapPos / 2 - 1], m_neighs[heapPos - 1]);
			heapPos /= 2;
		}
	}

	double GetWorstDist()
	{
		return m_dists.size() >= m_neighbors ? m_dists[0] : 1e308;
	}

#define TEST_NEIGHBOR_COUNT 33
	static void test()
	{
		vector<size_t> neighbors;
		vector<double> distances;
		WMatrix values1(0, 1);
		WMatrix values2(0, 1);
		WClosestNeighborFindingHelper ob(TEST_NEIGHBOR_COUNT, neighbors, distances);
		WRand prng(0);
		for(size_t i = 0; i < 300; i++)
		{
			double d = prng.uniform();
			ob.TryPoint(i, d);
			values1.newRow()[0] = d;
			values1.sort(0);
			values2.flush();
			for(size_t j = 0; j < std::min((size_t)TEST_NEIGHBOR_COUNT, values1.rows()); j++)
				values2.newRow()[0] = distances[j];
			values2.sort(0);
			for(size_t j = 0; j < std::min((size_t)TEST_NEIGHBOR_COUNT, values1.rows()); j++)
			{
				if(std::abs(values1[j][0] - values2[j][0]) > 1e-12)
					throw Ex("something is wrong");
			}
		}
	}
};

// --------------------------------------------------------------------------------

WNeighborFinderGeneralizing::WNeighborFinderGeneralizing(const WMatrix* pData, WDistanceMetric* pMetric, bool ownMetric)
: WNeighborFinder(pData), m_pMetric(pMetric), m_ownMetric(ownMetric)
{
	if(!m_pMetric)
	{
		m_pMetric = new WRowDistance();
		m_ownMetric = true;
	}
	m_pMetric->init(&pData->relation(), false);
}

// virtual
WNeighborFinderGeneralizing::~WNeighborFinderGeneralizing()
{
	if(m_ownMetric)
		delete(m_pMetric);
}

void WNeighborFinderGeneralizing::insertionSortNeighbors(size_t start, size_t end)
{
	for(size_t i = start + 1; i < end; i++)
	{
		for(size_t j = i; j > start; j--)
		{
			if(distance(j - 1) < distance(j))
				break;
			std::swap(m_neighs[j - 1], m_neighs[j]);
			std::swap(m_dists[j - 1], m_dists[j]);
		}
	}
}

void WNeighborFinderGeneralizing::sortNeighbors(size_t beg, size_t end)
{
	end = std::min(end, m_neighs.size());
	if(beg + 6 >= end)
	{
		insertionSortNeighbors(beg, end);
		return;
	}
	size_t initial_beg = beg;
	size_t initial_end = end;

	// Pick a pivot (using the median of 3 technique)
	double pivA = distance(0);
	double pivB = distance((beg + end) / 2);
	double pivC = distance(end - 1);
	double pivot;
	if(pivA < pivB)
	{
		if(pivB < pivC)
			pivot = pivB;
		else if(pivA < pivC)
			pivot = pivC;
		else
			pivot = pivA;
	}
	else
	{
		if(pivA < pivC)
			pivot = pivA;
		else if(pivB < pivC)
			pivot = pivC;
		else
			pivot = pivB;
	}

	// Do Quick Sort
	while(true)
	{
		while(beg + 1 < end && distance(beg) < pivot)
			beg++;
		while(end > beg + 1 && distance(end - 1) > pivot)
			end--;
		if(beg + 1 >= end)
			break;
		std::swap(m_dists[beg], m_dists[end - 1]);
		std::swap(m_neighs[beg], m_neighs[end - 1]);
		beg++;
		end--;
	}

	// Recurse
	if(distance(beg) < pivot)
		beg++;
	if(beg == initial_beg || end == initial_end)
	{
		insertionSortNeighbors(initial_beg, initial_end);
		return;
	}
	sortNeighbors(initial_beg, beg);
	sortNeighbors(beg, initial_end);
}






// --------------------------------------------------------------------------------

WBruteForceNeighborFinder::WBruteForceNeighborFinder(WMatrix* pData, WDistanceMetric* pMetric, bool ownMetric)
: WNeighborFinderGeneralizing(pData, pMetric, ownMetric)
{
}

WBruteForceNeighborFinder::~WBruteForceNeighborFinder()
{
}

// virtual
void WBruteForceNeighborFinder::reoptimize()
{
}

size_t WBruteForceNeighborFinder::findNearest(size_t k, const WVec& vec, size_t exclude)
{
	WClosestNeighborFindingHelper helper(k, m_neighs, m_dists);
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		if(i == exclude)
			continue;
		helper.TryPoint(i, m_pMetric->squaredDistance(vec, m_pData->row(i)));
	}
	return m_neighs.size();
}

size_t WBruteForceNeighborFinder::findWithinRadius(double squaredRadius, const WVec& vec, size_t exclude)
{
	m_neighs.clear();
	m_dists.clear();
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		if(i == exclude)
			continue;
		double d = m_pMetric->squaredDistance(vec, m_pData->row(i));
		if(d <= squaredRadius)
		{
			m_neighs.push_back(i);
			m_dists.push_back(d);
		}
	}
	return m_neighs.size();
}

// virtual
size_t WBruteForceNeighborFinder::findNearest(size_t k, const WVec& vec)
{
	return findNearest(k, vec, INVALID_INDEX);
}

// virtual
size_t WBruteForceNeighborFinder::findNearest(size_t k, size_t index)
{
	return findNearest(k, m_pData->row(index), index);
}

// virtual
size_t WBruteForceNeighborFinder::findWithinRadius(double squaredRadius, const WVec& vec)
{
	return findWithinRadius(squaredRadius, vec, INVALID_INDEX);
}

// virtual
size_t WBruteForceNeighborFinder::findWithinRadius(double squaredRadius, size_t index)
{
	return findWithinRadius(squaredRadius, m_pData->row(index), index);
}

// --------------------------------------------------------------------------------

WSparseNeighborFinder::WSparseNeighborFinder(WSparseMatrix* pData, WMatrix* pBogusData, WSparseSimilarity* pMetric, bool ownMetric)
: WNeighborFinderGeneralizing(pBogusData, new WRowDistance(), true),
  m_pData(pData),
  m_pSparseMetric(pMetric),
  m_ownSparseMetric(ownMetric)
{
}

WSparseNeighborFinder::~WSparseNeighborFinder()
{
	if(m_ownSparseMetric)
		delete(m_pSparseMetric);
}

// virtual
void WSparseNeighborFinder::reoptimize()
{
}

// virtual
size_t WSparseNeighborFinder::findNearest(size_t k, const WVec& vec)
{
	m_neighs.clear();
	m_dists.clear();
	multimap<double, size_t> priority_queue;
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		map<size_t, double>& row = m_pData->row(i);
		double similarity = m_pSparseMetric->similarity(row, vec);
		priority_queue.insert(pair<double, size_t>(similarity, i));
		if(priority_queue.size() > k)
			priority_queue.erase(priority_queue.begin());
	}
	for(multimap<double, size_t>::iterator it = priority_queue.begin(); it != priority_queue.end(); it++)
	{
		m_neighs.push_back(it->second);
		m_dists.push_back(1.0 / (it->first + 1e-6));
	}
	return m_neighs.size();
}

// virtual
size_t WSparseNeighborFinder::findNearest(size_t k, size_t index)
{
	map<size_t, double>& vec = m_pData->row(index);
	m_neighs.clear();
	m_dists.clear();
	multimap<double, size_t> priority_queue;
	for(size_t i = 0; i < m_pData->rows(); i++)
	{
		if(i == index)
			continue;
		map<size_t, double>& row = m_pData->row(i);
		double similarity = m_pSparseMetric->similarity(row, vec);
		priority_queue.insert(pair<double, size_t>(similarity, i));
		if(priority_queue.size() > k)
			priority_queue.erase(priority_queue.begin());
	}
	for(multimap<double, size_t>::iterator it = priority_queue.begin(); it != priority_queue.end(); it++)
	{
		m_neighs.push_back(it->second);
		m_dists.push_back(1.0 / (it->first + 1e-6));
	}
	return m_neighs.size();
}

// virtual
size_t WSparseNeighborFinder::findWithinRadius(double squaredRadius, const WVec& vec)
{
	throw Ex("Sorry, not implemented yet"); // SAKBox: was throw new, which catch (std::exception&) misses (and leaks)
}

// virtual
size_t WSparseNeighborFinder::findWithinRadius(double squaredRadius, size_t index)
{
	throw Ex("Sorry, not implemented yet"); // SAKBox: was throw new, which catch (std::exception&) misses (and leaks)
}

// --------------------------------------------------------------------------------

class WKdNode
{
protected:
	double m_minDist;
	double* m_pOffset;
	size_t m_dims;

public:
	WKdNode(size_t dims)
	{
		m_dims = dims;
		m_pOffset = new double[dims];
		WVecWrapper vw(m_pOffset, dims);
		vw.fill(0.0);
		m_minDist = 0;
	}

	virtual ~WKdNode()
	{
		delete[] m_pOffset;
	}

	virtual bool IsLeaf() = 0;

	// Builds an array of all the indexes in all of the leaf nodes that descend from me
	virtual size_t Gather(size_t* pOutIndexes) = 0;

	virtual void Insert(WKdTree* pTree, size_t index, double* pRow) = 0;

	virtual void Remove(WKdTree* pTree, size_t index, double* pRow) = 0;

	virtual void Rename(WKdTree* pTree, size_t oldIndex, size_t newIndex, double* pRow) = 0;

	double GetMinDist()
	{
		return m_minDist;
	}

	size_t GetDims()
	{
		return m_dims;
	}

	void CopyOffset(WKdNode* pParent)
	{
		memcpy(m_pOffset, pParent->m_pOffset, sizeof(double) * m_dims);
		m_minDist = pParent->m_minDist;
	}

	void AdjustOffset(size_t attr, double offset, const WVec& scaleFactors)
	{
		if(offset > m_pOffset[attr])
		{
			m_minDist -= (m_pOffset[attr] * m_pOffset[attr] * scaleFactors[attr] * scaleFactors[attr]);
			m_pOffset[attr] = offset;
			m_minDist += (m_pOffset[attr] * m_pOffset[attr] * scaleFactors[attr] * scaleFactors[attr]);
		}
	}
};


class WKdInteriorNode : public WKdNode
{
protected:
	WKdNode* m_pLess;
	WKdNode* m_pGreaterOrEqual;
	size_t m_size;
	size_t m_attr;
	double m_pivot;

public:
	size_t m_timeLeft;

	WKdInteriorNode(size_t dims, WKdNode* pLess, WKdNode* pGreaterOrEqual, size_t size, size_t attr, double pivot)
	: WKdNode(dims), m_pLess(pLess), m_pGreaterOrEqual(pGreaterOrEqual), m_size(size), m_attr(attr), m_pivot(pivot)
	{
		m_timeLeft = (size_t)std::min((double)0x7fffffff, ((double)size * size) / 36 + 6);
	}

	~WKdInteriorNode() override
	{
		delete(m_pLess);
		delete(m_pGreaterOrEqual);
	}

	bool IsLeaf() override { return false; }

	WKdNode* Rebuild(WKdTree* pTree)
	{
		size_t* pIndexes = new size_t[m_size];
		std::unique_ptr<size_t[]> hIndexes(pIndexes);
		size_t used = Gather(pIndexes);
		WAssert(used == m_size); // m_size is wrong. This may corrupt memory.
		return pTree->buildTree(used, pIndexes);
	}

	void Insert(WKdTree* pTree, size_t index, double* pRow) override
	{
		m_timeLeft--;
		if(pTree->isGreaterOrEqual(pRow, m_attr, m_pivot))
		{
			m_pGreaterOrEqual->Insert(pTree, index, pRow);
			m_size++;
			if(!m_pGreaterOrEqual->IsLeaf() && ((WKdInteriorNode*)m_pGreaterOrEqual)->m_timeLeft == 0 && m_timeLeft >= m_size / 4)
			{
				WKdNode* pNewNode = ((WKdInteriorNode*)m_pGreaterOrEqual)->Rebuild(pTree);
				delete(m_pGreaterOrEqual);
				m_pGreaterOrEqual = pNewNode;
			}
		}
		else
		{
			m_pLess->Insert(pTree, index, pRow);
			m_size++;
			if(!m_pLess->IsLeaf() && ((WKdInteriorNode*)m_pLess)->m_timeLeft == 0 && m_timeLeft >= m_size / 4)
			{
				WKdNode* pNewNode = ((WKdInteriorNode*)m_pLess)->Rebuild(pTree);
				delete(m_pLess);
				m_pLess = pNewNode;
			}
		}
	}

	void Remove(WKdTree* pTree, size_t index, double* pRow) override
	{
		m_timeLeft--;
		if(pTree->isGreaterOrEqual(pRow, m_attr, m_pivot))
			m_pGreaterOrEqual->Remove(pTree, index, pRow);
		else
			m_pLess->Remove(pTree, index, pRow);
		m_size--;
	}

	void Rename(WKdTree* pTree, size_t oldIndex, size_t newIndex, double* pRow) override
	{
		if(pTree->isGreaterOrEqual(pRow, m_attr, m_pivot))
			m_pGreaterOrEqual->Rename(pTree, oldIndex, newIndex, pRow);
		else
			m_pLess->Rename(pTree, oldIndex, newIndex, pRow);
	}

	WKdNode* GetLess() { return m_pLess; }
	WKdNode* GetGreaterOrEqual() { return m_pGreaterOrEqual; }
	size_t GetSize() { return m_size; }

	void GetDivision(size_t* pAttr, double* pPivot)
	{
		*pAttr = m_attr;
		*pPivot = m_pivot;
	}

	size_t Gather(size_t* pOutIndexes) override
	{
		size_t n = m_pLess->Gather(pOutIndexes);
		return m_pGreaterOrEqual->Gather(pOutIndexes + n) + n;
	}
};


class WKdLeafNode : public WKdNode
{
protected:
	vector<size_t> m_indexes;

public:
	WKdLeafNode(size_t count, size_t* pIndexes, size_t dims, size_t maxLeafSize)
	: WKdNode(dims)
	{
		m_indexes.reserve(std::max(count, maxLeafSize));
		for(size_t i = 0; i < count; i++)
			m_indexes.push_back(pIndexes[i]);
	}

	~WKdLeafNode() override
	{
	}

	bool IsLeaf() override { return true; }

	size_t GetSize()
	{
		return m_indexes.size();
	}

	void Insert(WKdTree* pTree, size_t index, double* pRow) override
	{
		m_indexes.push_back(index);
	}

	void Remove(WKdTree* pTree, size_t index, double* pRow) override
	{
		size_t count = m_indexes.size();
		for(size_t i = 0; i < count; i++)
		{
			if(m_indexes[i] == index)
			{
				m_indexes[i] = m_indexes[count - 1];
				m_indexes.pop_back();
				return;
			}
		}
		WAssert(false); // failed to find index. Did the row change?
	}

	void Rename(WKdTree* pTree, size_t oldIndex, size_t newIndex, double* pRow) override
	{
		size_t count = m_indexes.size();
		for(size_t i = 0; i < count; i++)
		{
			if(m_indexes[i] == oldIndex)
			{
				m_indexes[i] = newIndex;
				return;
			}
		}
		WAssert(false); // failed to find index. Did the row change?
	}

	vector<size_t>* GetIndexes() { return &m_indexes; }

	size_t Gather(size_t* pOutIndexes) override
	{
		for(vector<size_t>::iterator i = m_indexes.begin(); i < m_indexes.end(); i++)
		{
			*pOutIndexes = *i;
			pOutIndexes++;
		}
		return m_indexes.size();
	}
};


// --------------------------------------------------------------------------------------------------------

WKdTree::WKdTree(const WMatrix* pData, WDistanceMetric* pMetric, bool ownMetric)
: WNeighborFinderGeneralizing(pData, pMetric, ownMetric)
{
	m_maxLeafSize = 6;
	size_t count = pData->rows();
	WTEMPBUF(size_t, tmp, count);
	for(size_t i = 0; i < count; i++)
		tmp[i] = i;
	m_pRoot = buildTree(count, tmp);
}

// virtual
WKdTree::~WKdTree()
{
	delete(m_pRoot);
}

void WKdTree::computePivotAndGoodness(size_t count, size_t* pIndexes, size_t attr, double* pOutPivot, double* pOutGoodness)
{
	size_t valueCount = m_pMetric->relation()->valueCount(attr);
	if(valueCount > 0)
	{
		// Count the ocurrences of each value
		WTEMPBUF(size_t, counts, valueCount);
		memset(counts, '\0', sizeof(size_t) * valueCount);
		for(size_t i = 0; i < count; i++)
		{
			const WVec& pPat = m_pData->row(pIndexes[i]);
			if((int)pPat[attr] >= 0)
			{
				WAssert((unsigned int)pPat[attr] < (unsigned int)valueCount); // out of range
				if((unsigned int)pPat[attr] < (unsigned int)valueCount)
					counts[(int)pPat[attr]]++;
			}
		}

		// Total up the entropy
		size_t max = 0;
		size_t maxcount = 0;
		double entropy = 0;
		double ratio;
		for(size_t i = 0; i < valueCount; i++)
		{
			if(counts[i] > maxcount || i == 0)
			{
				maxcount = counts[i];
				max = i;
			}
			if(counts[i] > 0)
			{
				ratio = (double)counts[i] / count;
				entropy -= ratio * log(ratio);
			}
		}
		const WVec& pScaleFactors = m_pMetric->scaleFactors();
		entropy *= (pScaleFactors[attr] * pScaleFactors[attr]);

		*pOutPivot = (double)max;
		*pOutGoodness = entropy;
	}
	else
	{
		// Compute the mean
		size_t missing = 0;
		double mean = 0;
		for(size_t i = 0; i < count; i++)
		{
			WAssert(pIndexes[i] < m_pData->rows());
			const WVec& pPat = m_pData->row(pIndexes[i]);
			if(pPat[attr] != UNKNOWN_REAL_VALUE)
				mean += pPat[attr];
			else
				missing++;
		}
		mean /= (count - missing);

		// Compute the scaled variance
		double var = 0;
		double d;
		const WVec& scaleFactors = m_pMetric->scaleFactors();
		for(size_t i = 0; i < count; i++)
		{
			const WVec& pPat = m_pData->row(pIndexes[i]);
			if(pPat[attr] != UNKNOWN_REAL_VALUE)
			{
				d = (pPat[attr] - mean) * scaleFactors[attr];
				var += (d * d);
			}
		}
		var /= (count - missing); // (the biased estimator of variance is better for this purpose)

		*pOutPivot = mean;
		*pOutGoodness = var;
	}
}

size_t WKdTree::splitIndexes(size_t count, size_t* pIndexes, size_t attr, double pivot)
{
	size_t t;
	size_t beg = 0;
	size_t end = count - 1;
	if(m_pMetric->relation()->valueCount(attr) == 0)
	{
		while(end >= beg && end < count)
		{
			const WVec& pPat = m_pData->row(pIndexes[beg]);
			if(pPat[attr] >= pivot)
			{
				t = pIndexes[beg];
				pIndexes[beg] = pIndexes[end];
				pIndexes[end] = t;
				end--;
			}
			else
				beg++;
		}
	}
	else
	{
		while(end >= beg && end < count)
		{
			const WVec& pPat = m_pData->row(pIndexes[beg]);
			if((int)pPat[attr] == (int)pivot)
			{
				t = pIndexes[beg];
				pIndexes[beg] = pIndexes[end];
				pIndexes[end] = t;
				end--;
			}
			else
				beg++;
		}
	}
	return beg;
}

// static
bool WKdTree::isGreaterOrEqual(const double* pPat, size_t attr, double pivot)
{
	if(m_pMetric->relation()->valueCount(attr) == 0)
		return (pPat[attr] >= pivot);
	else
		return ((int)pPat[attr] == (int)pivot);
}

WKdNode* WKdTree::buildTree(size_t count, size_t* pIndexes)
{
	size_t dims = m_pMetric->relation()->size();
	if(count <= (size_t)m_maxLeafSize)
		return new WKdLeafNode(count, pIndexes, dims, m_maxLeafSize);

	// Find a good place to split
	double pivot, goodness, p, g;
	size_t attr = 0;
	computePivotAndGoodness(count, pIndexes, 0, &pivot, &goodness);
	for(size_t i = 1; i < dims; i++)
	{
		computePivotAndGoodness(count, pIndexes, i, &p, &g);
		if(g > goodness)
		{
			pivot = p;
			goodness = g;
			attr = i;
		}
	}

	// Split the data
	size_t lessCount = splitIndexes(count, pIndexes, attr, pivot);
	size_t greaterOrEqualCount = count - lessCount;
	if(lessCount == 0 || greaterOrEqualCount == 0)
		return new WKdLeafNode(count, pIndexes, dims, m_maxLeafSize);

	// Make an interior node
	WKdNode* pLess = buildTree(lessCount, pIndexes);
	WKdNode* greaterOrEqual = buildTree(greaterOrEqualCount, pIndexes + lessCount);
	return new WKdInteriorNode(dims, pLess, greaterOrEqual, count, attr, pivot);
}

class KdTree_Compare_Nodes_Functor
{
public:
	bool operator()(WKdNode* pA, WKdNode* pB) const
	{
		double a = pA->GetMinDist();
		double b = pB->GetMinDist();
		return (a > b);
	}
};

size_t WKdTree::findNearest(size_t k, const WVec& vec, size_t nExclude)
{
	WClosestNeighborFindingHelper helper(k, m_neighs, m_dists);
	KdTree_Compare_Nodes_Functor comparator;
	priority_queue< WKdNode*, vector<WKdNode*>, KdTree_Compare_Nodes_Functor > q(comparator);
	q.push(m_pRoot);
	while(q.size() > 0)
	{
		WKdNode* pNode = q.top();
		q.pop();
		if(pNode->GetMinDist() >= helper.GetWorstDist())
			break;
		if(pNode->IsLeaf())
		{
			double squaredDist;
			vector<size_t>* pIndexes = ((WKdLeafNode*)pNode)->GetIndexes();
			size_t count = pIndexes->size();
			for(size_t i = 0; i < count; i++)
			{
				size_t index = (*pIndexes)[i];
				if(index == nExclude)
					continue;
				const WVec& pCand = m_pData->row(index);
				squaredDist = m_pMetric->squaredDistance(vec, pCand);
				helper.TryPoint(index, squaredDist);
			}
		}
		else
		{
			size_t attr;
			double pivot;
			WKdInteriorNode* pParent = (WKdInteriorNode*)pNode;
			pParent->GetDivision(&attr, &pivot);
			WKdNode* pLess = pParent->GetLess();
			pLess->CopyOffset(pParent);
			WKdNode* pGreaterOrEqual = pParent->GetGreaterOrEqual();
			pGreaterOrEqual->CopyOffset(pParent);
			if(isGreaterOrEqual(vec.data(), attr, pivot))
				pLess->AdjustOffset(attr, vec[attr] - pivot, m_pMetric->scaleFactors());
			else
				pGreaterOrEqual->AdjustOffset(attr, pivot - vec[attr], m_pMetric->scaleFactors());
			q.push(pLess);
			q.push(pGreaterOrEqual);
		}
	}
	return m_neighs.size();
}

size_t WKdTree::findWithinRadius(double squaredRadius, const WVec& vec, size_t nExclude)
{
	m_neighs.clear();
	m_dists.clear();
	KdTree_Compare_Nodes_Functor comparator;
	priority_queue< WKdNode*, vector<WKdNode*>, KdTree_Compare_Nodes_Functor > q(comparator);
	q.push(m_pRoot);
	while(q.size() > 0)
	{
		WKdNode* pNode = q.top();
		q.pop();
		if(pNode->GetMinDist() > squaredRadius)
			break;
		if(pNode->IsLeaf())
		{
			double squaredDist;
			vector<size_t>* pIndexes = ((WKdLeafNode*)pNode)->GetIndexes();
			size_t count = pIndexes->size();
			for(size_t i = 0; i < count; i++)
			{
				size_t index = (*pIndexes)[i];
				if(index == nExclude)
					continue;
				const WVec& pCand = m_pData->row(index);
				squaredDist = m_pMetric->squaredDistance(vec, pCand);
				if(squaredDist <= squaredRadius)
				{
					m_neighs.push_back(index);
					m_dists.push_back(squaredDist);
				}
			}
		}
		else
		{
			size_t attr;
			double pivot;
			WKdInteriorNode* pParent = (WKdInteriorNode*)pNode;
			pParent->GetDivision(&attr, &pivot);
			WKdNode* pLess = pParent->GetLess();
			pLess->CopyOffset(pParent);
			WKdNode* pGreaterOrEqual = pParent->GetGreaterOrEqual();
			pGreaterOrEqual->CopyOffset(pParent);
			if(isGreaterOrEqual(vec.data(), attr, pivot))
				pLess->AdjustOffset(attr, vec[attr] - pivot, m_pMetric->scaleFactors());
			else
				pGreaterOrEqual->AdjustOffset(attr, pivot - vec[attr], m_pMetric->scaleFactors());
			q.push(pLess);
			q.push(pGreaterOrEqual);
		}
	}
	return m_neighs.size();
}

// virtual
size_t WKdTree::findNearest(size_t k, size_t index)
{
	return findNearest(k, m_pData->row(index), index);
}

// virtual
size_t WKdTree::findNearest(size_t k, const WVec& vec)
{
	return findNearest(k, vec, INVALID_INDEX);
}

// virtual
size_t WKdTree::findWithinRadius(double squaredRadius, size_t index)
{
	return findWithinRadius(squaredRadius, m_pData->row(index), index);
}

// virtual
size_t WKdTree::findWithinRadius(double squaredRadius, const WVec& vector)
{
	return findWithinRadius(squaredRadius, vector, INVALID_INDEX);
}

// virtual
void WKdTree::reoptimize()
{
	if(!m_pRoot->IsLeaf())
	{
		WKdNode* pNewNode = ((WKdInteriorNode*)m_pRoot)->Rebuild(this);
		delete(m_pRoot);
		m_pRoot = pNewNode;
	}
}

// static
double WKdTree::medianDistanceToNeighbor(WMatrix& data, size_t n)
{
	if(n < 1)
		return 0.0; // 0 is the point itself

	// Fill a vector with the distances to the n^th neighbor of each point
	WKdTree kdtree(&data, nullptr, false);
	vector<double> vals;
	vals.reserve(data.rows());
	for(size_t i = 0; i < data.rows(); i++)
	{
		size_t nc = kdtree.findNearest(n, i);
		kdtree.sortNeighbors();
		vals.push_back(sqrt(kdtree.distance(nc - 1)));
	}

	// Find the median value
	if(vals.size() < 1)
		throw Ex("at least one value is required to compute a median");
	if(vals.size() & 1)
	{
		vector<double>::iterator med = vals.begin() + (vals.size() / 2);
		std::nth_element(vals.begin(), med, vals.end());
		return *med;
	}
	else
	{
		vector<double>::iterator a = vals.begin() + (vals.size() / 2 - 1);
		std::nth_element(vals.begin(), a, vals.end());
		vector<double>::iterator b = std::min_element(a + 1, vals.end());
		return 0.5 * (*a + *b);
	}
}


//#	include "WImage.h"

void MeasureBounds(WMatrix* pData, WKdNode* pNode, size_t attr, double* pMin, double* pMax)
{
	if(pNode->IsLeaf())
	{
		double min = 1e200;
		double max = -1e200;
		vector<size_t>* pIndexes = ((WKdLeafNode*)pNode)->GetIndexes();
		for(size_t i = 0; i < pIndexes->size(); i++)
		{
			WVec& pPat = pData->row((*pIndexes)[i]);
			min = std::min(pPat[attr], min);
			max = std::max(pPat[attr], max);
		}
		*pMin = min;
		*pMax = max;
	}
	else
	{
		double min1, min2, max1, max2;
		WKdNode* pChild = ((WKdInteriorNode*)pNode)->GetLess();
		MeasureBounds(pData, pChild, attr, &min1, &max1);
		pChild = ((WKdInteriorNode*)pNode)->GetGreaterOrEqual();
		MeasureBounds(pData, pChild, attr, &min2, &max2);
		*pMin = std::min(min1, min2);
		*pMax = std::max(max1, max2);
	}
}

void DrawKdNode(WPlotWindow* pw, WKdNode* pNode, WMatrix* pData)
{
	if(pNode->IsLeaf())
	{
		vector<size_t>* pIndexes = ((WKdLeafNode*)pNode)->GetIndexes();
		for(size_t i = 0; i < pIndexes->size(); i++)
		{
			WVec& pPat = pData->row((*pIndexes)[i]);
			pw->dot(pPat[0], pPat[1], 5, 0xff00ff00, 0xff000000);
			std::ostringstream os;
			os << (*pIndexes)[i];
			string tmp = os.str();
			pw->label(pPat[0], pPat[1], tmp.c_str(), 1.0f, 0xffffffff);
		}
	}
	else
	{
		size_t attr;
		double pivot, min, max;
		((WKdInteriorNode*)pNode)->GetDivision(&attr, &pivot);
		if(attr == 0)
		{
			MeasureBounds(pData, pNode, 1, &min, &max);
			pw->line(pivot, min, pivot, max, 0xffff0000);
		}
		else
		{
			WAssert(attr == 1); // unsupported value
			MeasureBounds(pData, pNode, 0, &min, &max);
			pw->line(min, pivot, max, pivot, 0xffff0000);
		}
		WKdNode* pChild = ((WKdInteriorNode*)pNode)->GetLess();
		DrawKdNode(pw, pChild, pData);
		pChild = ((WKdInteriorNode*)pNode)->GetGreaterOrEqual();
		DrawKdNode(pw, pChild, pData);
	}
}

class WDontGoFarMetric : public WDistanceMetric
{
public:
	double m_squaredMaxDist;

	WDontGoFarMetric(double maxDist)
	: WDistanceMetric(), m_squaredMaxDist(maxDist * maxDist)
	{
	}

	~WDontGoFarMetric() override
	{
	}

	const char* name() const override { return "WDontGoFarMetric"; }

	WDomNode* serialize(WDom* pDoc) const override
	{
		throw Ex("not implemented");
		return nullptr;
	}

	void init(const WRelation* pRelation, bool own) override
	{
		setRelation(pRelation, own);
	}

	double squaredDistance(const WVec& pA, const WVec& pB) const override
	{
		double squaredDist = pA.squaredDistance(pB);
		if(squaredDist > m_squaredMaxDist)
			throw Ex("a kd-tree shouldn't have to look this far away");
		return squaredDist;
	}
};

void WKdTree_testThatItDoesntLookFar()
{
	WRand prng(0);
	WMatrix tmp(100000, 2);
	for(size_t i = 0; i < tmp.rows(); i++)
	{
		WVec& pRow = tmp[i];
		pRow[0] = prng.uniform();
		pRow[1] = prng.uniform();
	}
	WDontGoFarMetric metric(0.05);
	WKdTree kdTree(&tmp, &metric, false);
	WVec row(2);
	for(size_t i = 0; i < 100; i++)
	{
		row[0] = prng.uniform();
		row[1] = prng.uniform();
		kdTree.findNearest(5, row);
	}
}

#define TEST_DIMS 4
#define TEST_PATTERNS 1000
#define TEST_NEIGHBORS 24

// static
void WKdTree::test()
{
	WClosestNeighborFindingHelper::test();
	WKdTree_testThatItDoesntLookFar();

	// Test neighbor sorting
	{
		WRand rand(0);
		WMatrix data(256, 1);
		for(size_t i = 0; i < data.rows(); i++)
			data[i][0] = rand.uniform();
		data.sort(0);
		WRandomIndexIterator ii(data.rows(), rand);
		WKdTree kd(&data);
		size_t index;
		kd.m_neighs.clear();
		kd.m_dists.clear();
		ii.reset();
		while(ii.next(index))
		{
			kd.m_neighs.push_back(index);
			kd.m_dists.push_back(data[index][0]);
		}
		kd.sortNeighbors();
		for(size_t i = 0; i < data.rows(); i++)
		{
			if(kd.m_neighs[i] != i || kd.m_dists[i] != data[i][0])
				throw Ex("wrong answer");
		}
	}


	WMatrix data(new WUniformRelation(TEST_DIMS, 0));
	data.reserve(TEST_PATTERNS);
	WRand prng(0);
	for(size_t i = 0; i < TEST_PATTERNS; i++)
	{
		WVec& pPat = data.newRow();
		pPat.fillNormal(prng);
		pPat.normalize();
	}
	WBruteForceNeighborFinder bf(&data);
	WKdTree kd(&data);
	/*
	WAssert(TEST_DIMS == 2); // You must change TEST_DIMS to 2 if you're going to plot the tree
	WImage image;
	image.SetSize(1000, 1000);
	image.Clear(0xff000000);
	WPlotWindow pw(&image, -1.1, -1.1, 1.1, 1.1);
	DrawKdNode(&pw, kd.GetRoot(), &data);
	image.SavePNGFile("kdtree.png");
*/
	for(size_t i = 0; i < TEST_PATTERNS; i++)
	{
		size_t ncbf = bf.findNearest(TEST_NEIGHBORS, i);
		bf.sortNeighbors();
		size_t nckd = kd.findNearest(TEST_NEIGHBORS, i);
		kd.sortNeighbors();
		if(ncbf != nckd)
			throw Ex("found different number of neighbors");
		if(ncbf != TEST_NEIGHBORS)
			throw Ex("found unexpected number of neighbors");
		for(size_t j = 0; j < TEST_DIMS; j++)
		{
			if(bf.neighbor(j) != kd.neighbor(j))
				throw Ex("wrong answer!");
			if(j > 0 && kd.distance(j) < kd.distance(j - 1))
				throw Ex("Neighbors out of order");
		}
	}
}

// --------------------------------------------------------------------------------------------------------









class WBallNode
{
public:
	WVec m_center;
	double m_radius;

	WBallNode(size_t count, size_t* pIndexes, const WMatrix* pData, WDistanceMetric* pMetric)
	: m_center(pData->cols())
	{
		m_radius = sqrt(pData->boundingSphere(m_center, pIndexes, count, pMetric));
	}

	virtual ~WBallNode()
	{
	}

	virtual bool isLeaf() = 0;

	double distance(WDistanceMetric* pMetric, const WVec& pVec)
	{
		return sqrt(pMetric->squaredDistance(m_center, pVec)) - m_radius;
	}

	/// Move the center and radius just enough to enclose both pVec and the previous ball
	void enclose(WDistanceMetric* pMetric, const WVec& pVec)
	{
		double d = distance(pMetric, pVec) + 1e-9;
		if(d > 0)
		{
			size_t dims = pMetric->relation()->size();
			double s = 0.5 * d / (m_radius + d);
			for(size_t i = 0; i < dims; i++)
				m_center[i] += s * (pVec[i] - m_center[i]);
			m_radius += 0.5 * d;
		}
	}

	virtual void insert(size_t index, WDistanceMetric* pMetric, const WVec& pVec) = 0;

	virtual bool drop(size_t index, WDistanceMetric* pMetric, const WVec& pVec) = 0;

	virtual void dropAll() = 0;

	virtual void print(std::ostream& stream, size_t depth, WDistanceMetric* pMetric, const WVec& pVec, const WMatrix* pData) = 0;
};

class WBallInterior : public WBallNode
{
public:
	WBallNode* m_pLeft;
	WBallNode* m_pRight;

	WBallInterior(size_t count, size_t* pIndexes, const WMatrix* pData, WDistanceMetric* pMetric)
	: WBallNode(count, pIndexes, pData, pMetric), m_pLeft(nullptr), m_pRight(nullptr)
	{
	}

	~WBallInterior() override
	{
		delete(m_pLeft);
		delete(m_pRight);
	}

	bool isLeaf() override { return false; }

	void insert(size_t index, WDistanceMetric* pMetric, const WVec& pVec) override
	{
		enclose(pMetric, pVec);
		if(m_pLeft->distance(pMetric, pVec) < m_pRight->distance(pMetric, pVec))
			m_pLeft->insert(index, pMetric, pVec);
		else
			m_pRight->insert(index, pMetric, pVec);
	}

	bool drop(size_t index, WDistanceMetric* pMetric, const WVec& pVec) override
	{
		if(m_pLeft->distance(pMetric, pVec) <= 0.0)
		{
			if(m_pLeft->drop(index, pMetric, pVec))
				return true;
		}
		if(m_pRight->distance(pMetric, pVec) <= 0.0)
		{
			if(m_pRight->drop(index, pMetric, pVec))
				return true;
		}
		return false;
	}

	void dropAll() override
	{
		m_pLeft->dropAll();
		m_pRight->dropAll();
	}

	void print(std::ostream& stream, size_t depth, WDistanceMetric* pMetric, const WVec& pVec, const WMatrix* pData) override
	{
		m_pRight->print(stream, depth + 1, pMetric, pVec, pData);
		for(size_t i = 0; i < depth; i++)
			stream << "  ";
		stream << to_str(distance(pMetric, pVec));
		stream << "\n";
		m_pLeft->print(stream, depth + 1, pMetric, pVec, pData);
	}
};

class WBallLeaf : public WBallNode
{
public:
	std::vector<size_t> m_indexes;

	WBallLeaf(size_t count, size_t* pIndexes, const WMatrix* pData, WDistanceMetric* pMetric)
	: WBallNode(count, pIndexes, pData, pMetric)
	{
		m_indexes.reserve(count);
		for(size_t i = 0; i < count; i++)
			m_indexes.push_back(pIndexes[i]);
	}

	~WBallLeaf() override
	{
	}

	bool isLeaf() override { return true; }

	void insert(size_t index, WDistanceMetric* pMetric, const WVec& pVec) override
	{
		enclose(pMetric, pVec);
		m_indexes.push_back(index);
	}

	bool drop(size_t index, WDistanceMetric* pMetric, const WVec& pVec) override
	{
		for(size_t i = 0; i < m_indexes.size(); i++)
		{
			if(m_indexes[i] == index)
			{
				std::swap(m_indexes[i], m_indexes[m_indexes.size() - 1]);
				m_indexes.pop_back();
				return true;
			}
		}
		return false;
	}

	void dropAll() override
	{
		m_indexes.clear();
	}

	void print(std::ostream& stream, size_t depth, WDistanceMetric* pMetric, const WVec& pVec, const WMatrix* pData) override
	{
		for(size_t i = 0; i < depth; i++)
			stream << "  ";
		stream << to_str(distance(pMetric, pVec)) << "\n";
		for(size_t j = 0; j < m_indexes.size(); j++)
		{
			for(size_t i = 0; i <= depth; i++)
				stream << "  ";
			size_t index = m_indexes[j];
			double val = pData->row(index)[0];
			stream << to_str(index) << " (" << to_str(val) << ")\n";
		}
	}
};


WBallTree::WBallTree(const WMatrix* pData, WDistanceMetric* pMetric, bool ownMetric)
: WNeighborFinderGeneralizing(pData, pMetric, ownMetric),
  m_maxLeafSize(6),
  m_pRoot(nullptr)
{
	reoptimize();
}

// virtual
WBallTree::~WBallTree()
{
	delete(m_pRoot);
}

void WBallTree::reoptimize()
{
	if(m_pRoot && m_pRoot->isLeaf())
		return;
	delete(m_pRoot);
	WIndexVec indexes(m_pData->rows());
	WIndexVec::makeIndexVec(indexes.m_data, m_pData->rows());
	m_pRoot = buildTree(m_pData->rows(), indexes.m_data);
	m_size = m_pData->rows();
}

// virtual
size_t WBallTree::findNearest(size_t k, size_t index)
{
	return findNearest(k, m_pData->row(index), index);
}

// virtual
size_t WBallTree::findNearest(size_t k, const WVec& vec)
{
	return findNearest(k, vec, INVALID_INDEX);
}

// virtual
size_t WBallTree::findWithinRadius(double squaredRadius, size_t index)
{
	return findWithinRadius(squaredRadius, m_pData->row(index), index);
}

// virtual
size_t WBallTree::findWithinRadius(double squaredRadius, const WVec& vec)
{
	return findWithinRadius(squaredRadius, vec, INVALID_INDEX);
}

WBallNode* WBallTree::buildTree(size_t count, size_t* pIndexes)
{
	if(count > m_maxLeafSize)
	{
		// Find the two farthest points
		const WVec& pA = m_pData->row(pIndexes[0]);
		size_t b = 1;
		double sdist = m_pMetric->squaredDistance(pA, m_pData->row(pIndexes[b]));
		for(size_t i = 2; i < count; i++)
		{
			double cand = m_pMetric->squaredDistance(pA, m_pData->row(pIndexes[i]));
			if(cand > sdist)
			{
				sdist = cand;
				b = i;
			}
		}
		const WVec& pB = m_pData->row(pIndexes[b]);
		size_t c = 0;
		sdist = m_pMetric->squaredDistance(pB, m_pData->row(pIndexes[c]));
		for(size_t i = 1; i < count; i++)
		{
			if(i == b)
				continue;
			double cand = m_pMetric->squaredDistance(pB, m_pData->row(pIndexes[i]));
			if(cand > sdist)
			{
				sdist = cand;
				c = i;
			}
		}
		const WVec& pC = m_pData->row(pIndexes[c]);

		// Split based on closeness to b or c
		size_t leftCount = 0;
		for(size_t i = 0; i < count; i++)
		{
			double dB = m_pMetric->squaredDistance(m_pData->row(pIndexes[i]), pB);
			double dC = m_pMetric->squaredDistance(m_pData->row(pIndexes[i]), pC);
			if(dB < dC)
			{
				std::swap(pIndexes[leftCount], pIndexes[i]);
				leftCount++;
			}
		}
		if(leftCount == 0 || leftCount == count) // If we could not separate any of the points (which may occur if they are all the same point)...
			return new WBallLeaf(count, pIndexes, m_pData, m_pMetric);
		WBallInterior* pInterior = new WBallInterior(count, pIndexes, m_pData, m_pMetric);
		std::unique_ptr<WBallInterior> hInterior(pInterior);
		pInterior->m_pLeft = buildTree(leftCount, pIndexes);
		WAssert(pInterior->m_pLeft);
		pInterior->m_pRight = buildTree(count - leftCount, pIndexes + leftCount);
		WAssert(pInterior->m_pRight);
		return hInterior.release();
	}
	else
		return new WBallLeaf(count, pIndexes, m_pData, m_pMetric);
}


size_t WBallTree::findNearest(size_t k, const WVec& vec, size_t nExclude)
{
	WClosestNeighborFindingHelper helper(k, m_neighs, m_dists);
	WSimplePriorityQueue<WBallNode*> q;
	q.insert(m_pRoot, m_pRoot->distance(m_pMetric, vec));
	while(q.size() > 0)
	{
		double dist = q.peekValue();
		if(helper.GetWorstDist() < dist * dist)
			break;
		WBallNode* pBall = q.peekObject();
		q.pop();
		if(pBall->isLeaf())
		{
			WBallLeaf* pLeaf = (WBallLeaf*)pBall;
			for(size_t i = 0; i < pLeaf->m_indexes.size(); i++)
			{
				size_t index = pLeaf->m_indexes[i];
				if(index != nExclude)
					helper.TryPoint(pLeaf->m_indexes[i], m_pMetric->squaredDistance(m_pData->row(index), vec));
			}
		}
		else
		{
			WBallInterior* pInt = (WBallInterior*)pBall;
			q.insert(pInt->m_pLeft, pInt->m_pLeft->distance(m_pMetric, vec));
			q.insert(pInt->m_pRight, pInt->m_pRight->distance(m_pMetric, vec));
		}
	}
	return m_neighs.size();
}

size_t WBallTree::findWithinRadius(double squaredRadius, const WVec& vec, size_t nExclude)
{
	m_neighs.clear();
	m_dists.clear();
	WSimplePriorityQueue<WBallNode*> q;
	q.insert(m_pRoot, m_pRoot->distance(m_pMetric, vec));
	while(q.size() > 0)
	{
		double dist = q.peekValue();
		if(dist * dist > squaredRadius)
			break;
		WBallNode* pBall = q.peekObject();
		q.pop();
		if(pBall->isLeaf())
		{
			WBallLeaf* pLeaf = (WBallLeaf*)pBall;
			for(size_t i = 0; i < pLeaf->m_indexes.size(); i++)
			{
				size_t index = pLeaf->m_indexes[i];
				if(index != nExclude)
				{
					double d = m_pMetric->squaredDistance(m_pData->row(index), vec);
					if(d <= squaredRadius)
					{
						m_neighs.push_back(pLeaf->m_indexes[i]);
						m_dists.push_back(d);
					}
				}
			}
		}
		else
		{
			WBallInterior* pInt = (WBallInterior*)pBall;
			q.insert(pInt->m_pLeft, pInt->m_pLeft->distance(m_pMetric, vec));
			q.insert(pInt->m_pRight, pInt->m_pRight->distance(m_pMetric, vec));
		}
	}
	return m_neighs.size();
}

void WBallTree::insert(size_t index)
{
	m_pRoot->insert(index, m_pMetric, m_pData->row(index));
	m_size++;
}

void WBallTree::drop(size_t index)
{
	if(!m_pRoot->drop(index, m_pMetric, m_pData->row(index)))
		throw Ex("Could not find the specified index in this structure. (This could happen if the corresponding point in the dataset has changed.)");
	m_size--;
}

void WBallTree::dropAll()
{
	m_pRoot->dropAll();
	m_size = 0;
}

#define TEST_BALLTREE_ITERS 100
#define TEST_BALLTREE_ROWS 200
#define TEST_BALLTREE_DIMS 7
#define TEST_BALLTREE_NEIGHBORS 17
// static
void WBallTree::test()
{
	WRand r(0);
	for(size_t i = 0; i < TEST_BALLTREE_ITERS; i++)
	{
		WMatrix m(TEST_BALLTREE_ROWS, TEST_BALLTREE_DIMS);
		for(size_t j = 0; j < TEST_BALLTREE_ROWS; j++)
			m[j].fillUniform(r);
		WKdTree kd(&m);
		WBallTree ball(&m);
		size_t nckd = kd.findNearest(TEST_BALLTREE_NEIGHBORS, 0);
		kd.sortNeighbors();
		size_t ncbt = ball.findNearest(TEST_BALLTREE_NEIGHBORS, 0);
		ball.sortNeighbors();
		if(nckd != ncbt)
			throw Ex("found different number of neighbors");
		for(size_t j = 0; j < TEST_BALLTREE_NEIGHBORS; j++)
		{
			if(kd.neighbor(j) != ball.neighbor(j))
			{
				/*
				std::cout << "\nIter: " << to_str(i) << "\n";
				std::cout << "Matrix:\n";
				m.print(std::cout);
				std::cout << "kdTree:\n";
				WIndexVec::print(std::cout, kdInd, TEST_BALLTREE_NEIGHBORS);
				std::cout << "\nballTree:\n";
				WIndexVec::print(std::cout, ballInd, TEST_BALLTREE_NEIGHBORS);
				std::cout << "\nkdTree distances:\n";
				WVec::print(std::cout, 8, kdDist, TEST_BALLTREE_NEIGHBORS);
				std::cout << "\nballTree distances:\n";
				WVec::print(std::cout, 8, ballDist, TEST_BALLTREE_NEIGHBORS);
				std::cout << "\nTree:\n";
				ball.m_pRoot->print(std::cout, 0, ball.m_pMetric, m[0], &m);
				*/
				throw Ex("indexes differ");
			}
			if(kd.distance(j) != ball.distance(j))
				throw Ex("distances differ");
		}
	}
}













class WShortcutPrunerAtomicCycleDetector : public WAtomicCycleFinder
{
protected:
	WShortcutPruner* m_pThis;
	size_t m_thresh;

public:
	WShortcutPrunerAtomicCycleDetector(size_t nodes, WShortcutPruner* pThis, size_t thresh) : WAtomicCycleFinder(nodes), m_pThis(pThis), m_thresh(thresh)
	{
	}

	~WShortcutPrunerAtomicCycleDetector() override
	{
	}

	bool onDetectAtomicCycle(vector<size_t>& cycle) override
	{
		if(cycle.size() >= (size_t)m_thresh)
		{
			m_pThis->onDetectBigAtomicCycle(cycle);
			return false;
		}
		else
			return true;
	}
};

WShortcutPruner::WShortcutPruner(size_t* pNeighborhoods, size_t n, size_t k)
: m_pNeighborhoods(pNeighborhoods), m_n(n), m_k(k), m_cycleThresh(10), m_subGraphRange(6), m_cuts(0)
{
}

WShortcutPruner::~WShortcutPruner()
{
}

bool WShortcutPruner::isEveryNodeReachable()
{
	WBitTable visited(m_n);
	deque<size_t> q;
	visited.set(0);
	q.push_back(0);
	while(q.size() > 0)
	{
		size_t cur = q.front();
		q.pop_front();
		for(size_t j = 0; j < m_k; j++)
		{
			size_t neigh = m_pNeighborhoods[m_k * cur + j];
			if(neigh < m_n && !visited.bit(neigh))
			{
				visited.set(neigh);
				q.push_back(neigh);
			}
		}
	}
	for(size_t i = 0; i < m_n; i++)
	{
		if(!visited.bit(i))
			return false;
	}
	return true;
}

size_t WShortcutPruner::prune()
{
	while(true)
	{
		bool everyNodeReachable = isEveryNodeReachable();
		WShortcutPrunerAtomicCycleDetector g(m_n, this, m_cycleThresh);
		size_t* pHood = m_pNeighborhoods;
		for(size_t i = 0; i < m_n; i++)
		{
			for(size_t j = 0; j < m_k; j++)
			{
				if(pHood[j] < m_n)
					g.addEdgeIfNotDupe(i, pHood[j]);
			}
			pHood += m_k;
		}
		size_t oldCuts = m_cuts;
		g.compute();
		if(everyNodeReachable && !isEveryNodeReachable())
			throw Ex("Cutting shortcuts should not segment the graph");
		if(m_cuts == oldCuts)
			break;
	}
	return m_cuts;
}

void WShortcutPruner::onDetectBigAtomicCycle(vector<size_t>& cycle)
{
	// Make a subgraph containing only nodes close to the cycle
	size_t* mapIn = new size_t[m_n];
	std::unique_ptr<size_t[]> hMapIn(mapIn);
	vector<size_t> mapOut;
	WBitTable visited(m_n);
	deque<size_t> q;
	for(vector<size_t>::iterator it = cycle.begin(); it != cycle.end(); it++)
	{
		q.push_back(*it);
		q.push_back(1);
	}
	while(q.size() > 0)
	{
		size_t cur = q.front();
		q.pop_front();
		size_t depth = q.front();
		q.pop_front();
		mapIn[cur] = mapOut.size();
		mapOut.push_back(cur);
		if(depth <= (size_t)m_subGraphRange)
		{
			for(size_t j = 0; j < m_k; j++)
			{
				size_t neigh = m_pNeighborhoods[cur * m_k + j];
				if(neigh < m_n && !visited.bit(neigh))
				{
					visited.set(neigh);
					q.push_back(neigh);
					q.push_back(depth + 1);
				}
			}
		}
	}

	// Compute betweenness of all edges
	WBrandesBetweennessCentrality g(mapOut.size());
	for(size_t i = 0; i < mapOut.size(); i++)
	{
		size_t* pHood = m_pNeighborhoods + mapOut[i] * m_k;
		for(size_t j = 0; j < m_k; j++)
		{
			size_t neigh = pHood[j];
			if(neigh < m_n && visited.bit(neigh))
			{
				g.addDirectedEdgeIfNotDupe(i, mapIn[neigh]);
				g.addDirectedEdgeIfNotDupe(mapIn[neigh], i);
			}
		}
	}
	g.compute();

	// Find the edge on the cycle with the largest betweenness
	size_t shortcutFrom = 0;
	size_t shortcutTo = 0;
	double shortcutBetweenness = 0;
	for(size_t i = 0; i < cycle.size(); i++)
	{
		size_t from = cycle[i];
		size_t to = cycle[(i + 1) % cycle.size()];
		size_t forwIndex = g.neighborIndex(mapIn[from], mapIn[to]);
		size_t revIndex = g.neighborIndex(mapIn[to], mapIn[from]);
		double d = g.edgeBetweennessByNeighbor(mapIn[from], forwIndex) + g.edgeBetweennessByNeighbor(mapIn[to], revIndex);
		if(i == 0 || d > shortcutBetweenness)
		{
			shortcutBetweenness = d;
			shortcutFrom = from;
			shortcutTo = to;
		}
	}

	// Cut the shortcut
	bool cutForward = false;
	for(size_t j = 0; j < m_k; j++)
	{
		if(m_pNeighborhoods[shortcutFrom * m_k + j] == shortcutTo)
		{
			m_pNeighborhoods[shortcutFrom * m_k + j] = INVALID_INDEX;
			cutForward = true;
			m_cuts++;
			break;
		}
	}
	bool cutReverse = false;
	for(size_t j = 0; j < m_k; j++)
	{
		if(m_pNeighborhoods[shortcutTo * m_k + j] == shortcutFrom)
		{
			m_pNeighborhoods[shortcutTo * m_k + j] = INVALID_INDEX;
			cutReverse = true;
			m_cuts++;
			break;
		}
	}
	if(!cutForward && !cutReverse)
		throw Ex("Failed to find the offending edge");
}

// static
void WShortcutPruner::test()
{
	// Make a fully-connected grid
	size_t w = 6;
	size_t h = 6;
	size_t n = w * h;
	size_t k = 4;
	size_t* pNeighbors = new size_t[n * k];
	std::unique_ptr<size_t[]> hNeighbors(pNeighbors);
	size_t i = 0;
	size_t* pHood = pNeighbors;
	for(size_t y = 0; y < h; y++)
	{
		for(size_t x = 0; x < w; x++)
		{
			size_t j = 0;
			pHood[j++] = (x > 0 ? i - 1 : INVALID_INDEX);
			pHood[j++] = (x < w - 1 ? i + 1 : INVALID_INDEX);
			pHood[j++] = (y > 0 ? i - w : INVALID_INDEX);
			pHood[j++] = (y < h - 1 ? i + w : INVALID_INDEX);
			pHood += k;
			i++;
		}
	}

	// Add 3 shortcuts
	pNeighbors[(0 * w + 0) * k + 0] = n - 1; // connect (0,0) to (w-1, h-1)
	pNeighbors[(0 * w + (w - 1)) * k + 1] = n - 1; // connect (w-1,0) to (w-1,h-1)
	pNeighbors[((h - 1) * w + (w - 1)) * k + 0] = w - 1; // connect (w-1,h-1) to (w-1,0)

	// Cut the shortcuts
	WShortcutPruner pruner(pNeighbors, n, k);
	pruner.setCycleThreshold(h);
	pruner.setSubGraphRange(3);
	size_t cuts = pruner.prune();
	if(pNeighbors[(0 * w + 0) * k + 0] != INVALID_INDEX)
		throw Ex("missed a shortcut");
	if(pNeighbors[(0 * w + (w - 1)) * k + 1] != INVALID_INDEX)
		throw Ex("missed a shortcut");
	if(pNeighbors[((h - 1) * w + (w - 1)) * k + 0] != INVALID_INDEX)
		throw Ex("missed a shortcut");
	if(cuts != 3)
		throw Ex("wrong number of cuts");
}











class WCycleCutAtomicCycleDetector : public WAtomicCycleFinder
{
protected:
	WCycleCut* m_pThis;
	size_t m_thresh;
	bool m_restore;
	bool m_gotOne;

public:
	WCycleCutAtomicCycleDetector(size_t nodes, WCycleCut* pThis, size_t thresh, bool restore) : WAtomicCycleFinder(nodes), m_pThis(pThis), m_thresh(thresh), m_restore(restore), m_gotOne(false)
	{
	}

	~WCycleCutAtomicCycleDetector() override
	{
	}

	bool gotOne() { return m_gotOne; }

	bool onDetectAtomicCycle(vector<size_t>& cycle) override
	{
		if(cycle.size() >= (size_t)m_thresh)
		{
			if(m_restore)
				m_gotOne = true;
			else
				m_pThis->onDetectBigAtomicCycle(cycle);
			return false;
		}
		else
			return true;
	}
};

WCycleCut::WCycleCut(WNeighborGraph* pNeighborGraph, const WMatrix* pPoints, size_t k)
: m_pNeighborGraph(pNeighborGraph), m_pPoints(pPoints), m_k(k), m_cycleThresh(10), m_cutCount(0)
{
	// Compute the mean neighbor distance
	size_t count = 0;
	double sum = 0;
	for(size_t i = 0; i < m_pPoints->rows(); i++)
	{
		size_t nc = pNeighborGraph->findNearest(m_k, i);
		for(size_t j = 0; j < nc; j++)
			sum += sqrt(pNeighborGraph->distance(j));
		count += nc;
	}
	m_aveDist = sum / count;

	// Compute the capacities
	for(size_t i = 0; i < m_pPoints->rows(); i++)
	{
		size_t nc = pNeighborGraph->findNearest(m_k, i);
		for(size_t j = 0; j < nc; j++)
		{
			double cap = 1.0 / (m_aveDist + sqrt(pNeighborGraph->distance(j)));
			m_capacities[make_pair(i, pNeighborGraph->neighbor(j))] = cap;
			m_capacities[make_pair(pNeighborGraph->neighbor(j), i)] = cap;
		}
	}
}

WCycleCut::~WCycleCut()
{
}

bool WCycleCut::doAnyBigAtomicCyclesExist()
{
	// Make the graph
	WCycleCutAtomicCycleDetector g(m_pPoints->rows(), this, m_cycleThresh, true);
	for(size_t i = 0; i < m_pPoints->rows(); i++)
	{
		size_t nc = m_pNeighborGraph->findNearest(m_k, i);
		for(size_t j = 0; j < nc; j++)
			g.addEdgeIfNotDupe(i, m_pNeighborGraph->neighbor(j));
	}

	// Find a large atomic cycle (calls onDetectBigAtomicCycle when found)
	g.compute();
	return g.gotOne();
}

size_t WCycleCut::cut()
{
	m_cuts.clear();

	// Cut the graph
	while(true)
	{
		// Make the graph
		WCycleCutAtomicCycleDetector g(m_pPoints->rows(), this, m_cycleThresh, false);
		for(size_t i = 0; i < m_pPoints->rows(); i++)
		{
			size_t nc = m_pNeighborGraph->findNearest(m_k, i);
			for(size_t j = 0; j < nc; j++)
				g.addEdgeIfNotDupe(i, m_pNeighborGraph->neighbor(j));
		}

		// Find a large atomic cycle (calls onDetectBigAtomicCycle when found)
		size_t oldCuts = m_cutCount;
		g.compute();
		if(m_cutCount == oldCuts)
			break;
	}

	// Restore superfluous cuts
	for(vector<size_t>::iterator it = m_cuts.begin(); it != m_cuts.end();)
	{
		size_t point = *it;
		it++;
		WAssert(it != m_cuts.end()); // broken cuts list
		size_t neigh = *it;
		it++;
		WAssert(it != m_cuts.end()); // broken cuts list
		size_t other = *it;
		it++;

		// Restore the edge if it doesn't create a big atomic cycle
		m_pNeighborGraph->set(point, neigh, other);
		if(!doAnyBigAtomicCyclesExist())
			m_cutCount--;
		else
			m_pNeighborGraph->set(point, neigh, INVALID_INDEX);
	}
	//cerr << "cuts: " << m_cutCount << "\n";

	m_pNeighborGraph->dropInvalidNeighbors();
	return m_cutCount;
}

void WCycleCut::onDetectBigAtomicCycle(vector<size_t>& cycle)
{
	// Find the bottleneck
	double bottleneck = 1e308;
	for(size_t i = 0; i < cycle.size(); i++)
	{
		size_t from = cycle[i];
		size_t to = cycle[(i + 1) % cycle.size()];
		pair<size_t, size_t> p = make_pair(from, to);
		double d = m_capacities[p];
		if(i == 0 || d < bottleneck)
			bottleneck = d;
	}
	WAssert(bottleneck > 0); // all capacities should be greater than zero

	// Reduce every edge in the cycle by the bottleneck's capacity
	for(size_t i = 0; i < cycle.size(); i++)
	{
		size_t from = cycle[i];
		size_t to = cycle[(i + 1) % cycle.size()];
		pair<size_t, size_t> p1 = make_pair(from, to);
		pair<size_t, size_t> p2 = make_pair(to, from);
		double d = m_capacities[p1];
		if(d - bottleneck > 1e-12)
		{
			// Reduce the capacity
			m_capacities[p1] = d - bottleneck;
			m_capacities[p2] = d - bottleneck;
		}
		else
		{
			// Remove the edge
			m_capacities.erase(p1);
			m_capacities.erase(p2);
			size_t forw = INVALID_INDEX;
			size_t nc = m_pNeighborGraph->findNearest(m_k, from);
			for(size_t j = 0; j < nc; j++)
			{
				if(m_pNeighborGraph->neighbor(j) == to)
				{
					forw = j;
					break;
				}
			}
			size_t rev = INVALID_INDEX;
			nc = m_pNeighborGraph->findNearest(m_k, to);
			for(size_t j = 0; j < nc; j++)
			{
				if(m_pNeighborGraph->neighbor(j) == from)
				{
					rev = j;
					break;
				}
			}
			WAssert(rev != INVALID_INDEX || forw != INVALID_INDEX); // couldn't find the edge
			if(forw != INVALID_INDEX)
			{
				m_pNeighborGraph->set(from, forw, INVALID_INDEX);
				m_cuts.push_back(from);
				m_cuts.push_back(forw);
				m_cuts.push_back(to);
				m_cutCount++;
			}
			if(rev != INVALID_INDEX)
			{
				m_pNeighborGraph->set(to, rev, INVALID_INDEX);
				m_cuts.push_back(to);
				m_cuts.push_back(rev);
				m_cuts.push_back(from);
				m_cutCount++;
			}
		}
	}
}

// static
void WCycleCut::test()
{
	// todo: This test was removed because I didn't want to port it when I made some API changes.
	// I believe WCycleCut still works, but I really should write a new test to exercise it.
}




} // namespace WClasses


