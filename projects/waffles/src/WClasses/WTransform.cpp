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

#include "WTransform.h"
#include "WDom.h"
#include "WVec.h"
#include "WDistribution.h"
#include "WRand.h"
#include "WManifold.h"
#include "WCluster.h"
#include "WString.h"
#include "WNeuralNet.h"
#include "WRecommender.h"
#include "WHolders.h"
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <math.h>
#include <string>
#include <cmath>
#include <memory>

namespace WClasses
{

using std::string;
using std::vector;
using std::ostringstream;

WTransform::WTransform()
{
}

WTransform::WTransform(const WDomNode* pNode)
{
}

// virtual
WTransform::~WTransform()
{
}

// virtual
WDomNode* WTransform::baseDomNode(WDom* pDoc, const char* szClassName) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "class", szClassName);
	return pNode;
}

// ---------------------------------------------------------------

WIncrementalTransform::WIncrementalTransform(const WDomNode* pNode)
: WTransform(pNode)
{
	m_pRelationBefore = WRelation::deserialize(pNode->get("before"));
	m_pRelationAfter = WRelation::deserialize(pNode->get("after"));
}

// virtual
WIncrementalTransform::~WIncrementalTransform()
{
	delete(m_pRelationBefore);
	delete(m_pRelationAfter);
}

// virtual
WDomNode* WIncrementalTransform::baseDomNode(WDom* pDoc, const char* szClassName) const
{
	if(!m_pRelationAfter)
		throw Ex("train must be called before serialize");
	WDomNode* pNode = WTransform::baseDomNode(pDoc, szClassName);
	pNode->add(pDoc, "before", m_pRelationBefore->serialize(pDoc));
	pNode->add(pDoc, "after", m_pRelationAfter->serialize(pDoc));
	return pNode;
}

void WIncrementalTransform::setBefore(WRelation* pRel)
{
	delete(m_pRelationBefore);
	m_pRelationBefore = pRel;
}

void WIncrementalTransform::setAfter(WRelation* pRel)
{
	delete(m_pRelationAfter);
	m_pRelationAfter = pRel;
}

void WIncrementalTransform::train(const WMatrix& data)
{
	setBefore(data.relation().clone());
	setAfter(trainInner(data));
}

void WIncrementalTransform::train(const WRelation& relation)
{
	setBefore(relation.clone());
	setAfter(trainInner(relation));
}

// virtual
WMatrix* WIncrementalTransform::reduce(const WMatrix& in)
{
	train(in);
	return transformBatch(in);
}

// virtual
WMatrix* WIncrementalTransform::transformBatch(const WMatrix& in)
{
	if(!m_pRelationAfter)
		throw Ex("train has not been called");
	size_t nRows = in.rows();
	WMatrix* pOut = new WMatrix(m_pRelationAfter->clone());
	std::unique_ptr<WMatrix> hOut(pOut);
	pOut->newRows(nRows);
	for(size_t i = 0; i < nRows; i++)
		transform(in.row(i), pOut->row(i));
	return hOut.release();
}

WVec& WIncrementalTransform::innerBuf()
{
	if(!m_pRelationAfter)
		throw Ex("Attempted to use a transform that was not trained");
	m_innerBuf.resize(m_pRelationAfter->size());
	return m_innerBuf;
}

// virtual
std::unique_ptr<WMatrix> WIncrementalTransform::untransformBatch(const WMatrix& in)
{
	if(!m_pRelationBefore)
		throw Ex("train has not been called");
	size_t nRows = in.rows();
	auto pOut = std::unique_ptr<WMatrix>(new WMatrix(before().clone()));
	pOut->newRows(nRows);
	for(size_t i = 0; i < nRows; i++)
		untransform(in.row(i), pOut->row(i));
	return pOut;
}

//static
void WIncrementalTransform::test()
{
	// Make an input matrix
	vector<size_t> valCounts;
	valCounts.push_back(0);
	valCounts.push_back(1);
	valCounts.push_back(2);
	valCounts.push_back(3);
	valCounts.push_back(0);
	WMatrix m(valCounts);
	m.newRows(2);
	m[0][0] = 2.4;
	m[0][1] = 0;
	m[0][2] = 0;
	m[0][3] = 2;
	m[0][4] = 8.2;
	m[1][0] = 0.0;
	m[1][1] = 0;
	m[1][2] = 1;
	m[1][3] = 0;
	m[1][4] = 2.2;

	// Make an expected output matrix
	WMatrix e(2, 7);
	e[0][0] = 1;
	e[0][1] = 0;
	e[0][2] = 0;
	e[0][3] = 0;
	e[0][4] = 0;
	e[0][5] = 1;
	e[0][6] = 1;
	e[1][0] = 0;
	e[1][1] = 0;
	e[1][2] = 1;
	e[1][3] = 1;
	e[1][4] = 0;
	e[1][5] = 0;
	e[1][6] = 0;

	// Transform the input matrix and check it
	WIncrementalTransformChainer trans(new WNormalize(), new WNominalToCat());
	trans.train(m);
	WMatrix* pA = trans.transformBatch(m);
	std::unique_ptr<WMatrix> hA(pA);
	if(pA->sumSquaredDifference(e) > 1e-12)
		throw Ex("Expected:\n", to_str(e), "\nGot:\n", to_str(*pA));
	if(!pA->relation().areContinuous())
		throw Ex("failed");
	auto pB = trans.untransformBatch(*pA);
	if(pB->sumSquaredDifference(m) > 1e-12)
		throw Ex("Expected:\n", to_str(m), "\nGot:\n", to_str(*pB));
	if(!pB->relation().isCompatible(m.relation()) || !m.relation().isCompatible(pB->relation()))
		throw Ex("failed");

	// Round-trip it through serialization
	WDom doc;
	WDomNode* pNode = trans.serialize(&doc);
	WRand rand(0);
	WLearnerLoader ll;
	WIncrementalTransform* pTrans = ll.loadIncrementalTransform(pNode);
	std::unique_ptr<WIncrementalTransform> hTrans(pTrans);

	// Transform the input matrix again, and check it
	WMatrix* pC = pTrans->transformBatch(m);
	std::unique_ptr<WMatrix> hC(pC);
	if(pC->sumSquaredDifference(e) > 1e-12)
		throw Ex("Expected:\n", to_str(e), "\nGot:\n", to_str(*pC));
	if(!pC->relation().areContinuous())
		throw Ex("failed");
	auto pD = trans.untransformBatch(*pC);
	if(pD->sumSquaredDifference(m) > 1e-12)
		throw Ex("Expected:\n", to_str(m), "\nGot:\n", to_str(*pD));
	if(!pD->relation().isCompatible(m.relation()) || !m.relation().isCompatible(pD->relation()))
		throw Ex("failed");
}






