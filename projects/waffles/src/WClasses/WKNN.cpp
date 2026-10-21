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

#include "WKNN.h"
#include <math.h>
#include "WError.h"
#include <stdlib.h>
#include "WDom.h"
#include "WDistribution.h"
#include "WHeap.h"
#include "WNeighborFinder.h"
#include "WVec.h"
#include "WHillClimber.h"
#include "WCluster.h"
#include "WBitTable.h"
#include "WDistance.h"
#include "WSparseMatrix.h"
#include "WHolders.h"
#include "WBitTable.h"
#include <map>
#include <queue>
#include <memory>
#include <random>

using std::multimap;
using std::map;
using std::pair;
using std::priority_queue;
using std::vector;


namespace WClasses
{

class WKnnScaleFactorCritic : public WTargetFunction
{
protected:
	size_t m_labelDims;
	WKNN* m_pLearner;
	double* m_pAccuracy;

public:
	WKnnScaleFactorCritic(WKNN* pLearner, size_t featureDims, size_t labelDims)
	: WTargetFunction(featureDims), m_labelDims(labelDims)
	{
		m_pLearner = pLearner;
		m_pAccuracy = new double[labelDims];
	}

	~WKnnScaleFactorCritic() override
	{
		delete[] m_pAccuracy;
	}

	void initVector(WVec& pVector) override
	{
		WDistanceMetric* pMetric = m_pLearner->metric();
		pVector.copy(pMetric->scaleFactors());
	}

	bool isStable() override { return false; }
	virtual bool isConstrained() { return false; }

protected:
	double computeError(const WVec& pVector) override
	{
		// todo: this method is WAAAY too inefficient
		WMatrix* pFeatures = m_pLearner->features();
		WMatrix* pLabels = m_pLearner->labels();
		WKNN temp;
		temp.setNeighborCount(m_pLearner->neighborCount());
		temp.beginIncrementalLearning(pFeatures->relation(), pLabels->relation());
		temp.metric()->scaleFactors().copy(pVector);
		return temp.crossValidate(*pFeatures, *pLabels, 2);
	}
};


WKNN::WKNN()
: WIncrementalLearner()
{
	m_eInterpolationMethod = Linear;
	m_eTrainMethod = StoreAll;
	m_trainParam = 0.0;
	m_pLearner = nullptr;
	m_bOwnLearner = false;
	m_nNeighbors = 1;
	m_pFeatures = nullptr;
	m_pSparseFeatures = nullptr;
	m_pLabels = nullptr;
	m_pNeighborFinder = nullptr;
	m_normalizeScaleFactors = true;
	m_optimizeScaleFactors = false;
	m_pDistanceMetric = nullptr;
	m_pSparseMetric = nullptr;
	m_ownMetric = false;
	m_pCritic = nullptr;
	m_pScaleFactorOptimizer = nullptr;
}

WKNN::WKNN(const WDomNode* pNode)
: WIncrementalLearner(pNode)
{
	m_pNeighborFinder = nullptr;
	m_pCritic = nullptr;
	m_pScaleFactorOptimizer = nullptr;
	m_pLearner = nullptr;
	m_bOwnLearner = false;
	m_nNeighbors = (size_t)pNode->getInt("neighbors");
	m_eInterpolationMethod = (InterpolationMethod)pNode->getInt("interpMethod");
	m_eTrainMethod = (TrainMethod)pNode->getInt("trainMethod");
	m_trainParam = pNode->getDouble("trainParam");
	m_normalizeScaleFactors = pNode->getBool("normalize");
	m_optimizeScaleFactors = pNode->getBool("optimize");
	WMatrix* pFeatures = nullptr;
	WSparseMatrix* pSparseFeatures = nullptr;
	WDomNode* pFeaturesNode = pNode->getIfExists("features");
	if(pFeaturesNode)
		pFeatures = new WMatrix(pFeaturesNode);
	else
		pSparseFeatures = new WSparseMatrix(pNode->get("sparseFeatures"));
	WMatrix* pLabels = new WMatrix(pNode->get("labels"));
	WDomNode* pMetricNode = pNode->getIfExists("metric");
	m_pDistanceMetric = nullptr;
	m_pSparseMetric = nullptr;
	if(pMetricNode)
		m_pDistanceMetric = WDistanceMetric::deserialize(pNode->get("metric"));
	else
		m_pSparseMetric = WSparseSimilarity::deserialize(pNode->get("sparseMetric"));
	m_ownMetric = true;
	m_pFeatures = nullptr;
	m_pSparseFeatures = nullptr;
	m_pLabels = nullptr;
	if(pFeatures)
		beginIncrementalLearningInner(pFeatures->relation(), pLabels->relation());
	else
	{
		WUniformRelation rel(pSparseFeatures->cols(), 0);
		beginIncrementalLearningInner(rel, pLabels->relation());
	}
	delete(m_pFeatures);
	delete(m_pSparseFeatures);
	delete(m_pLabels);
	m_pFeatures = pFeatures;
	m_pSparseFeatures = pSparseFeatures;
	m_pLabels = pLabels;
}

WKNN::~WKNN()
{
	delete(m_pNeighborFinder);
	delete(m_pFeatures);
	delete(m_pSparseFeatures);
	delete(m_pLabels);
	delete(m_pScaleFactorOptimizer);
	delete(m_pCritic);
	if(m_ownMetric)
	{
		delete(m_pDistanceMetric);
		delete(m_pSparseMetric);
	}
}

// virtual
WDomNode* WKNN::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WKNN");
	pNode->add(pDoc, "neighbors", m_nNeighbors);
	if(m_eInterpolationMethod == Learner)
		throw Ex("Sorry, serialize is not supported for the \"Learner\" interpolation method");
	pNode->add(pDoc, "interpMethod", (long long)m_eInterpolationMethod);
	pNode->add(pDoc, "trainMethod", (long long)m_eTrainMethod);
	pNode->add(pDoc, "trainParam", m_trainParam);
	pNode->add(pDoc, "normalize", m_normalizeScaleFactors);
	pNode->add(pDoc, "optimize", m_optimizeScaleFactors);
	if(m_pFeatures)
		pNode->add(pDoc, "features", m_pFeatures->serialize(pDoc));
	else
		pNode->add(pDoc, "sparseFeatures", m_pSparseFeatures->serialize(pDoc));
	pNode->add(pDoc, "labels", m_pLabels->serialize(pDoc));
	if(m_pDistanceMetric)
		pNode->add(pDoc, "metric", m_pDistanceMetric->serialize(pDoc));
	else
		pNode->add(pDoc, "sparseMetric", m_pSparseMetric->serialize(pDoc));
	return pNode;
}

