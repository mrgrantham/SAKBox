/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or pay it forward in their own field. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "WNaiveInstance.h"
#include "WVec.h"
#include "WDom.h"
#include "WDistribution.h"
#include "WTransform.h"
#include <map>

using std::multimap;
using std::make_pair;

namespace WClasses
{

class WNaiveInstanceAttr
{
protected:
	multimap<double, const double*> m_instances;

public:
	WNaiveInstanceAttr() {}

	WNaiveInstanceAttr(WDomNode* pAttr, size_t labelDims, WHeap* pHeap)
	{
		WDomListIterator it(pAttr);
		size_t count = it.remaining() / (1 + labelDims);
		if(count * (1 + labelDims) != it.remaining())
			throw Ex("invalid list size");
		for(size_t i = 0; i < count; i++)
		{
			double d = it.currentDouble();
			it.advance();
			double* pLabel = (double*)pHeap->allocAligned(sizeof(double) * labelDims);
			m_instances.insert(make_pair(d, pLabel));
			for(size_t j = 0; j < labelDims; j++)
			{
				*(pLabel++) = it.currentDouble();
				it.advance();
			}
		}
	}

	virtual ~WNaiveInstanceAttr()
	{
	}

	multimap<double, const double*>& instances() { return m_instances; }

	WDomNode* serialize(WDom* pDoc, size_t labelDims)
	{
		WDomNode* pList = pDoc->newList();
		for(multimap<double, const double*>::iterator it = m_instances.begin(); it != m_instances.end(); it++)
		{
			pList->add(pDoc, it->first);
			for(size_t i = 0; i < labelDims; i++)
				pList->add(pDoc, it->second[i]);
		}
		return pList;
	}

	void addInstance(double dInput, const double* pOutputs)
	{
		m_instances.insert(make_pair(dInput, pOutputs));
	}
};

// -----------------------------------------------------------

WNaiveInstance::WNaiveInstance()
: WIncrementalLearner(), m_pHeap(nullptr)
{
	m_nNeighbors = 12;
	m_pAttrs = nullptr;
}

WNaiveInstance::WNaiveInstance(const WDomNode* pNode)
: WIncrementalLearner(pNode), m_pHeap(nullptr)
{
	m_pAttrs = nullptr;
	m_nNeighbors = (size_t)pNode->getInt("neighbors");
	beginIncrementalLearningInner(*m_pRelFeatures, *m_pRelLabels);
	WDomNode* pAttrs = pNode->get("attrs");
	WDomListIterator it(pAttrs);
	if(it.remaining() != m_pRelFeatures->size())
		throw Ex("Expected ", to_str(m_pRelFeatures->size()), " attrs, got ", to_str(it.remaining()), " attrs");
	m_pHeap = new WHeap(1024);
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
	{
		delete(m_pAttrs[i]);
		m_pAttrs[i] = new WNaiveInstanceAttr(it.current(), m_pRelLabels->size(), m_pHeap);
		it.advance();
	}
}

// virtual
WNaiveInstance::~WNaiveInstance()
{
	clear();
}

void WNaiveInstance::clear()
{
	if(m_pAttrs)
	{
		for(size_t i = 0; i < m_pRelFeatures->size(); i++)
			delete(m_pAttrs[i]);
		delete[] m_pAttrs;
	}
	m_pAttrs = nullptr;
	m_pValueSums.resize(0);
	delete(m_pHeap);
	m_pHeap = nullptr;
}

// virtual
WDomNode* WNaiveInstance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WNaiveInstance");
	pNode->add(pDoc, "neighbors", m_nNeighbors);
	WDomNode* pAttrs = pNode->add(pDoc, "attrs", pDoc->newList());
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
		pAttrs->add(pDoc, m_pAttrs[i]->serialize(pDoc, m_pRelLabels->size()));
	return pNode;
}

void WNaiveInstance::autoTune(WMatrix& features, WMatrix& labels)
{
	// Find the best ess value
	size_t bestK = 0;
	double bestErr = 1e308;
	size_t cap = size_t(floor(sqrt(double(features.rows()))));
	for(size_t i = 2; i < cap; i = size_t(i * 1.5))
	{
		m_nNeighbors = i;
		double d = crossValidate(features, labels, 2);
		if(d < bestErr)
		{
			bestErr = d;
			bestK = i;
		}
		else if(i >= 15)
			break;
	}

	// Set the best values
	m_nNeighbors = bestK;
}

// virtual
void WNaiveInstance::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	if(!featureRel.areContinuous() || !labelRel.areContinuous())
		throw Ex("Only continuous attributes are supported.");
	clear();
	m_pAttrs = new WNaiveInstanceAttr*[m_pRelFeatures->size()];
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
		m_pAttrs[i] = new WNaiveInstanceAttr();
	m_pValueSums.resize(m_pRelLabels->size());
	m_pWeightSums.resize(m_pRelLabels->size());
	m_pSumBuffer.resize(m_pRelLabels->size());
	m_pSumOfSquares.resize(m_pRelLabels->size());
}