WDataPreprocessor::WDataPreprocessor(const WMatrix& source, size_t rowStart, size_t colStart, size_t rowCount, size_t colCount, bool allowMissing, bool allowNominal, bool allowContinuous, double minVal, double maxVal)
{
	if(rowStart > 0 || colStart > 0 || rowCount < source.rows() || colCount < source.cols())
	{
		WMatrix* pTemp = new WMatrix(source, rowStart, colStart, rowCount, colCount);
		m_pTransform = autoTrans(*pTemp, allowMissing, allowNominal, allowContinuous, minVal, maxVal);
		if(m_pTransform)
		{
			m_processedData.push_back(m_pTransform->transformBatch(*pTemp));
			delete(pTemp);
		}
		else
			m_processedData.push_back(pTemp);
	}
	else
	{
		m_pTransform = autoTrans(source, allowMissing, allowNominal, allowContinuous, minVal, maxVal);
		if(m_pTransform)
			m_processedData.push_back(m_pTransform->transformBatch(source));
		else
			m_processedData.push_back(new WMatrix(source));
	}
}

void WDataPreprocessor::add(const WMatrix& source, size_t rowStart, size_t colStart, size_t rowCount, size_t colCount)
{
	if(rowStart > 0 || colStart > 0 || rowCount < source.rows() || colCount < source.cols())
	{
		WMatrix* pTemp = new WMatrix(source, rowStart, colStart, rowCount, colCount);
		if(m_pTransform)
		{
			m_processedData.push_back(m_pTransform->transformBatch(*pTemp));
			delete(pTemp);
		}
		else
			m_processedData.push_back(pTemp);
	}
	else
	{
		if(m_pTransform)
			m_processedData.push_back(m_pTransform->transformBatch(source));
		else
			m_processedData.push_back(new WMatrix(source));
	}
}

WIncrementalTransform* WDataPreprocessor::autoTrans(const WMatrix& data, bool allowMissing, bool allowNominal, bool allowContinuous, double minVal, double maxVal)
{
	// Determine what types are present in the data
	bool hasNominal = false;
	bool hasContinuous = false;
	const WRelation& rel = data.relation();
	for(size_t i = 0; i < rel.size(); i++)
	{
		if(rel.valueCount(i) == 0)
		{
			hasContinuous = true;
			if(hasNominal)
				break;
		}
		else
		{
			hasNominal = true;
			if(hasContinuous)
				break;
		}
	}

	WIncrementalTransform* pTrans = nullptr;

	// Impute features if necessary
	if(!allowMissing && data.doesHaveAnyMissingValues())
		pTrans = WIncrementalTransformChainer::chain(new WImputeMissingVals(), pTrans);

	// Normalize labels if necessary
	if(allowContinuous)
	{
		// Find the min and max
		double dataMin = 1e300;
		double dataMax = -1e300;
		for(size_t i = 0; i < data.cols(); i++)
		{
			if(data.relation().valueCount(i) != 0)
				continue;
			dataMin = std::min(data.columnMin(i), dataMin);
			dataMax = std::max(data.columnMax(i), dataMax);
		}
		if(hasNominal && !allowNominal)
		{
			minVal = std::min(0.0, minVal);
			maxVal = std::max(1.0, maxVal);
		}
		bool outOfRange = dataMin < minVal || dataMax > maxVal;
		bool usesLittleOfRange = (dataMax - dataMin) >= 1e-12 && (dataMax - dataMin) * 4 < maxVal - minVal;
		if(outOfRange || usesLittleOfRange)
			pTrans = WIncrementalTransformChainer::chain(new WNormalize(minVal, maxVal), pTrans);
	}

	// Nomcat if necessary
	if(!allowNominal && hasNominal)
	{
		if(!allowContinuous)
			throw Ex("Either nominal or continuous values must be allowed");
		pTrans = WIncrementalTransformChainer::chain(new WNominalToCat(16), pTrans);
	}

	// Discretize if necessary
	if(!allowContinuous && hasContinuous)
	{
		if(!allowNominal)
			throw Ex("Either nominal or continuous values must be allowed");
		pTrans = WIncrementalTransformChainer::chain(new WDiscretize(), pTrans);
	}

	// Train
	if(pTrans)
		pTrans->train(data);

	return pTrans;
}











WIncrementalTransformChainer::WIncrementalTransformChainer(WIncrementalTransform* pFirst, WIncrementalTransform* pSecond)
: WIncrementalTransform(), m_pFirst(pFirst), m_pSecond(pSecond)
{
}

WIncrementalTransformChainer::WIncrementalTransformChainer(const WDomNode* pNode, WLearnerLoader& ll)
: WIncrementalTransform(pNode)
{
	m_pFirst = ll.loadIncrementalTransform(pNode->get("first"));
	m_pSecond = ll.loadIncrementalTransform(pNode->get("second"));
}

// virtual
WIncrementalTransformChainer::~WIncrementalTransformChainer()
{
	delete(m_pFirst);
	delete(m_pSecond);
}

// static
WIncrementalTransform* WIncrementalTransformChainer::chain(WIncrementalTransform* pA, WIncrementalTransform* pB)
{
	if(pA == nullptr)
		return pB;
	else if(pB == nullptr)
		return pA;
	else
		return new WIncrementalTransformChainer(pA, pB);
}