void WKNN::autoTune(WMatrix& feats, WMatrix& labs)
{
	// Find the best value for k
	size_t cap = size_t(floor(sqrt(double(feats.rows()))));
	size_t bestK = 1;
	double bestErr = 1e308;
	for(size_t i = 1; i < cap; i *= 3)
	{
		setNeighborCount(i);
		double d = crossValidate(feats, labs, 2);
		if(d < bestErr)
		{
			bestErr = d;
			bestK = i;
		}
		else if(i >= 27)
			break;
	}

	// Set the best values
	m_nNeighbors = bestK;

	// Try without normalization
	m_normalizeScaleFactors = false;
	double d = crossValidate(feats, labs, 2);
	if(d >= bestErr)
		m_normalizeScaleFactors = true;
}

void WKNN::setNeighborCount(size_t k)
{
	m_nNeighbors = k;
}

void WKNN::setInterpolationMethod(InterpolationMethod eMethod)
{
	if(eMethod == Learner)
		throw Ex("You should call SetInterpolationLearner instead");
	m_eInterpolationMethod = eMethod;
}

void WKNN::setInterpolationLearner(WSupervisedLearner* pLearner, bool bOwnLearner)
{
	if(m_bOwnLearner)
		delete(m_pLearner);
	m_pLearner = pLearner;
	m_eInterpolationMethod = Learner;
	m_bOwnLearner = bOwnLearner;
}

size_t WKNN::addVector(const WVec& feat, const WVec& lab)
{
	// Store the features
	size_t index;
	if(m_pNeighborFinder)
	{
		delete(m_pNeighborFinder);
		m_pNeighborFinder = nullptr;
	}
	index = m_pFeatures->rows();
	m_pFeatures->newRow().copy(feat);

	// Store the labels
	m_pLabels->newRow().copy(lab);
	return index;
}

void WKNN::setNormalizeScaleFactors(bool b)
{
	m_normalizeScaleFactors = b;
}

void WKNN::setOptimizeScaleFactors(bool b)
{
	m_optimizeScaleFactors = b;
}

void WKNN::setMetric(WDistanceMetric* pMetric, bool own)
{
	if(m_ownMetric)
	{
		delete(m_pDistanceMetric);
		delete(m_pSparseMetric);
	}
	m_pDistanceMetric = pMetric;
	m_pSparseMetric = nullptr;
	m_ownMetric = own;
}

void WKNN::setMetric(WSparseSimilarity* pMetric, bool own)
{
	if(m_ownMetric)
	{
		delete(m_pDistanceMetric);
		delete(m_pSparseMetric);
	}
	m_pDistanceMetric = nullptr;
	m_pSparseMetric = pMetric;
	m_ownMetric = own;
}