// virtual
void WNaiveInstance::trainIncremental(const WVec& pIn, const WVec& pOut)
{
	if(!m_pHeap)
		m_pHeap = new WHeap(1024);
	double* pOutputs = (double*)m_pHeap->allocAligned(sizeof(double) * m_pRelLabels->size());
	memcpy(pOutputs, pOut.data(), sizeof(double) * m_pRelLabels->size());
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
	{
		if(pIn[i] != UNKNOWN_REAL_VALUE)
			m_pAttrs[i]->addInstance(pIn[i], pOutputs);
	}
}

// virtual
void WNaiveInstance::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(!features.relation().areContinuous())
		throw Ex("WNaiveInstance only supports continuous features. Perhaps you should wrap it in a WAutoFilter.");
	if(!labels.relation().areContinuous())
		throw Ex("WNaiveInstance only supports continuous labels. Perhaps you should wrap it in a WAutoFilter.");

	beginIncrementalLearningInner(features.relation(), labels.relation());
	for(size_t i = 0; i < features.rows(); i++)
		trainIncremental(features[i], labels[i]);
}

// virtual
void WNaiveInstance::trainSparse(WSparseMatrix& features, WMatrix& labels)
{
	throw Ex("Sorry, trainSparse is not implemented yet in WNaiveInstance");
}

void WNaiveInstance::evalInput(size_t nInputDim, double dInput)
{
	// Init the accumulators
	m_pSumBuffer.fill(0.0);
	m_pSumOfSquares.fill(0.0);

	// Find the nodes on either side of dInput
	WNaiveInstanceAttr* pAttr = m_pAttrs[nInputDim];
	multimap<double, const double*>& instances = pAttr->instances();
	multimap<double, const double*>::iterator itLeft = instances.lower_bound(dInput);
	multimap<double, const double*>::iterator itRight = itLeft;
	bool leftValid = true;
	if(itLeft == instances.end())
	{
		if(instances.size() > 0)
			itLeft--;
		else
			leftValid = false;
	}
	else
		itRight++;

	// Compute the mean and variance of the values for the k-nearest neighbors
	size_t nNeighbors = 0;
	bool goRight;
	while(true)
	{
		// Pick the closer of the two nodes
		if(!leftValid)
		{
			if(itRight == instances.end())
				break;
			goRight = true;
		}
		else // go right if there is a right neighbour, unless the left one is closer
			goRight = itRight != instances.end() && !(dInput - itLeft->first < itRight->first - dInput);

		// Accumulate values
		const double* pOutputVec = goRight ? itRight->second : itLeft->second;
		WConstVecWrapper vw(pOutputVec, m_pSumBuffer.size());
		m_pSumBuffer += vw;
		for(size_t j = 0; j < m_pRelLabels->size(); j++)
			m_pSumOfSquares[j] += (pOutputVec[j] * pOutputVec[j]);

		// See if we're done
		if(++nNeighbors >= m_nNeighbors)
			break;

		// Advance
		if(goRight)
			itRight++;
		else
		{
			if(itLeft == instances.begin())
				leftValid = false;
			else
				itLeft--;
		}
	}
	m_pSumBuffer *= (1.0 / nNeighbors);
	m_pSumOfSquares *= (1.0 / nNeighbors);

	// Accumulate the predictions across all dimensions
	int dims = 0;
	double weight;
	for(size_t i = 0; i < m_pRelLabels->size(); i++)
	{
		weight = 1.0 / std::max(m_pSumOfSquares[i] - (m_pSumBuffer[i] * m_pSumBuffer[i]), 1e-5);
		m_pWeightSums[dims] += weight;
		m_pValueSums[dims] += weight * m_pSumBuffer[dims];
		dims++;
	}
}

// virtual
void WNaiveInstance::predictDistribution(const WVec& pIn, WPrediction* pOut)
{
	m_pWeightSums.fill(0.0);
	m_pValueSums.fill(0.0);
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
		evalInput(i, pIn[i]);
	for(size_t i = 0; i < m_pRelLabels->size(); i++)
	{
		WNormalDistribution* pNorm = pOut[i].makeNormal();
		pNorm->setMeanAndVariance(m_pValueSums[i] / m_pWeightSums[i], 1.0 / m_pWeightSums[i]);
	}
}

// virtual
void WNaiveInstance::predict(const WVec& pIn, WVec& pOut)
{
	m_pWeightSums.fill(0.0);
	m_pValueSums.fill(0.0);
	for(size_t i = 0; i < m_pRelFeatures->size(); i++)
		evalInput(i, pIn[i]);
	for(size_t i = 0; i < m_pRelLabels->size(); i++)
		pOut[i] = m_pValueSums[i] / m_pWeightSums[i];
}

//static
void WNaiveInstance::test()
{

	WNaiveInstance* pNI = new WNaiveInstance();
	WAutoFilter af(pNI);
	pNI->setNeighbors(8);
	af.basicTest(0.72, 0.44, 0.02);
}

} // namespace WClasses