// virtual
WDomNode* WIncrementalTransformChainer::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WIncrementalTransformChainer");
	pNode->add(pDoc, "first", m_pFirst->serialize(pDoc));
	pNode->add(pDoc, "second", m_pSecond->serialize(pDoc));
	return pNode;
}

// virtual
WRelation* WIncrementalTransformChainer::trainInner(const WMatrix& data)
{
	m_pFirst->train(data);
	WMatrix* pData2 = m_pFirst->transformBatch(data); // todo: often this step is computation overkill since m_pSecond may not even use it during training. Is there a way to avoid doing it?
	std::unique_ptr<WMatrix> hData2(pData2);
	m_pSecond->train(*pData2);
	return m_pSecond->after().clone();
}

// virtual
WRelation* WIncrementalTransformChainer::trainInner(const WRelation& relation)
{
	m_pFirst->train(relation);
	m_pSecond->train(m_pFirst->after());
	return m_pSecond->after().clone();
}

// virtual
void WIncrementalTransformChainer::transform(const WVec& in, WVec& out)
{
	WVec& buf = m_pFirst->innerBuf();
	m_pFirst->transform(in, buf);
	m_pSecond->transform(buf, out);
}

// virtual
void WIncrementalTransformChainer::untransform(const WVec& in, WVec& out)
{
	WVec& buf = m_pFirst->innerBuf();
	m_pSecond->untransform(in, buf);
	m_pFirst->untransform(buf, out);
}

// virtual
void WIncrementalTransformChainer::untransformToDistribution(const WVec& in, WPrediction* out)
{
	WVec& buf = m_pFirst->innerBuf();
	m_pSecond->untransform(in, buf);
	m_pFirst->untransformToDistribution(buf, out);
}

// ---------------------------------------------------------------

WPCA::WPCA(size_t target_Dims)
: WIncrementalTransform(), m_targetDims(target_Dims), m_pBasisVectors(nullptr), m_aboutOrigin(false), m_rand(0)
{
}

WPCA::WPCA(const WDomNode* pNode)
: WIncrementalTransform(pNode), m_rand(0)
{
	m_pBasisVectors = new WMatrix(pNode->get("basis"));
	m_targetDims = m_pBasisVectors->rows();
	m_aboutOrigin = pNode->getBool("aboutOrigin");
}

// virtual
WPCA::~WPCA()
{
	delete(m_pBasisVectors);
}

// virtual
WDomNode* WPCA::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WPCA");
	pNode->add(pDoc, "basis", m_pBasisVectors->serialize(pDoc));
	pNode->add(pDoc, "aboutOrigin", m_aboutOrigin);
	return pNode;
}

void WPCA::computeEigVals()
{
	m_eigVals.resize(m_targetDims);
}

// virtual
WRelation* WPCA::trainInner(const WMatrix& data)
{
	if(!before().areContinuous())
		throw Ex("WPCA doesn't support nominal values. (You could filter with nominaltocat to make them real.)");
	delete(m_pBasisVectors);
	m_pBasisVectors = new WMatrix(1 + m_targetDims, before().size());

	// Compute the mean
	WVec& mean = m_pBasisVectors->row(0);
	if(m_aboutOrigin)
		mean.fill(0.0);
	else
		data.centroid(mean);

	// Make a copy of the data
	WMatrix tmpData(data.relation().cloneMinimal());
	tmpData.copy(data);

	// Compute the principle components
	double sse = 0;
	if(m_eigVals.size() > 0)
		sse = tmpData.sumSquaredDistance(mean);
	for(size_t i = 0; i < m_targetDims; i++)
	{
		WVec& vec = m_pBasisVectors->row(i + 1);
		tmpData.principalComponentIgnoreUnknowns(vec, mean, &m_rand);
		tmpData.removeComponent(mean, vec);
		if(m_eigVals.size() > 0)
		{
			double t = tmpData.sumSquaredDistance(mean);
			m_eigVals[i] = (sse - t) / (data.rows() - 1);
			sse = t;
		}
	}

	return new WUniformRelation(m_targetDims, 0);
}

// virtual
WRelation* WPCA::trainInner(const WRelation& relation)
{
	throw Ex("This transform cannot be trained without data");
	return before().clone();
}

// virtual
void WPCA::transform(const WVec& in, WVec& out)
{
	WVec& c = m_pBasisVectors->row(0);
	for(size_t i = 0; i < m_targetDims; i++)
	{
		WVec& basisVector = m_pBasisVectors->row(1 + i);
		out[i] = basisVector.dotProductIgnoringUnknowns(c, in);
	}
}

// virtual
void WPCA::untransform(const WVec& in, WVec& out)
{
	throw Ex("Sorry, not implemented yet");
	/* todo: need invert the basis matrix
	out.copy(m_pBasisVectors->row(0));
	for(size_t i = 0; i < m_targetDims; i++)
		out.addScaled(in[i], m_pBasisVectors->row(1 + i));
*/
}

// virtual
void WPCA::untransformToDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, PCA cannot untransform to a distribution");
}

// --------------------------------------------------------------------------

WNoiseGenerator::WNoiseGenerator()
: WIncrementalTransform(), m_rand(0), m_mean(0), m_deviation(1)
{
}

WNoiseGenerator::WNoiseGenerator(const WDomNode* pNode)
: WIncrementalTransform(pNode), m_rand(0)
{
	m_mean = pNode->getDouble("mean");
	m_deviation = pNode->getDouble("dev");
}

WNoiseGenerator::~WNoiseGenerator()
{
}

// virtual
WDomNode* WNoiseGenerator::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WNoiseGenerator");
	pNode->add(pDoc, "mean", m_mean);
	pNode->add(pDoc, "dev", m_deviation);
	return pNode;
}

// virtual
WRelation* WNoiseGenerator::trainInner(const WMatrix& data)
{
	return data.relation().clone();
}