// virtual
void WKNN::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	clear();
	if(!m_pDistanceMetric && !m_pSparseMetric)
		setMetric(new WRowDistance(), true);
	if(m_pDistanceMetric)
	{
		m_pFeatures = new WMatrix(featureRel.cloneMinimal());
		m_pDistanceMetric->init(&m_pFeatures->relation(), false);

		// Scale factor optimization
		if(m_optimizeScaleFactors)
		{
			m_pCritic = new WKnnScaleFactorCritic(this, featureRel.size(), labelRel.size());
			m_pScaleFactorOptimizer = new WMomentumGreedySearch(m_pCritic);
		}
	}
	else if(m_pSparseMetric)
	{
		if(!featureRel.areContinuous(0, featureRel.size()))
			throw Ex("Sorry, nominal features cannot be used in conjunction with sparse metrics");
		m_pSparseFeatures = new WSparseMatrix(0, featureRel.size(), UNKNOWN_REAL_VALUE);
	}
	else
		throw Ex("Some sort of distance or similarity metric is required");

	m_pLabels = new WMatrix(labelRel.cloneMinimal());

	// Allocate a buffer for counting values
	size_t maxOutputValueCount = 0;
	for(size_t n = 0; n < labelRel.size(); n++)
		maxOutputValueCount = std::max(maxOutputValueCount, labelRel.valueCount(n));
	m_valueCounts.resize(maxOutputValueCount);
}

// virtual
void WKNN::trainIncremental(const WVec& in, const WVec& out)
{
	// Make a copy of the vector
	WAssert(m_pDistanceMetric);
	addVector(in, out);

	// Learn how to scale the attributes
	if(m_pScaleFactorOptimizer && m_pFeatures->rows() > 50)
	{
		m_pScaleFactorOptimizer->iterate();
		m_pDistanceMetric->scaleFactors().copy(m_pScaleFactorOptimizer->currentVector());
	}
}

void WKNN::trainInner(const WMatrix& feats, const WMatrix& labs)
{
	if(m_pSparseMetric)
		throw Ex("This method is not compatible with sparse similarity metrics. You should either use trainSparse instead, or use a dense dissimilarity metric.");
	beginIncrementalLearningInner(feats.relation(), labs.relation());

	// Give each attribute an equal chance by scaling out the deviation
	WVec& scaleFactors = m_pDistanceMetric->scaleFactors();
	if(m_normalizeScaleFactors)
	{
		for(size_t i = 0; i < feats.cols(); i++)
		{
			if(feats.relation().valueCount(i) == 0)
			{
				double m = feats.columnMean(i);
				double d = sqrt(feats.columnVariance(i, m));
				if(d >= 1e-8)
					scaleFactors[i] = 1.0 / (2.0 * d);
				else
					scaleFactors[i] = 1.0;
			}
			else
				scaleFactors[i] = 1.0;
		}
	}
	else
		scaleFactors.fill(1.0);

	if(m_eTrainMethod == StoreAll)
	{
		m_pFeatures->reserve(feats.rows());
		m_pLabels->reserve(feats.rows());
		for(size_t i = 0; i < feats.rows(); i++)
			addVector(feats[i], labs[i]);
	}
	else if(m_eTrainMethod == ValidationPrune)
	{
		throw Ex("Sorry, this training method is not implemented yet");
	}
	else if(m_eTrainMethod == DrawRandom)
	{
		size_t n = (size_t)m_trainParam;
		m_pFeatures->reserve(n);
		m_pLabels->reserve(n);
		for(size_t i = 0; i < n; i++)
		{
			size_t index = (size_t)m_rand.next(feats.rows());
			addVector(feats[index], labs[index]);
		}
	}

	// Learn to scale the attributes
	if(m_pScaleFactorOptimizer)
	{
		if(!m_pNeighborFinder)
		{
			m_pNeighborFinder = new WKdTree(m_pFeatures, m_pDistanceMetric, false);
		}
		for(size_t j = 0; j < 50; j++)
		{
			m_pScaleFactorOptimizer->iterate();
			m_pNeighborFinder->reoptimize();
		}
		scaleFactors.copy(m_pScaleFactorOptimizer->currentVector());
	}
}

// virtual
void WKNN::trainSparse(WSparseMatrix& feats, WMatrix& labs)
{
	if(feats.rows() != labs.rows())
		throw Ex("Expected the features and labels to have the same number of rows");
	if(m_pDistanceMetric)
		throw Ex("This method is not compatible with dense dissimilarity metrics. You should either use the train method instead, or use a sparse similarity metric.");
	if(!m_pSparseMetric)
		setMetric(new WCosineSimilarity(), true);
	WUniformRelation featureRel(feats.cols(), 0);
	beginIncrementalLearning(featureRel, labs.relation());

	// Copy the training data
	m_pSparseFeatures->newRows(feats.rows());
	m_pSparseFeatures->copyFrom(&feats);
	m_pLabels->copy(labs);
}

size_t WKNN::findNeighbors(const WVec& vec)
{
	if(!m_pNeighborFinder)
	{
		if(m_pDistanceMetric)
		{
			//m_pNeighborFinder = new WBruteForceNeighborFinder(m_pFeatures, m_pDistanceMetric, false);
			m_pNeighborFinder = new WKdTree(m_pFeatures, m_pDistanceMetric, false);
		}
		else
		{
			WAssert(m_pSparseMetric);
			WAssert(m_pSparseFeatures);
			WMatrix bogus;
			m_pNeighborFinder = new WSparseNeighborFinder(m_pSparseFeatures, &bogus, m_pSparseMetric, false);
		}
	}
	return m_pNeighborFinder->findNearest(m_nNeighbors, vec);
}

void WKNN::interpolateMean(size_t nc, const WVec& in, WPrediction* out, WVec* pOut2)
{
	for(size_t i = 0; i < m_pLabels->cols(); i++)
	{
		if(m_pLabels->relation().valueCount(i) == 0)
		{
			// Continuous label
			double dSum = 0;
			double dSumOfSquares = 0;
			size_t count = 0;
			for(size_t j = 0; j < nc; j++)
			{
				size_t k = m_pNeighborFinder->neighbor(j);
				WVec& neighbor = m_pLabels->row(k);
				dSum += neighbor[i];
				dSumOfSquares += (neighbor[i] * neighbor[i]);
				count++;
			}
			if(out)
			{
				if(count > 0)
				{
					double mean = dSum / count;
					out[i].makeNormal()->setMeanAndVariance(mean, dSumOfSquares / count - (mean * mean));
				}
				else
					out[i].makeNormal()->setMeanAndVariance(0, 1);
			}
			if(pOut2)
			{
				if(count > 0)
					(*pOut2)[i] = dSum / count;
				else
					(*pOut2)[i] = 0;
			}
		}
		else
		{
			// Nominal label
			size_t nValueCount = m_pLabels->relation().valueCount(i);
			m_valueCounts.fill(0.0, 0, nValueCount);
			for(size_t j = 0; j < nc; j++)
			{
				size_t k = m_pNeighborFinder->neighbor(j);
				WVec& neighbor = m_pLabels->row(k);
				int val = (int)neighbor[i];
				if(val < 0 || val >= (int)nValueCount)
					throw Ex("WKNN doesn't support unknown label values");
				m_valueCounts[val]++;
			}
			if(out)
				out[i].makeCategorical()->setValues(nValueCount, m_valueCounts.data());
			if(pOut2)
				(*pOut2)[i] = (double)m_valueCounts.indexOfMax((size_t)0, nValueCount);
		}
	}
}

void WKNN::interpolateLinear(size_t nc, const WVec& in, WPrediction* out, WVec* pOut2)
{
	for(size_t i = 0; i < m_pLabels->cols(); i++)
	{
		if(m_pLabels->relation().valueCount(i) == 0)
		{
			// Continuous label
			double dSum = 0;
			double dSumOfSquares = 0;
			double dTot = 0;
			for(size_t j = 0; j < nc; j++)
			{
				size_t k = m_pNeighborFinder->neighbor(j);
				WVec& neighbor = m_pLabels->row(k);
				if(neighbor[i] == UNKNOWN_REAL_VALUE)
					throw Ex("WKNN doesn't support unknown label values");
				double d = 1.0 / std::max(sqrt(m_pNeighborFinder->distance(j)), 1e-9); // the weight
				dTot += d;
				d *= neighbor[i]; // weighted sum
				dSum += d;
				d *= neighbor[i]; // weighted sum of squares
				dSumOfSquares += d;
			}
			if(out)
			{
				if(dTot > 0)
				{
					double d = dSum / dTot;
					out[i].makeNormal()->setMeanAndVariance(d, dSumOfSquares / dTot - (d * d));
				}
				else
					out[i].makeNormal()->setMeanAndVariance(0, 1);
			}
			if(pOut2)
			{
				if(dTot > 0)
					(*pOut2)[i] = dSum / dTot;
				else
					(*pOut2)[i] = 0;
			}
		}
		else
		{
			// Nominal label
			int nValueCount = (int)m_pLabels->relation().valueCount(i);
			m_valueCounts.fill(0.0, 0, nValueCount);
			[[maybe_unused]] double dSumWeight = 0;
			for(size_t j = 0; j < nc; j++)
			{
				size_t k = m_pNeighborFinder->neighbor(j);
				if(k < m_pLabels->rows())
				{
					WVec& neighbor = m_pLabels->row(k);
					double d = 1.0 / std::max(m_pNeighborFinder->distance(j), 1e-9); // to be truly "linear", we should use sqrt(d) instead of d, but this is faster to compute and arguably better for nominal values anyway
					int val = (int)neighbor[i];
					if(val < 0 || val >= nValueCount)
						throw Ex("WKNN doesn't support unknown label values");
					m_valueCounts[val] += d;
					dSumWeight += d;
				}
			}
			if(out)
				out[i].makeCategorical()->setValues(nValueCount, m_valueCounts.data());
			if(pOut2)
				(*pOut2)[i] = (double)m_valueCounts.indexOfMax((size_t)0, nValueCount);
		}
	}
}