// virtual
WRelation* WNoiseGenerator::trainInner(const WRelation& relation)
{
	return relation.clone();
}

// virtual
void WNoiseGenerator::transform(const WVec& in, WVec& out)
{
	size_t nDims = before().size();
	for(size_t i = 0; i < nDims; i++)
	{
		size_t vals = before().valueCount(i);
		if(vals == 0)
			out[i] = m_rand.normal() * m_deviation + m_mean;
		else
			out[i] = (double)m_rand.next(vals);
	}
}

// --------------------------------------------------------------------------

WPairProduct::WPairProduct(size_t nMaxDims)
: WIncrementalTransform(), m_maxDims(nMaxDims)
{
}

WPairProduct::WPairProduct(const WDomNode* pNode)
: WIncrementalTransform(pNode)
{
	m_maxDims = (size_t)pNode->getInt("maxDims");
}

WPairProduct::~WPairProduct()
{
}

// virtual
WDomNode* WPairProduct::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WPairProduct");
	pNode->add(pDoc, "maxDims", m_maxDims);
	return pNode;
}

// virtual
WRelation* WPairProduct::trainInner(const WMatrix& data)
{
	size_t nAttrsIn = before().size();
	size_t nAttrsOut = std::min(m_maxDims, nAttrsIn * (nAttrsIn + 1) / 2);
	return new WUniformRelation(nAttrsOut, 0);
}

// virtual
WRelation* WPairProduct::trainInner(const WRelation& relation)
{
	size_t nAttrsIn = before().size();
	size_t nAttrsOut = std::min(m_maxDims, nAttrsIn * (nAttrsIn + 1) / 2);
	return new WUniformRelation(nAttrsOut, 0);
}

// virtual
void WPairProduct::transform(const WVec& in, WVec& out)
{
	size_t i, j, nAttr;
	size_t nAttrsIn = before().size();
	size_t nAttrsOut = after().size();
	nAttr = 0;
	for(j = 0; j < nAttrsIn && nAttr < nAttrsOut; j++)
	{
		for(i = j; i < nAttrsIn && nAttr < nAttrsOut; i++)
			out[nAttr++] = in[i] * in[j];
	}
	WAssert(nAttr == nAttrsOut);
}

// --------------------------------------------------------------------------

WDataAugmenter::WDataAugmenter(WIncrementalTransform* pTransform)
: WIncrementalTransform(), m_pTransform(pTransform)
{
}

WDataAugmenter::WDataAugmenter(const WDomNode* pNode, WLearnerLoader& ll)
: WIncrementalTransform(pNode)
{
	m_pTransform = ll.loadIncrementalTransform(pNode->get("trans"));
}

// virtual
WDataAugmenter::~WDataAugmenter()
{
	delete(m_pTransform);
}

// virtual
WDomNode* WDataAugmenter::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WDataAugmenter");
	pNode->add(pDoc, "trans", m_pTransform->serialize(pDoc));
	return pNode;
}

// virtual
WRelation* WDataAugmenter::trainInner(const WMatrix& data)
{
	m_pTransform->train(data);
	WMixedRelation* pNewRel = new WMixedRelation();
	pNewRel->addAttrs(data.relation());
	pNewRel->addAttrs(m_pTransform->after());
	return pNewRel;
}

// virtual
WRelation* WDataAugmenter::trainInner(const WRelation& relation)
{
	m_pTransform->train(relation);
	WMixedRelation* pNewRel = new WMixedRelation();
	pNewRel->addAttrs(before());
	pNewRel->addAttrs(m_pTransform->after());
	return pNewRel;
}

// virtual
void WDataAugmenter::transform(const WVec& in, WVec& out)
{
	memcpy(out.data(), in.data(), before().size() * sizeof(double));
	m_pTransform->transform(in, m_pTransform->innerBuf());
	out.copy(before().size(), m_pTransform->innerBuf());
}

// virtual
void WDataAugmenter::untransform(const WVec& in, WVec& out)
{
	memcpy(out.data(), in.data(), before().size() * sizeof(double));
}

// virtual
void WDataAugmenter::untransformToDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, this method is not implemented yet");
}

// --------------------------------------------------------------------------

WAttributeSelector::WAttributeSelector(const WDomNode* pNode)
: WIncrementalTransform(pNode), m_seed(1234567)
{
	m_labelDims = (size_t)pNode->getInt("labels");
	m_targetFeatures = (size_t)pNode->getInt("target");
	WDomNode* pRanksNode = pNode->get("ranks");
	WDomListIterator it(pRanksNode);
	m_ranks.reserve(it.remaining());
	for(; it.current(); it.advance())
		m_ranks.push_back((size_t)it.currentInt());
	if(m_ranks.size() + (size_t)m_labelDims != (size_t)before().size())
		throw Ex("invalid attribute selector");
	if(m_targetFeatures > m_ranks.size())
		throw Ex("invalid attribute selector");
}

// virtual
WDomNode* WAttributeSelector::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WAttributeSelector");
	pNode->add(pDoc, "labels", m_labelDims);
	pNode->add(pDoc, "target", m_targetFeatures);
	WDomNode* pRanksNode = pNode->add(pDoc, "ranks", pDoc->newList());
	for(size_t i = 0; i < m_ranks.size(); i++)
		pRanksNode->add(pDoc, m_ranks[i]);
	return pNode;
}

WRelation* WAttributeSelector::setTargetFeatures(size_t n)
{
	if(n > before().size())
		throw Ex("out of range");
	WMixedRelation* pRelAfter;
	if(before().type() == WRelation::ARFF)
		pRelAfter = new WArffRelation();
	else
		pRelAfter = new WMixedRelation();
	for(size_t i = 0; i < m_targetFeatures; i++)
		pRelAfter->copyAttr(&before(), m_ranks[i]);
	if(m_labelDims > before().size())
		throw Ex("label dims out of range");
	size_t featureDims = before().size() - m_labelDims;
	for(size_t i = 0; i < m_labelDims; i++)
		pRelAfter->copyAttr(&before(), featureDims + i);
	return pRelAfter;
}