void WKNN::interpolateLearner(size_t nc, const WVec& in, WPrediction* out, WVec* pOut2)
{
	WAssert(m_pLearner); // no learner is set
	WMatrix dataFeatures(m_pFeatures->relation().cloneMinimal());
	WReleaseDataHolder hDataFeatures(&dataFeatures);
	dataFeatures.reserve(nc);
	WMatrix dataLabels(m_pLabels->relation().cloneMinimal());
	WReleaseDataHolder hDataLabels(&dataLabels);
	dataLabels.reserve(nc);
	for(size_t i = 0; i < nc; i++)
	{
		size_t nNeighbor = m_pNeighborFinder->neighbor(i);
		dataFeatures.takeRow(&m_pFeatures->row(nNeighbor));
		dataLabels.takeRow(&m_pLabels->row(nNeighbor));
	}
	m_pLearner->train(dataFeatures, dataLabels);
	if(out)
		m_pLearner->predictDistribution(in, out);
	if(pOut2)
		m_pLearner->predict(in, *pOut2);
}

// virtual
void WKNN::predictDistribution(const WVec& in, WPrediction* out)
{
	size_t nc = findNeighbors(in);
	switch(m_eInterpolationMethod)
	{
		case Linear: interpolateLinear(nc, in, out, nullptr); break;
		case Mean: interpolateMean(nc, in, out, nullptr); break;
		case Learner: interpolateLearner(nc, in, out, nullptr); break;
		default:
			WAssert(false); // unexpected enumeration
			break;
	}
}

// virtual
void WKNN::predict(const WVec& in, WVec& out)
{
	size_t nc = findNeighbors(in);
	switch(m_eInterpolationMethod)
	{
		case Linear: interpolateLinear(nc, in, nullptr, &out); break;
		case Mean: interpolateMean(nc, in, nullptr, &out); break;
		case Learner: interpolateLearner(nc, in, nullptr, &out); break;
		default:
			WAssert(false); // unexpected enumeration
			break;
	}
}

// virtual
void WKNN::clear()
{
	delete(m_pNeighborFinder);
	m_pNeighborFinder = nullptr;
	delete(m_pFeatures);
	m_pFeatures = nullptr;
	delete(m_pSparseFeatures);
	m_pSparseFeatures = nullptr;
	delete(m_pLabels);
	m_pLabels = nullptr;
	delete(m_pScaleFactorOptimizer);
	m_pScaleFactorOptimizer = nullptr;
	delete(m_pCritic);
	m_pCritic = nullptr;
	m_valueCounts.resize(0);
}

//static
void WKNN::test()
{
	WKNN knn;
	knn.setNeighborCount(3);
	knn.basicTest(0.72, 0.92, 0.1);
}


// ---------------------------------------------------------------------------------------

WNeighborTransducer::WNeighborTransducer()
: WTransducer(), m_friendCount(12)
{
}

void WNeighborTransducer::autoTune(WMatrix& features, WMatrix& labels)
{
	// Find the best value for k
	size_t cap = size_t(floor(sqrt(double(features.rows()))));
	size_t bestK = 1;
	double bestErr = 1e308;
	for(size_t i = 1; i < cap; i *= 3)
	{
		m_friendCount = i;
		double d = crossValidate(features, labels, 2);
		if(d < bestErr)
		{
			bestErr = d;
			bestK = i;
		}
		else if(i >= 27)
			break;
	}

	// Set the best values
	m_friendCount = bestK;
}