// virtual
WRelation* WAttributeSelector::trainInner(const WMatrix& data)
{
	// Normalize all the data
	if(m_labelDims > data.cols())
		throw Ex("label dims is greater than the number of columns in the data");
	WNormalize norm;
	norm.train(data);
	WMatrix* pNormData = norm.transformBatch(data);
	std::unique_ptr<WMatrix> hNormData(pNormData);

	// Divide into features and labels
	size_t curDims = data.cols() - m_labelDims;
	m_ranks.resize(curDims);
	WMatrix* pFeatures = new WMatrix(*pNormData, 0, 0, data.rows(), data.cols() - m_labelDims);
	std::unique_ptr<WMatrix> hFeatures(pFeatures);
	WMatrix* pLabels = new WMatrix(*pNormData, 0, data.cols() - m_labelDims, data.rows(), m_labelDims);
	std::unique_ptr<WMatrix> hLabels(pLabels);
	vector<size_t> indexMap;
	indexMap.reserve(curDims);
	for(size_t i = 0; i < curDims; i++)
		indexMap.push_back(i);

	// Produce a ranked attributed ordering by deselecting the weakest attribute each time
	WRand rand(m_seed);
	while(curDims > 1)
	{
		// Convert nominal attributes to a categorical distribution
		WNominalToCat ntc;
		ntc.train(*pFeatures);
		WMatrix* pFeaturesReal = ntc.transformBatch(*pFeatures);
		std::unique_ptr<WMatrix> hFeaturesReal(pFeaturesReal);
		vector<size_t> rmap;
		ntc.reverseAttrMap(rmap);
		WNominalToCat ntc2;
		ntc2.train(*pLabels);
		WMatrix* pLabelsReal = ntc2.transformBatch(*pLabels);
		std::unique_ptr<WMatrix> hLabelsReal(pLabelsReal);

		// Train a single-layer neural network with the normalized remaining data
		WNeuralNet nn;
		WBlockLinear* pB = new WBlockLinear(pFeaturesReal->cols(), pLabelsReal->cols());
		nn.add(pB);
		nn.add(new WBlockTanh(pLabelsReal->cols()));
		WSGDOptimizer optimizer(nn, rand);
		optimizer.setWindowSize(30);
		optimizer.setImprovementThresh(0.002);
		optimizer.optimizeWithValidation(*pFeaturesReal, *pLabelsReal);

		// Identify the weakest attribute
		size_t pos = 0;
		double weakest = 1e308;
		size_t weakestIndex = 0;
		WVec& weights = optimizer.model().weights;
		for(size_t i = 0; i < curDims; i++)
		{
			double w = 0;
			while(pos < pFeaturesReal->cols() && rmap[pos] == i)
			{
				for(size_t neuron = 0; neuron < pB->outputs(); neuron++)
					w = std::max(w, std::abs(weights[pLabelsReal->cols() + pLabelsReal->cols() * pos + neuron]));
				pos++;
			}
			if(w < weakest)
			{
				weakest = w;
				weakestIndex = i;
			}
		}

		// Deselect the weakest attribute
		m_ranks[curDims - 1] = indexMap[weakestIndex];
		indexMap.erase(indexMap.begin() + weakestIndex);
		pFeatures->deleteColumns(weakestIndex, 1);
		curDims--;
		WAssert(pFeatures->cols() == curDims);
	}
	m_ranks[0] = indexMap[0];
	return setTargetFeatures(m_targetFeatures);
}

// virtual
WRelation* WAttributeSelector::trainInner(const WRelation& relation)
{
	throw Ex("This transform cannot be trained without data");
	return before().clone();
}

// virtual
void WAttributeSelector::transform(const WVec& in, WVec& out)
{
	size_t i;
	for(i = 0; i < m_targetFeatures; i++)
		out[i] = in[m_ranks[i]];
	size_t featureDims = before().size() - m_labelDims;
	for(size_t j = 0; j < m_labelDims; j++)
		out[i++] = in[featureDims + j];
}

//static
void WAttributeSelector::test()
{
	WRand prng(0);
	WMatrix data(0, 21);
	for(size_t i = 0; i < 256; i++)
	{
		WVec& vec = data.newRow();
		vec.fillUniform(prng);
		vec[20] = 0.2 * vec[3] * vec[3] * -7.0 * vec[3] * vec[13] + vec[17];
	}
	WAttributeSelector as(1, 3);
	as.train(data);
	std::vector<size_t>& r = as.ranks();
	if(r[1] == r[0] || r[2] == r[0] || r[2] == r[1])
		throw Ex("bogus rankings");
	if(r[0] != 3 && r[0] != 13 && r[0] != 17)
		throw Ex("failed");
	if(r[1] != 3 && r[1] != 13 && r[1] != 17)
		throw Ex("failed");
	if(r[2] != 3 && r[2] != 13 && r[2] != 17)
		throw Ex("failed");
}

// --------------------------------------------------------------------------

WNominalToCat::WNominalToCat(size_t nValueCap, double lo, double hi)
: WIncrementalTransform(), m_valueCap(nValueCap), m_preserveUnknowns(false), m_lo(lo), m_hi(hi)
{
}

WNominalToCat::WNominalToCat(const WDomNode* pNode)
: WIncrementalTransform(pNode)
{
	m_valueCap = (size_t)pNode->getInt("valueCap");
	m_preserveUnknowns = pNode->getBool("pu");
	m_lo = pNode->getDouble("lo");
	m_hi = pNode->getDouble("hi");
}

WRelation* WNominalToCat::init()
{
	size_t nDims = 0;
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		nDims += oneHot(nValues) ? nValues : 1;
	}
	return new WUniformRelation(nDims);
}

// virtual
WNominalToCat::~WNominalToCat()
{
}

// virtual
WRelation* WNominalToCat::trainInner(const WMatrix& data)
{
	return init();
}

// virtual
WRelation* WNominalToCat::trainInner(const WRelation& relation)
{
	return init();
}

// virtual
WDomNode* WNominalToCat::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WNominalToCat");
	pNode->add(pDoc, "valueCap", m_valueCap);
	pNode->add(pDoc, "pu", m_preserveUnknowns);
	pNode->add(pDoc, "lo", m_lo);
	pNode->add(pDoc, "hi", m_hi);
	return pNode;
}

// virtual
void WNominalToCat::transform(const WVec& in, WVec& out)
{
	size_t nInAttrCount = before().size();
	size_t j = 0;
	for(size_t i = 0; i < nInAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues < 3)
		{
			if(nValues == 0)
				out[j++] = in[i];
			else if(nValues == 1)
			{
				if(in[i] == UNKNOWN_DISCRETE_VALUE)
					out[j++] = UNKNOWN_REAL_VALUE;
				else
					out[j++] = m_lo;
			}
			else
			{
				if(in[i] == UNKNOWN_DISCRETE_VALUE)
				{
					if(m_preserveUnknowns)
						out[j++] = UNKNOWN_REAL_VALUE;
					else
						out[j++] = 0.5 * (m_lo + m_hi);
				}
				else
					out[j++] = in[i];
			}
		}
		else if(oneHot(nValues))
		{
			if(in[i] >= 0)
			{
				WAssert(in[i] < nValues);
				out.fill(m_lo, j, nValues);
				out[j + (int)in[i]] = m_hi;
			}
			else
			{
				if(m_preserveUnknowns)
					out.fill(UNKNOWN_REAL_VALUE, j, nValues);
				else
					out.fill((m_hi - m_lo) / nValues + m_lo, j, nValues);
			}
			j += nValues;
		}
		else
		{
			if(in[i] == UNKNOWN_DISCRETE_VALUE)
				out[j++] = UNKNOWN_REAL_VALUE;
			else
				out[j++] = in[i];
		}
	}
}

// virtual
void WNominalToCat::untransform(const WVec& in, WVec& out)
{
	size_t nOutAttrCount = before().size();
	size_t j = 0;
	for(size_t i = 0; i < nOutAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues < 3)
		{
			if(nValues == 0)
				out[i] = in[j++];
			else if(nValues == 1)
			{
				if(in[j++] == UNKNOWN_REAL_VALUE)
					out[i] = UNKNOWN_DISCRETE_VALUE;
				else
					out[i] = 0;
			}
			else
			{
				if(in[j] == UNKNOWN_REAL_VALUE)
					out[i] = UNKNOWN_DISCRETE_VALUE;
				else
					out[i] = (in[j] + in[j] < m_lo + m_hi ? 0 : 1);
				j++;
			}
		}
		else if(oneHot(nValues))
		{
			double max = in[j++];
			out[i] = 0.0;
			for(size_t k = 1; k < nValues; k++)
			{
				if(in[j] > max)
				{
					max = in[j];
					out[i] = (double)k;
				}
				j++;
			}
		}
		else
		{
			if(in[j] == UNKNOWN_REAL_VALUE)
				out[i] = UNKNOWN_DISCRETE_VALUE;
			else
				out[i] = std::max(0.0, std::min(double(nValues - 1), floor(in[j] + 0.5)));
			j++;
		}
	}
}

// virtual
void WNominalToCat::untransformToDistribution(const WVec& in, WPrediction* out)
{
	size_t nOutAttrCount = before().size();
	size_t j = 0;
	for(size_t i = 0; i < nOutAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues < 3)
		{
			if(nValues == 0)
			{
				WNormalDistribution* pNorm = out->makeNormal();
				pNorm->setMeanAndVariance(in[j], 1.0); // todo: should we throw an exception here since we have no way to estimate the variance?
			}
			else if(nValues == 1)
			{
				WCategoricalDistribution* pCat = out->makeCategorical();
				pCat->setToUniform(1);
			}
			else
			{
				WCategoricalDistribution* pCat = out->makeCategorical();
				if(in[j] == UNKNOWN_REAL_VALUE)
					pCat->setToUniform(2);
				else
				{
					WVec& vals = pCat->values(2);
					double t = (in[j] - m_lo) / (m_hi - m_lo);
					vals[0] = 1.0 - t;
					vals[1] = t;
					pCat->normalize(); // We have to normalize to ensure the values are properly clipped.
				}
			}
			j++;
			out++;
		}
		else if(oneHot(nValues))
		{
			WCategoricalDistribution* pCat = out->makeCategorical();
			pCat->setValues(nValues, in.data() + j);
			j += nValues;
			out++;
		}
		else
		{
			WCategoricalDistribution* pCat = out->makeCategorical();
			pCat->setSpike(nValues, std::max(size_t(0), std::min(nValues - 1, size_t(floor(in[j] + 0.5)))), 3);
			j++;
			out++;
		}
	}
}

void WNominalToCat::reverseAttrMap(vector<size_t>& rmap)
{
	rmap.clear();
	size_t nInAttrCount = before().size();
	for(size_t i = 0; i < nInAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(oneHot(nValues))
		{
			for(size_t j = 0; j < nValues; j++)
				rmap.push_back(i);
		}
		else
			rmap.push_back(i);
	}
}

// --------------------------------------------------------------------------

WNormalize::WNormalize(double min, double max)
: WIncrementalTransform(), m_min(min), m_max(max)
{
}

WNormalize::WNormalize(const WDomNode* pNode)
: WIncrementalTransform(pNode)
{
	m_min = pNode->getDouble("min");
	m_max = pNode->getDouble("max");
	m_mins.deserialize(pNode->get("mins"));
	m_ranges.deserialize(pNode->get("ranges"));
}

// virtual
WNormalize::~WNormalize()
{
}

// virtual
WDomNode* WNormalize::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WNormalize");
	pNode->add(pDoc, "min", m_min);
	pNode->add(pDoc, "max", m_max);
	pNode->add(pDoc, "mins", m_mins.serialize(pDoc));
	pNode->add(pDoc, "ranges", m_ranges.serialize(pDoc));
	return pNode;
}