// virtual
std::unique_ptr<WMatrix> WNeighborTransducer::transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2)
{
	// Make a dataset containing all rows
	WMatrix featuresAll(features1.relation().cloneMinimal());
	featuresAll.reserve(features1.rows() + features2.rows());
	WReleaseDataHolder hFeaturesAll(&featuresAll);
	for(size_t i = 0; i < features2.rows(); i++)
		featuresAll.takeRow((WVec*)&features2[i]);
	for(size_t i = 0; i < features1.rows(); i++)
		featuresAll.takeRow((WVec*)&features1[i]);
	auto out = std::unique_ptr<WMatrix>(new WMatrix(labels1.relation().clone()));
	out->newRows(features2.rows());

	// Find friends
	WNeighborFinder* pNF = new WNeighborGraph(new WKdTree(&featuresAll, nullptr, true), true, m_friendCount);
	std::unique_ptr<WNeighborFinder> hNF(pNF);

	// Transduce
	for(size_t lab = 0; lab < labels1.cols(); lab++)
	{
		size_t labelValues = labels1.relation().valueCount(lab);
		double* tallys = new double[labelValues];
		std::unique_ptr<double[]> hTallys(tallys);

		// Label the unlabeled patterns
		WBitTable labeled(features2.rows());
		WMatrix labelList(features2.rows(), 3); // pattern index, most likely label, confidence
		for(size_t i = 0; i < features2.rows(); i++)
			labelList[i][0] = (double)i;
		while(labelList.rows() > 0)
		{
			// Compute the most likely label and the confidence for each pattern
			for(size_t i = 0; i < labelList.rows(); i++)
			{
				// Find the most common label
				WVec& row = labelList.row(i);
				size_t index = (size_t)row[0];
				pNF->findNearest(m_friendCount, index);
				WVecWrapper vw2(tallys, labelValues);
				vw2.fill(0.0);
				for(size_t j = 0; j < m_friendCount; j++)
				{
					if(pNF->neighbor(j) >= features2.rows())
					{
						int label = (int)labels1[pNF->neighbor(j) - features2.rows()][lab];
						if(label >= 0 && label < (int)labelValues)
							tallys[label]++;
					}
					else if(labeled.bit(pNF->neighbor(j)))
					{
						int label = (int)out->row(pNF->neighbor(j))[lab];
						if(label >= 0 && label < (int)labelValues)
							tallys[label] += 0.6;
					}
				}

				WConstVecWrapper vw(tallys, labelValues);
				int label = (int)vw.indexOfMax();
				double conf = tallys[label];

				// Penalize for dissenting votes
				for(size_t j = 0; j < m_friendCount; j++)
				{
					if(pNF->neighbor(j) >= features2.rows())
					{
						int l2 = (int)labels1[pNF->neighbor(j) - features2.rows()][lab];
						if(l2 != label)
							conf *= 0.5;
					}
					else if(labeled.bit(pNF->neighbor(j)))
					{
						int l2 = (int)out->row(pNF->neighbor(j))[lab];
						if(l2 != label)
							conf *= 0.8;
					}
				}
				row[1] = label;
				row[2] = conf;
			}
			labelList.sort(2);

			// Assign the labels to the patterns we are most confident about
			size_t maxCount = std::max((size_t)4, features1.rows() / 8);
			size_t count = 0;
			for(size_t i = labelList.rows() - 1; i < labelList.rows(); i--)
			{
				WVec& row = labelList.row(i);
				size_t index = (size_t)row[0];
				int label = (int)row[1];
				out->row(index)[lab] = label;
				labeled.set(index);
				labelList.deleteRow(i);
				if(count >= maxCount)
					break;
				count++;
			}
		}
	}
	return out;
}

WInstanceTable::WInstanceTable(size_t dims, size_t* pDims)
: WIncrementalLearner(), m_dims(dims)
{
	m_pDims = new size_t[dims];
	memcpy(m_pDims, pDims, sizeof(size_t) * dims);
	m_product = 1;
	m_pScales = new size_t[dims];
	for(size_t i = 0; i < dims; i++)
	{
		m_pScales[i] = m_product;
		m_product *= pDims[i];
		m_pDims[i] = pDims[i];
	}
	m_pTable = nullptr;
	clear();
}

// virtual
WInstanceTable::~WInstanceTable()
{
	delete[] m_pDims;
	delete[] m_pScales;
	clear();
}

// virtual
WDomNode* WInstanceTable::serialize(WDom* pDoc) const
{
	throw Ex("not implemented yet");
	return nullptr;
}

// virtual
void WInstanceTable::trainSparse(WSparseMatrix& features, WMatrix& labels)
{
	throw Ex("Sorry, trainSparse is not implemented yet in WInstanceTable");
}

// virtual
void WInstanceTable::trainInner(const WMatrix& features, const WMatrix& labels)
{
	beginIncrementalLearningInner(features.relation(), labels.relation());
	for(size_t i = 0; i < features.rows(); i++)
		trainIncremental(features[i], labels[i]);
}

// virtual
void WInstanceTable::predictDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, this model cannot predict a distribution");
}

// virtual
void WInstanceTable::predict(const WVec& in, WVec& out)
{
	size_t pos = 0;
	for(size_t i = 0; i < m_dims; i++)
	{
		size_t n = (size_t)floor(in[i] + 0.5);
		if(n >= m_pDims[i])
			throw Ex("dim=", to_str(i), ", index=", to_str(in[i]), ", out of range. Expected >= 0 and < ", to_str(m_pDims[i]));
		pos += n * m_pScales[i];
	}
	size_t labelDims = m_pRelLabels->size();
	out.copy(m_pTable + pos * labelDims, labelDims);
}

// virtual
void WInstanceTable::clear()
{
	delete[] m_pTable;
	m_pTable = nullptr;
}

// virtual
void WInstanceTable::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	// Allocate the table
	clear();
	size_t total = m_product * labelRel.size();
	m_pTable = new double[total];

	// Initialize with small random values
	double* p = m_pTable;
	for(size_t i = 0; i < total; i++)
		*(p++) = m_rand.uniform() * 0.1;

	m_dims = featureRel.size();
}

// virtual
void WInstanceTable::trainIncremental(const WVec& in, const WVec& out)
{
	size_t pos = 0;
	for(size_t i = 0; i < m_dims; i++)
	{
		size_t n = (size_t)floor(in[i] + 0.5);
		if(n >= m_pDims[i])
			throw Ex("dim=", to_str(i), ", index=", to_str(in[i]), ", out of range. Expected >= 0 and < ", to_str(m_pDims[i]));
		pos += n * m_pScales[i];
	}
	size_t labelDims = m_pRelLabels->size();
	memcpy(m_pTable + pos * labelDims, out.data(), sizeof(double) * labelDims);
}











WSparseInstance::WSparseInstance()
: WSupervisedLearner(), m_neighborCount(1), m_pInstanceFeatures(nullptr), m_pInstanceLabels(nullptr), m_pMetric(nullptr), m_pSkipRows(nullptr)
{
}

// SAKBox: m_pSkipRows was left uninitialised, though predict reads it and
// clear() deletes it (it's only used during training, so null here)
WSparseInstance::WSparseInstance(const WDomNode* pNode)
: WSupervisedLearner(pNode), m_pSkipRows(nullptr)
{
	m_neighborCount = (size_t)pNode->getInt("neighbors");
	m_pInstanceFeatures = new WSparseMatrix(pNode->get("if"));
	m_pInstanceLabels = new WMatrix(pNode->get("il"));
	m_pMetric = WSparseSimilarity::deserialize(pNode->get("metric"));
}

WSparseInstance::~WSparseInstance()
{
	clear();
}

// virtual
WDomNode* WSparseInstance::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WSparseInstance");
	pNode->add(pDoc, "neighbors", m_neighborCount);
	pNode->add(pDoc, "if", m_pInstanceFeatures->serialize(pDoc));
	pNode->add(pDoc, "il", m_pInstanceLabels->serialize(pDoc));
	pNode->add(pDoc, "metric", m_pMetric->serialize(pDoc));
	return pNode;
}

void WSparseInstance::setNeighborCount(size_t k)
{
	m_neighborCount = k;
}

void WSparseInstance::trainInner(const WMatrix& features, const WMatrix& labels)
{
	// Split the training data into two parts
	if(features.rows() != labels.rows())
		throw Ex("Expected the same number of rows in the features and labels");
	clear();
	WMatrix f1(features.relation().clone());
	WMatrix f2(features.relation().clone());
	WMatrix l1(labels.relation().clone());
	WMatrix l2(labels.relation().clone());
	WReleaseDataHolder hf1(&f1);
	WReleaseDataHolder hf2(&f2);
	WReleaseDataHolder hl1(&l1);
	WReleaseDataHolder hl2(&l2);
	for(size_t i = 0; i < features.rows(); i++)
	{
		if(i & 1)
		{
			f2.takeRow((WVec*)&features[i]);
			l2.takeRow((WVec*)&labels[i]);
		}
		else
		{
			f1.takeRow((WVec*)&features[i]);
			l1.takeRow((WVec*)&labels[i]);
		}
	}

	// Copy the training portion of the data to be the stored instances
	m_pInstanceFeatures = new WSparseMatrix(f1.rows(), f1.cols(), UNKNOWN_REAL_VALUE);
	m_pInstanceFeatures->copyFrom(&f1);
	m_pInstanceLabels = new WMatrix();
	m_pInstanceLabels->copy(l1);

	// Prune the instances
	prune(f2, l2);
}