void WNormalize::setMinsAndRanges(const WRelation& rel, const WVec& mins, const WVec& ranges)
{
	setBefore(rel.clone());
	setAfter(rel.clone());
	m_mins.copy(mins);
	m_ranges.copy(ranges);
}

// virtual
WRelation* WNormalize::trainInner(const WMatrix& data)
{
	size_t nAttrCount = before().size();
	m_mins.resize(nAttrCount);
	m_ranges.resize(nAttrCount);
	for(size_t i = 0; i < nAttrCount; i++)
	{
		if(before().valueCount(i) == 0)
		{
			m_mins[i] = data.columnMin(i);
			if(m_mins[i] >= 1e300)
			{
				m_mins[i] = 0.0;
				m_ranges[i] = 1.0;
			}
			else
			{
				m_ranges[i] = data.columnMax(i) - m_mins[i];
				if(m_ranges[i] < 1e-12)
					m_ranges[i] = 1.0;
			}
		}
		else
		{
			m_mins[i] = 0;
			m_ranges[i] = 0;
		}
	}
	return data.relation().clone();
}

// virtual
WRelation* WNormalize::trainInner(const WRelation& relation)
{
	throw Ex("This transform cannot be trained without data");
	return before().clone();
}

// virtual
void WNormalize::transform(const WVec& in, WVec& out)
{
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		if(before().valueCount(i) == 0)
		{
			if(in[i] == UNKNOWN_REAL_VALUE)
				out[i] = UNKNOWN_REAL_VALUE;
			else
				out[i] = WMatrix::normalizeValue(in[i], m_mins[i], m_mins[i] + m_ranges[i], m_min, m_max);
		}
		else
			out[i] = in[i];
	}
}

// virtual
void WNormalize::untransform(const WVec& in, WVec& out)
{
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		if(before().valueCount(i) == 0)
		{
			if(in[i] == UNKNOWN_REAL_VALUE)
				out[i] = UNKNOWN_REAL_VALUE;
			else
				out[i] = WMatrix::normalizeValue(in[i], m_min, m_max, m_mins[i], m_mins[i] + m_ranges[i]);
		}
		else
			out[i] = in[i];
	}
}

// virtual
void WNormalize::untransformToDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, cannot denormalize to a distribution");
}

// --------------------------------------------------------------------------

WDiscretize::WDiscretize(size_t buckets)
: WIncrementalTransform()
{
	m_bucketsIn = buckets;
	m_bucketsOut = -1;
}

WDiscretize::WDiscretize(const WDomNode* pNode)
: WIncrementalTransform(pNode)
{
	m_bucketsIn = (size_t)pNode->getInt("bucketsIn");
	m_bucketsOut = (size_t)pNode->getInt("bucketsOut");
	m_mins.deserialize(pNode->get("mins"));
	m_ranges.deserialize(pNode->get("ranges"));
}

// virtual
WDiscretize::~WDiscretize()
{
}

// virtual
WDomNode* WDiscretize::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WDiscretize");
	pNode->add(pDoc, "bucketsIn", m_bucketsIn);
	pNode->add(pDoc, "bucketsOut", m_bucketsOut);
	pNode->add(pDoc, "mins", m_mins.serialize(pDoc));
	pNode->add(pDoc, "ranges", m_ranges.serialize(pDoc));
	return pNode;
}

// virtual
WRelation* WDiscretize::trainInner(const WMatrix& data)
{
	// Make the relations
	m_bucketsOut = m_bucketsIn;
	if(m_bucketsOut > data.rows())
		m_bucketsOut = std::max((size_t)2, (size_t)sqrt((double)data.rows()));
	size_t nAttrCount = data.cols();
	WMixedRelation* pRelationAfter = new WMixedRelation();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues > 0)
			pRelationAfter->addAttr(nValues);
		else
			pRelationAfter->addAttr(m_bucketsOut);
	}

	// Determine the boundaries
	m_mins.resize(nAttrCount);
	m_ranges.resize(nAttrCount);
	for(size_t i = 0; i < nAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues > 0)
		{
			m_mins[i] = 0;
			m_ranges[i] = 0;
		}
		else
		{
			m_mins[i] = data.columnMin(i);
			m_ranges[i] = data.columnMax(i) - m_mins[i];
			m_ranges[i] = std::max(m_ranges[i], 1e-9);
		}
	}
	return pRelationAfter;
}

// virtual
WRelation* WDiscretize::trainInner(const WRelation& relation)
{
	throw Ex("This transform cannot be trained without data");
	return before().clone();
}

// virtual
void WDiscretize::transform(const WVec& in, WVec& out)
{
	if(m_mins.size() == 0)
		throw Ex("Train was not called");
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues > 0)
			out[i] = in[i];
		else
			out[i] = std::max(0, std::min((int)(m_bucketsOut - 1), (int)(((in[i] - m_mins[i]) * m_bucketsOut) / m_ranges[i])));
	}
}

// virtual
void WDiscretize::untransform(const WVec& in, WVec& out)
{
	if(m_mins.size() == 0)
		throw Ex("Train was not called");
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues > 0)
			out[i] = in[i];
		else
			out[i] = (((double)in[i] + .5) * m_ranges[i]) / m_bucketsOut + m_mins[i];
	}
}

// virtual
void WDiscretize::untransformToDistribution(const WVec& in, WPrediction* out)
{
	if(m_mins.size() == 0)
		throw Ex("Train was not called");
	size_t attrCount = before().size();
	for(size_t i = 0; i < attrCount; i++)
	{
		size_t nValues = before().valueCount(i);
		if(nValues > 0)
			out[i].makeCategorical()->setSpike(nValues, (size_t)in[i], 1);
		else
			out[i].makeNormal()->setMeanAndVariance((((double)in[i] + .5) * m_ranges[i]) / m_bucketsOut + m_mins[i], m_ranges[i] * m_ranges[i]);
	}
}