void WSparseInstance::prune(const WMatrix& holdOutFeatures, const WMatrix& holdOutLabels)
{

	// Make a list of all known elements
	vector< std::pair<size_t, size_t> > elements;
	for(size_t i = 0; i < m_pInstanceFeatures->rows(); i++)
	{
		SparseVec::const_iterator endit = m_pInstanceFeatures->rowEnd(i);
		for(SparseVec::const_iterator it = m_pInstanceFeatures->rowBegin(i); it != endit; it++)
			elements.push_back(std::pair<size_t, size_t>(i, it->first));
	}

	// Prune the stored instances
	m_pSkipRows = new WBitTable(m_pInstanceFeatures->rows());
	double bestErr = sumSquaredError(holdOutFeatures, holdOutLabels);
	while(elements.size() > 0)
	{
		bool improved = false;
		// SAKBox: shuffle with the learner's own (seeded) generator. Upstream
		// used std::random_shuffle, removed in C++17; its replacement here was
		// seeded from std::random_device, so every run came out differently.
		for(size_t i = elements.size(); i > 1; i--)
			std::swap(elements[i - 1], elements[(size_t)m_rand.next(i)]);
		// SAKBox: by index; an element is dropped by moving the last one into
		// its place, which is examined next. (This did it-- on an iterator,
		// which stepped before begin() when dropping the first element.)
		for(size_t k = 0; k < elements.size();)
		{
			std::pair<size_t, size_t>* it = &elements[k];
			if(m_pSkipRows->bit(it->first))
			{
				// drop the element from the list
				std::swap(*it, elements.back());
				elements.pop_back();
				continue;
			}
			else
			{
				// Try dropping an element
				double oldVal = m_pInstanceFeatures->get(it->first, it->second);
				m_pInstanceFeatures->set(it->first, it->second, UNKNOWN_REAL_VALUE);
				double candErr = sumSquaredError(holdOutFeatures, holdOutLabels);
				if(candErr <= bestErr)
				{
					bestErr = candErr;
					improved = true;

					// Drop the element from the list of elements to consider removing
					std::swap(*it, elements.back());
					elements.pop_back();
					continue;
				}
				else
				{
					// Put it back
					m_pInstanceFeatures->set(it->first, it->second, oldVal);

					// Try dropping the whole row
					m_pSkipRows->set(it->first);
					candErr = sumSquaredError(holdOutFeatures, holdOutLabels);
					if(candErr <= bestErr)
					{
						bestErr = candErr;
						improved = true;
					}
					else
						m_pSkipRows->unset(it->first); // Put it back
				}
			}
			k++;
		}
		if(!improved)
			break;
	}

	// Get rid of the black list
	for(size_t i = m_pInstanceFeatures->rows() - 1; i < m_pInstanceFeatures->rows(); i--)
	{
		if(m_pSkipRows->bit(i))
		{
			// SAKBox: the labels were swapped with m_pInstanceFeatures->rows() - 1,
			// taken after its last row was deleted, so one row short: from then
			// on rows and labels no longer matched
			m_pInstanceFeatures->swapRows(i, m_pInstanceFeatures->rows() - 1);
			m_pInstanceFeatures->deleteLastRow();
			m_pInstanceLabels->swapRows(i, m_pInstanceLabels->rows() - 1);
			m_pInstanceLabels->deleteRow(m_pInstanceLabels->rows() - 1);
		}
	}
	delete(m_pSkipRows);
	m_pSkipRows = nullptr;
}

void WSparseInstance::setMetric(WSparseSimilarity* pMetric)
{
	delete(m_pMetric);
	m_pMetric = pMetric;
}

// virtual
void WSparseInstance::predictDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, this learner cannot predict ditributions");
}

class WSparseInstance_comparator
{
public:
	bool operator()(const std::pair<double, size_t>& a, const std::pair<double, size_t>& b)
	{
		return a.first > b.first;
	}
};

// virtual
void WSparseInstance::predict(const WVec& in, WVec& out)
{
	// Make sure we have a metric to use
	if(!m_pMetric)
		setMetric(new WEuclidSimilarity());

	// Find the k-nearest neighbors
	priority_queue< std::pair<double, size_t>, std::vector<std::pair<double, size_t> >, WSparseInstance_comparator > neighbors;
	for(size_t i = 0; i < m_pInstanceFeatures->rows(); i++)
	{
		if(!m_pSkipRows || !m_pSkipRows->bit(i))
		{
			double similarity = m_pMetric->similarity(m_pInstanceFeatures->row(i), in);
			neighbors.push(std::pair<double, size_t>(similarity, i));
			if(neighbors.size() > m_neighborCount)
				neighbors.pop();
		}
	}

	// Combine the labels of the neighbors, weighted by similarity
	out.fill(0.0);
	double sumWeight = 0.0;
	while(neighbors.size() > 0)
	{
		size_t index = neighbors.top().second;
		double similarity = neighbors.top().first;
		out += (m_pInstanceLabels->row(index) * similarity);
		sumWeight += similarity;
		neighbors.pop();
	}
	out *= (1.0 / std::max(1e-12, sumWeight));
}

// virtual
void WSparseInstance::clear()
{
	delete(m_pInstanceFeatures);
	m_pInstanceFeatures = nullptr;
	delete(m_pInstanceLabels);
	m_pInstanceLabels = nullptr;
	delete(m_pMetric);
	m_pMetric = nullptr;
	delete(m_pSkipRows);
	m_pSkipRows = nullptr;
}

//static
void WSparseInstance::test()
{
	WSparseInstance learner;
	learner.setNeighborCount(3);
	// SAKBox: were 0.0, placeholders (and this test wasn't run); measured
	// 0.672 and 0.81
	learner.basicTest(0.65, 0.79);
}




} // namespace WClasses