WImputeMissingVals::WImputeMissingVals()
: m_pCF(nullptr), m_pNTC(nullptr), m_pLabels(nullptr), m_pBatch(nullptr)
{
}

WImputeMissingVals::WImputeMissingVals(const WDomNode* pNode, WLearnerLoader& ll)
: WIncrementalTransform(pNode), m_pLabels(nullptr), m_pBatch(nullptr)
{
	m_pCF = ll.loadCollaborativeFilter(pNode->get("cf"));
	WDomNode* pNTC = pNode->getIfExists("ntc");
	if(pNTC)
		m_pNTC = new WNominalToCat(pNTC);
	else
		m_pNTC = nullptr;
}

// virtual
WImputeMissingVals::~WImputeMissingVals()
{
	delete(m_pCF);
	delete(m_pNTC);
	delete(m_pBatch);
}

// virtual
WDomNode* WImputeMissingVals::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WImputeMissingVals");
	pNode->add(pDoc, "cf", m_pCF->serialize(pDoc));
	if(m_pNTC)
		pNode->add(pDoc, "ntc", m_pNTC->serialize(pDoc));
	return pNode;
}

void WImputeMissingVals::setCollaborativeFilter(WCollaborativeFilter* pCF)
{
	delete(m_pCF);
	m_pCF = pCF;
}

// virtual
WRelation* WImputeMissingVals::trainInner(const WMatrix& data)
{
	// Train the nominalToCat filter if needed
	if(data.relation().areContinuous())
	{
		delete(m_pNTC);
		m_pNTC = nullptr;
	}
	else if(!m_pNTC)
	{
		m_pNTC = new WNominalToCat();
		m_pNTC->preserveUnknowns();
	}
	const WMatrix* pData;
	std::unique_ptr<WMatrix> hData;
	if(m_pNTC)
	{
		m_pNTC->train(data);
		WMatrix* pTemp = m_pNTC->transformBatch(data);
		hData.reset(pTemp);
		pData = pTemp;
	}
	else
		pData = &data;

	// Train the collaborative filter
	if(!m_pCF)
		m_pCF = new WMatrixFactorization(std::max(size_t(2), std::min(size_t(8), data.cols() / 3)));
	m_pCF->trainDenseMatrix(*pData, m_pLabels);
	return before().clone();
}

// virtual
WRelation* WImputeMissingVals::trainInner(const WRelation& relation)
{
	throw Ex("This transform cannot be trained without data");
	return before().clone();
}

// virtual
void WImputeMissingVals::transform(const WVec& in, WVec& out)
{
	// If there are no missing values, just copy it across
	size_t dims = before().size();
	size_t i;
	for(i = 0; i < dims; i++)
	{
		if(before().valueCount(i) == 0)
		{
			if(in[i] == UNKNOWN_REAL_VALUE)
				break;
		}
		else
		{
			if(in[i] == UNKNOWN_DISCRETE_VALUE)
				break;
		}
	}
	if(i >= dims)
	{
		out.copy(in);
		return;
	}

	// Convert to all real values if necessary
	WVec* pVec;
	if(m_pNTC)
	{
		m_pNTC->transform(in, m_pNTC->innerBuf());
		pVec = &m_pNTC->innerBuf();
		dims = m_pNTC->after().size();
	}
	else
	{
		out.copy(in);
		pVec = &out;
	}

	// Impute the missing values
	m_pCF->impute(*pVec, dims);

	// Convert back to nominal if necessary
	if(m_pNTC)
		m_pNTC->untransform(*pVec, out);
}

// virtual
void WImputeMissingVals::untransform(const WVec& in, WVec& out)
{
	memcpy(out.data(), in.data(), after().size() * sizeof(double));
}

// virtual
void WImputeMissingVals::untransformToDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, cannot unimpute to a distribution");
}

// virtual
WMatrix* WImputeMissingVals::transformBatch(const WMatrix& in)
{
	WMatrix* out = new WMatrix();
	out->copy(in);
	size_t dims = out->cols();
	for(size_t i = 0; i < out->rows(); i++)
	{
		WVec& vec = out->row(i);
		for(size_t j = 0; j < dims; j++)
		{
			if(vec[j] == UNKNOWN_REAL_VALUE)
				vec[j] = m_pCF->predict(i, j);
		}
	}
	return out;
}

void WImputeMissingVals::setLabels(const WMatrix* pLabels)
{
	m_pLabels = pLabels;
}


// --------------------------------------------------------------------------

WLogify::WLogify()
: WIncrementalTransform()
{
}

WLogify::WLogify(const WDomNode* pNode)
: WIncrementalTransform(pNode)
{
}

// virtual
WLogify::~WLogify()
{
}

// virtual
WDomNode* WLogify::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WLogify");
	return pNode;
}

// virtual
WRelation* WLogify::trainInner(const WMatrix& data)
{
	return data.relation().clone();
}

// virtual
WRelation* WLogify::trainInner(const WRelation& relation)
{
	return relation.clone();
}

// virtual
void WLogify::transform(const WVec& in, WVec& out)
{
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		if(before().valueCount(i) == 0)
		{
			if(in[i] == UNKNOWN_REAL_VALUE)
				out[i] = UNKNOWN_REAL_VALUE;
			else
				out[i] = log(in[i]);
		}
		else
			out[i] = in[i];
	}
}

// virtual
void WLogify::untransform(const WVec& in, WVec& out)
{
	size_t nAttrCount = before().size();
	for(size_t i = 0; i < nAttrCount; i++)
	{
		if(before().valueCount(i) == 0)
		{
			if(in[i] == UNKNOWN_REAL_VALUE)
				out[i] = UNKNOWN_REAL_VALUE;
			else
				out[i] = exp(in[i]);
		}
		else
			out[i] = in[i];
	}
}

// virtual
void WLogify::untransformToDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, cannot unlogify to a distribution");
}



} // namespace WClasses

