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

#include "WLearner.h"
#include <stdlib.h>
#include <string.h>
#include "WError.h"
#include "WVec.h"
#include "WHeap.h"
#include "WDom.h"
#include "WGaussianProcess.h"
#include "WImage.h"
#include "WKNN.h"
#include "WDecisionTree.h"
#include "WNaiveInstance.h"
#include "WNeuralNet.h"
#include "WLinear.h"
#include "WNaiveBayes.h"
#include "WEnsemble.h"
#include "WPolynomial.h"
#include "WTransform.h"
#include "WRand.h"
#include "WHolders.h"
#include "WPlot.h"
#include "WDistribution.h"
#include "WRecommender.h"
#include <cmath>
#include <iostream>

using std::vector;

namespace WClasses
{

WPrediction::~WPrediction()
{
	delete(m_pDistribution);
}

bool WPrediction::isContinuous()
{
	return m_pDistribution->type() == WUnivariateDistribution::normal;
}

// static
void WPrediction::predictionArrayToVector(size_t nOutputCount, WPrediction* pOutputs, WVec& vec)
{
	for(size_t i = 0; i < nOutputCount; i++)
		vec[i] = pOutputs[i].mode();
}

// static
void WPrediction::vectorToPredictionArray(WRelation* pRelation, size_t nOutputCount, WVec& vec, WPrediction* pOutputs)
{
	size_t nInputs = pRelation->size() - nOutputCount;
	for(size_t i = 0; i < nOutputCount; i++)
	{
		size_t nValueCount = pRelation->valueCount(nInputs + i);
		if(nValueCount == 0)
			pOutputs[i].makeNormal()->setMeanAndVariance(vec[i], 1);
		else
			pOutputs[i].makeCategorical()->setSpike(nValueCount, (size_t)vec[i], 1);
	}
}

double WPrediction::mode()
{
	return m_pDistribution->mode();
}


WCategoricalDistribution* WPrediction::makeCategorical()
{
	if(!m_pDistribution || m_pDistribution->type() != WUnivariateDistribution::categorical)
	{
		delete(m_pDistribution);
		m_pDistribution = new WCategoricalDistribution();
	}
	return (WCategoricalDistribution*)m_pDistribution;
}

WNormalDistribution* WPrediction::makeNormal()
{
	if(!m_pDistribution || m_pDistribution->type() != WUnivariateDistribution::normal)
	{
		delete(m_pDistribution);
		m_pDistribution = new WNormalDistribution();
	}
	return (WNormalDistribution*)m_pDistribution;
}

WCategoricalDistribution* WPrediction::asCategorical()
{
	if(!m_pDistribution || m_pDistribution->type() != WUnivariateDistribution::categorical)
		throw Ex("The current distribution is not a categorical distribution");
	return (WCategoricalDistribution*)m_pDistribution;
}

WNormalDistribution* WPrediction::asNormal()
{
	if(!m_pDistribution || m_pDistribution->type() != WUnivariateDistribution::normal)
		throw Ex("The current distribution is not a normal distribution");
	return (WNormalDistribution*)m_pDistribution;
}
// ---------------------------------------------------------------

WTransducer::WTransducer()
: m_rand(0)
{
}

WTransducer::~WTransducer()
{
}
class WTransducerTrainAndTestCleanUpper
{
protected:
	WMatrix* m_pData;
	size_t m_nTestSize;

public:
	WTransducerTrainAndTestCleanUpper(WMatrix* pData, size_t nTestSize)
	: m_pData(pData), m_nTestSize(nTestSize)
	{
	}

	~WTransducerTrainAndTestCleanUpper()
	{
		while(m_pData->rows() > m_nTestSize)
			m_pData->releaseRow(m_pData->rows() - 1);
	}
};

// virtual
std::unique_ptr<WMatrix> WTransducer::transduce(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2)
{
	if(features1.rows() != labels1.rows())
		throw Ex("Expected features1 and labels1 to have the same number of rows");
	if(features1.cols() != features2.cols())
		throw Ex("Expected both feature matrices to have the same number of cols");

	// Convert the features to a form that this algorithm can handle
	const WMatrix* pF1 = &features1;
	const WMatrix* pF2 = &features2;
	std::unique_ptr<WMatrix> hF1;
	std::unique_ptr<WMatrix> hF2;
	if(!canImplicitlyHandleNominalFeatures())
	{
		if(!canImplicitlyHandleContinuousFeatures())
			throw Ex("Can't handle nominal or continuous features");

		// Convert nominal features to continuous
		if(!features1.relation().areContinuous())
		{
			WNominalToCat ntc;
			ntc.train(*pF1);
			WMatrix* pTemp = ntc.transformBatch(*pF1);
			hF1.reset(pTemp);
			pF1 = pTemp;
			pTemp = ntc.transformBatch(*pF2);
			hF2.reset(pTemp);
			pF2 = pTemp;
		}
	}
	if(!canImplicitlyHandleContinuousFeatures())
	{
		if(!canImplicitlyHandleNominalFeatures())
			throw Ex("Can't handle nominal or continuous features");

		// Convert continuous features to nominal
		if(!features1.relation().areNominal())
		{
			WDiscretize disc;
			disc.train(*pF1); // todo: should really use both feature sets here
			WMatrix* pTemp = disc.transformBatch(*pF1);
			hF1.reset(pTemp);
			pF1 = pTemp;
			pTemp = disc.transformBatch(*pF2);
			hF2.reset(pTemp);
			pF2 = pTemp;
		}
	}
	if(canImplicitlyHandleContinuousFeatures())
	{
		// Normalize feature values to fall within a supported range
		double fmin, fmax;
		if(!supportedFeatureRange(&fmin, &fmax))
		{
			WNormalize norm(fmin, fmax);
			norm.train(*pF1); // todo: should really use both feature sets here
			WMatrix* pTemp = norm.transformBatch(*pF1);
			hF1.reset(pTemp);
			pF1 = pTemp;
			pTemp = norm.transformBatch(*pF2);
			hF2.reset(pTemp);
			pF2 = pTemp;
		}
	}

	// Take care of the labels
	if(!canImplicitlyHandleContinuousLabels())
	{
		if(!canImplicitlyHandleNominalLabels())
			throw Ex("This algorithm says it cannot handle nominal or continuous labels");
		if(labels1.relation().areNominal())
			return transduceInner(*pF1, labels1, *pF2);
		else
		{
			WDiscretize disc;
			disc.train(labels1);
			WMatrix* pL1 = disc.transformBatch(labels1);
			auto pL2 = transduceInner(*pF1, *pL1, *pF2);
			return std::unique_ptr<WMatrix>(disc.untransformBatch(*pL2));
		}
	}
	else
	{
		if(canImplicitlyHandleNominalLabels() || labels1.relation().areContinuous())
		{
			double lmin, lmax;
			if(supportedLabelRange(&lmin, &lmax))
				return transduceInner(*pF1, labels1, *pF2);
			else
			{
				WNormalize norm(lmin, lmax);
				norm.train(labels1);
				WMatrix* pL1 = norm.transformBatch(labels1);
				std::unique_ptr<WMatrix> hL1(pL1);
				auto pL2 = transduceInner(*pF1, *pL1, *pF2);
				return std::unique_ptr<WMatrix>(norm.untransformBatch(*pL2));
			}
		}
		else
		{
			double lmin, lmax;
			if(supportedLabelRange(&lmin, &lmax))
			{
				WNominalToCat ntc;
				ntc.train(labels1);
				WMatrix* pL1 = ntc.transformBatch(labels1);
				std::unique_ptr<WMatrix> hL1(pL1);
				auto pL2 = transduceInner(*pF1, *pL1, *pF2);
				return std::unique_ptr<WMatrix>(ntc.untransformBatch(*pL2));
			}
			else
			{
				// todo: both nominalToCat and normalization filters are necessary in this case
				throw Ex("case not yet supported");
				return std::unique_ptr<WMatrix>(nullptr);
			}
		}
	}
}

// virtual
double WTransducer::trainAndTest(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, double* pOutSAE)
{
	// Check assumptions
	if(testFeatures.rows() != testLabels.rows())
		throw Ex("Expected the test features to have the same number of rows as the test labels");
	if(trainFeatures.cols() != testFeatures.cols())
		throw Ex("Expected the training features and test features to have the same number of columns");

	// Transduce
	auto pPredictedLabels = transduce(trainFeatures, trainLabels, testFeatures);

	// Evaluate the results
	size_t labelDims = trainLabels.cols();
	double sse = 0.0;
	for(size_t i = 0; i < labelDims; i++)
		sse += testLabels.columnSumSquaredDifference(*pPredictedLabels, i, pOutSAE);
	return sse;
}

// virtual
void WTransducer::transductiveConfusionMatrix(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, std::vector<WMatrix*>& stats)
{
	// Check assumptions
	if(testFeatures.rows() != testLabels.rows())
		throw Ex("Expected the test features to have the same number of rows as the test labels");
	if(trainFeatures.cols() != testFeatures.cols())
		throw Ex("Expected the training features and test features to have the same number of columns");

	// Transduce
	auto pPredictedLabels = transduce(trainFeatures, trainLabels, testFeatures);

	// Evaluate the results
	size_t labelDims = trainLabels.cols();
	stats.resize(labelDims);
	for(size_t j = 0; j < labelDims; j++)
	{
		size_t vals = testLabels.relation().valueCount(j);
		if(vals > 0)
		{
			stats[j] = new WMatrix(vals, vals);
			stats[j]->fill(0.0);
		}
		else
			stats[j] = nullptr;
	}
	for(size_t i = 0; i < pPredictedLabels->rows(); i++)
	{
		const WVec& target = testLabels[i];
		WVec& pred = pPredictedLabels->row(i);
		for(size_t j = 0; j < labelDims; j++)
		{
			if(stats[j])
			{
				if((int)target[j] >= 0 && (int)pred[j] >= 0)
					stats[j]->row((int)target[j])[(int)pred[j]]++;
			}
		}
	}
}

double WTransducer::crossValidate(const WMatrix& features, const WMatrix& labels, size_t folds, double* pOutSAE, RepValidateCallback pCB, size_t nRep, void* pThis)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected the features and labels to have the same number of rows");

	// Do cross-validation
	WMatrix trainFeatures(features.relation().cloneMinimal());
	trainFeatures.reserve(features.rows());
	WMatrix testFeatures(features.relation().cloneMinimal());
	testFeatures.reserve(features.rows() / folds + 1);
	WMatrix trainLabels(labels.relation().cloneMinimal());
	trainLabels.reserve(labels.rows());
	WMatrix testLabels(labels.relation().cloneMinimal());
	testLabels.reserve(labels.rows() / folds + 1);
	double sse = 0.0;
	for(size_t i = 0; i < folds; i++)
	{
		// Divide into a training set and a test set
		WReleaseDataHolder hTrainFeatures(&trainFeatures);
		WReleaseDataHolder hTestFeatures(&testFeatures);
		WReleaseDataHolder hTrainLabels(&trainLabels);
		WReleaseDataHolder hTestLabels(&testLabels);
		size_t foldStart = i * features.rows() / folds;
		size_t foldEnd = (i + 1) * features.rows() / folds;
		for(size_t j = 0; j < foldStart; j++)
		{
			trainFeatures.takeRow((WVec*)&features[j]);
			trainLabels.takeRow((WVec*)&labels[j]);
		}
		for(size_t j = foldStart; j < foldEnd; j++)
		{
			testFeatures.takeRow((WVec*)&features[j]);
			testLabels.takeRow((WVec*)&labels[j]);
		}
		for(size_t j = foldEnd; j < features.rows(); j++)
		{
			trainFeatures.takeRow((WVec*)&features[j]);
			trainLabels.takeRow((WVec*)&labels[j]);
		}

		// Evaluate
		double foldsse = trainAndTest(trainFeatures, trainLabels, testFeatures, testLabels, pOutSAE);
		sse += foldsse;
		if(pCB)
			pCB(pThis, nRep, i, foldsse, testLabels.rows());
	}
	return sse;
}

double WTransducer::repValidate(const WMatrix& features, const WMatrix& labels, size_t reps, size_t folds, double* pOutSAE, RepValidateCallback pCB, void* pThis)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected the features and labels to have the same number of rows");
	WMatrix f(features.relation().cloneMinimal());
	WReleaseDataHolder hF(&f);
	WMatrix l(labels.relation().cloneMinimal());
	WReleaseDataHolder hL(&l);
	for(size_t i = 0; i < features.rows(); i++)
	{
		f.takeRow((WVec*)&features[i]);
		l.takeRow((WVec*)&labels[i]);
	}
	double ssse = 0.0;
	for(size_t i = 0; i < reps; i++)
	{
		f.shuffle(m_rand, &l);
		ssse += crossValidate(f, l, folds, pOutSAE, pCB, i, pThis);
	}
	return ssse / reps;
}

// ---------------------------------------------------------------

WSupervisedLearner::WSupervisedLearner()
: WTransducer(), m_pRelFeatures(nullptr), m_pRelLabels(nullptr)
{
}

WSupervisedLearner::WSupervisedLearner(const WDomNode* pNode)
: WTransducer()
{
	m_pRelFeatures = WRelation::deserialize(pNode->get("_rf"));
	m_pRelLabels = WRelation::deserialize(pNode->get("_rl"));
}

WSupervisedLearner::~WSupervisedLearner()
{
	delete(m_pRelFeatures);
	delete(m_pRelLabels);
}

const WRelation& WSupervisedLearner::relFeatures()
{
	if(!m_pRelFeatures)
		throw Ex("Training has not begun yet");
	return *m_pRelFeatures;
}

const WRelation& WSupervisedLearner::relLabels()
{
	if(!m_pRelLabels)
		throw Ex("Training has not begun yet");
	return *m_pRelLabels;
}

WDomNode* WSupervisedLearner::baseDomNode(WDom* pDoc, const char* szClassName) const
{
	if(!m_pRelLabels)
		throw Ex("The model must be trained before it is serialized.");
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "class", szClassName);
	pNode->add(pDoc, "_rf", m_pRelFeatures->serialize(pDoc));
	pNode->add(pDoc, "_rl", m_pRelLabels->serialize(pDoc));
	return pNode;
}

std::string to_str(const WSupervisedLearner& learner)
{
	WDom doc;
	learner.serialize(&doc);
	return to_str(doc);
}

void WSupervisedLearner::train(const WMatrix& features, const WMatrix& labels)
{
	// Check assumptions
	if(features.rows() != labels.rows())
		throw Ex("Expected features and labels to have the same number of rows");
	if(labels.cols() == 0)
		throw Ex("Expected at least one label dimension");
	delete(m_pRelFeatures);
	m_pRelFeatures = features.relation().cloneMinimal();
	delete(m_pRelLabels);
	m_pRelLabels = labels.relation().clone(); // not cloneMinimal() because
	// we need the columns's data
	// when doing 'predictdistribution'
	trainInner(features, labels);
}

void WSupervisedLearner::confusion(WMatrix& features, WMatrix& labels, std::vector<WMatrix*>& stats)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected the features and rows to have the same number of rows");
	size_t labelDims = labels.cols();
	stats.resize(labelDims);
	for(size_t j = 0; j < labelDims; j++)
	{
		size_t vals = labels.relation().valueCount(j);
		if(vals > 0)
		{
			stats[j] = new WMatrix(vals, vals);
			stats[j]->fill(0.0);
		}
		else
			stats[j] = nullptr;
	}
	WVec prediction(labelDims);
	for(size_t i = 0; i < features.rows(); i++)
	{
		predict(features[i], prediction);
		WVec& target = labels[i];
		for(size_t j = 0; j < labelDims; j++)
		{
			if(labels.relation().valueCount(j) > 0)
			{
				if((int)target[j] >= 0 && (int)prediction[j] >= 0)
					stats[j]->row((int)target[j])[(int)prediction[j]]++;
			}
		}
	}
}

double WSupervisedLearner::sumSquaredError(const WMatrix& features, const WMatrix& labels, double* pOutSAE)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected the features and labels to have the same number of rows");
	if(!m_pRelFeatures->isCompatible(features.relation()))
		throw Ex("Features incompatible with this learner");
	if(!m_pRelLabels->isCompatible(labels.relation()))
		throw Ex("Labels incompatible with this learner");
	size_t labelDims = labels.cols();
	WVec prediction(labelDims);
	double sae = 0.0;
	double sse = 0.0;
	for(size_t i = 0; i < features.rows(); i++)
	{
		predict(features[i], prediction);
		const WVec& targ = labels[i];
		for(size_t j = 0; j < labelDims; j++)
		{
			if(labels.relation().valueCount(j) == 0)
			{
				if(targ[j] != UNKNOWN_REAL_VALUE)
				{
					double d = targ[j] - prediction[j];
					sse += (d * d);
					sae += std::abs(d);
				}
			}
			else
			{
				if(targ[j] != UNKNOWN_DISCRETE_VALUE && (int)targ[j] != (int)prediction[j])
				{
					sse += 1.0;
					sae += 1.0;
				}
			}
		}
	}
	if(pOutSAE)
		*pOutSAE = sae;
	return sse;
}

// virtual
std::unique_ptr<WMatrix> WSupervisedLearner::transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2)
{
	// Train
	train(features1, labels1);

	// Predict
	auto pOut = std::unique_ptr<WMatrix>(new WMatrix(labels1.relation().clone()));
	pOut->newRows(features2.rows());
	for(size_t i = 0; i < features2.rows(); i++)
		predict(features2.row(i), pOut->row(i));
	return pOut;
}

// virtual
double WSupervisedLearner::trainAndTest(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, double* pOutSAE)
{
	train(trainFeatures, trainLabels);
	return sumSquaredError(testFeatures, testLabels);
}

size_t WSupervisedLearner::precisionRecallContinuous(WPrediction* pOutput, double* pFunc, WMatrix& trainFeatures, WMatrix& trainLabels, WMatrix& testFeatures, WMatrix& testLabels, size_t label)
{
	// Predict the variance for each pattern
	train(trainFeatures, trainLabels);
	WMatrix stats(testFeatures.rows(), 2);
	for(size_t i = 0; i < testFeatures.rows(); i++)
	{
		predictDistribution(testFeatures[i], pOutput);
		WVec& resultsVec = stats.row(i);
		resultsVec[0] = testLabels[i][label];
		if(resultsVec[0] < 0.0 || resultsVec[0] > 1.0)
			throw Ex("Expected continuous labels to range from 0 to 1");
		WNormalDistribution* pDist = pOutput[label].asNormal();
		resultsVec[1] = pDist->mean();
	}

	// Make the precision/recall data
	stats.sort(1); // biggest mean last
	stats.reverseRows(); // biggest mean first
	double sumRelevantRetrieved = 0.0;
	for(size_t i = 0; i < stats.rows(); i++)
	{
		WVec& vecIn = stats.row(i);
		sumRelevantRetrieved += vecIn[0];
		pFunc[i] = sumRelevantRetrieved / (i + 1);
	}
	return stats.rows();
}

size_t WSupervisedLearner::precisionRecallNominal(WPrediction* pOutput, double* pFunc, WMatrix& trainFeatures, WMatrix& trainLabels, WMatrix& testFeatures, WMatrix& testLabels, size_t label, int value)
{
	// Predict the likelihood that each pattern is relevant
	train(trainFeatures, trainLabels);
	WMatrix stats(testFeatures.rows(), 2);
	size_t nActualRelevant = 0;
	for(size_t i = 0; i < testFeatures.rows(); i++)
	{
		predictDistribution(testFeatures[i], pOutput);
		WVec& statsVec = stats.row(i);
		statsVec[0] = testLabels[i][label];
		if((int)statsVec[0] == value)
			nActualRelevant++;
		WCategoricalDistribution* pDist = pOutput[label].asCategorical();
		statsVec[1] = pDist->likelihood((double)value); // predicted confidence that it is relevant
	}

	// Make the precision/recall data
	stats.sort(1); // most confident last
	size_t nFoundRelevant = 0;
	size_t nFoundTotal = 0;
	for(size_t i = stats.rows() - 1; i < stats.rows(); i--)
	{
		WVec& vecIn = stats.row(i);
		nFoundTotal++;
		if((int)vecIn[0] == value) // if actually relevant
		{
			nFoundRelevant++;
			if(nFoundTotal <= 1)
				pFunc[nFoundRelevant - 1] = 1.0;
			else
				pFunc[nFoundRelevant - 1] = (double)(nFoundRelevant - 1) / (nFoundTotal - 1);
		}
	}
	WAssert(nFoundRelevant == nActualRelevant);
	return nActualRelevant;
}

void WSupervisedLearner::addInterpolatedFunction(double* pOut, size_t nOutVals, double* pIn, size_t nInVals)
{
	if(nInVals > nOutVals)
	{
		size_t inPos = 0;
		size_t outPos, n, count;
		double d;
		for(outPos = 0; outPos < nOutVals; outPos++)
		{
			n = outPos * nInVals / nOutVals;
			d = 0;
			count = 0;
			while(inPos <= n)
			{
				d += pIn[inPos++];
				count++;
			}
			pOut[outPos] += d / count;
		}
	}
	else if(nInVals < nOutVals)
	{
		double d;
		size_t n, i, j;
		for(n = 0; n < nOutVals; n++)
		{
			d = (double)n * nInVals / nOutVals;
			i = (int)d;
			j = std::min(i + 1, nInVals - 1);
			d -= i;
			pOut[n] += ((1.0 - d) * pIn[i] + d * pIn[j]);
		}
	}
	else
	{
		for(size_t n = 0; n < nOutVals; n++)
			pOut[n] += pIn[n];
	}
}

void WSupervisedLearner::precisionRecall(double* pOutPrecision, size_t nPrecisionSize, WMatrix& features, WMatrix& labels, size_t label, size_t nReps)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected the features and labels to have the same number of rows");
	size_t nFuncs = std::max((size_t)1, labels.relation().valueCount(label));
	WVecWrapper vw2(pOutPrecision, nFuncs * nPrecisionSize);
	vw2.fill(0.0);
	double* pFunc = new double[features.rows()];
	std::unique_ptr<double[]> hFunc(pFunc);
	WPrediction* out = new WPrediction[labels.cols()];
	std::unique_ptr<WPrediction[]> hOut(out);
	WMatrix otherFeatures(features.relation().cloneMinimal());
	WMatrix otherLabels(labels.relation().cloneMinimal());
	size_t valueCount = labels.relation().valueCount(label);
	for(size_t nRep = 0; nRep < nReps; nRep++)
	{
		// Split the data
		WMergeDataHolder hFeatures(features, otherFeatures);
		WMergeDataHolder hLabels(labels, otherLabels);
		features.shuffle(m_rand, &labels);
		size_t otherSize = features.rows() / 2;
		features.splitBySize(otherFeatures, otherSize);
		labels.splitBySize(otherLabels, otherSize);

		// Measure precision/recall and merge with the data we've gotten so far
		if(valueCount == 0)
		{
			size_t relevant = precisionRecallContinuous(out, pFunc, features, labels, otherFeatures, otherLabels, label);
			addInterpolatedFunction(pOutPrecision, nPrecisionSize, pFunc, relevant);
			relevant = precisionRecallContinuous(out, pFunc, otherFeatures, otherLabels, features, labels, label);
			addInterpolatedFunction(pOutPrecision, nPrecisionSize, pFunc, relevant);
		}
		else
		{
			for(int i = 0; i < (int)valueCount; i++)
			{
				size_t relevant = precisionRecallNominal(out, pFunc, features, labels, otherFeatures, otherLabels, label, i);
				addInterpolatedFunction(pOutPrecision + nPrecisionSize * i, nPrecisionSize, pFunc, relevant);
				relevant = precisionRecallNominal(out, pFunc, otherFeatures, otherLabels, features, labels, label, i);
				addInterpolatedFunction(pOutPrecision + nPrecisionSize * i, nPrecisionSize, pFunc, relevant);
			}
		}
	}
	WVecWrapper vw(pOutPrecision, nFuncs * nPrecisionSize);
	vw *= (1.0 / (2 * nReps));
}


#define TEST_SIZE 5000
// static
void WSupervisedLearner::test()
{
	/*	// Make a probabilistic training set
	WRand rand(0);
	vector<size_t> vals1;
	vals1.push_back(3);
	vector<size_t> vals2;
	vals2.push_back(2);
	WMatrix f(vals1);
	WMatrix l(vals2);
	f.newRows(TEST_SIZE);
	l.newRows(TEST_SIZE);
	for(size_t i = 0; i < TEST_SIZE; i++)
	{
		size_t n = size_t(rand.next(3));
		if(n == 0)
		{
			if(rand.uniform() < 0.15)
				l[i][0] = 0;
			else
				l[i][0] = 1;
		}
		else if(n == 1)
		{
			if(rand.uniform() < 0.3)
				l[i][0] = 0;
			else
				l[i][0] = 1;
		}
		else
		{
			if(rand.uniform() < 0.85)
				l[i][0] = 0;
			else
				l[i][0] = 1;
		}
		f[i][0] = double(n);
	}

	// Train the model
	WNeuralNetLearner model;
	model.addLayer(new WNeuralNetLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE));
	model.train(f, l);
	WPrediction out;
	double d, prob;

// Uncomment this block if you want to see how it does without calibration (which should be a little worse than with it).
// 	d = 0;
// 	model.predictDistribution(&d, &out);
// 	prob = out.asCategorical()->values(2)[0];
// 	d = 1;
// 	model.predictDistribution(&d, &out);
// 	prob = out.asCategorical()->values(2)[0];
// 	d = 2;
// 	model.predictDistribution(&d, &out);
// 	prob = out.asCategorical()->values(2)[0];

	// Calibrate the model
	model.calibrate(f, l);

	// Check that the predicted distributions are close to the expected distributions
	d = 0;
	model.predictDistribution(&d, &out);
	prob = out.asCategorical()->values(2)[0];
	if(std::abs(prob - 0.15) > .11)
		throw Ex("failed");
	d = 1;
	model.predictDistribution(&d, &out);
	prob = out.asCategorical()->values(2)[0];
	if(std::abs(prob - 0.30) > .16)
		throw Ex("failed");
	d = 2;
	model.predictDistribution(&d, &out);
	prob = out.asCategorical()->values(2)[0];
	if(std::abs(prob - 0.85) > .11)
		throw Ex("failed");*/
}

void WSupervisedLearner_basicTestEngine(WSupervisedLearner* pLearner, WMatrix& features, WMatrix& labels, WMatrix& testFeatures, WMatrix& testLabels, double minAccuracy, WRand* pRand, double warnRange, double deviation, bool printAccuracy)
{
	// Train the model
	pLearner->train(features, labels);

	// free up some memory, just because we can
	features.flush();
	labels.flush();

	// Test the accuracy
	double resultsBefore = 1.0 - pLearner->sumSquaredError(testFeatures, testLabels) / testFeatures.rows();
	if(printAccuracy)
	{
		std::cerr << "AccBeforeSerial: " << resultsBefore;
	}
	if(resultsBefore < minAccuracy)
		throw Ex("accuracy has regressed. Expected at least ", to_str(minAccuracy), ". Only got ", to_str(resultsBefore), ". (Sometimes, harmless changes that affect random orderings can trigger small regressions, so don't panic yet.)");
	if(resultsBefore >= minAccuracy + warnRange)
		std::cout << "\nThe measured accuracy (" << resultsBefore << ") is much better than expected (" << minAccuracy << "). Please increase the expected accuracy value so that any future regressions will be caught.\n";

	// Roundtrip the model through serialization
	const WRelation& relLabelsBefore = pLearner->relLabels();
	WDom doc;
	doc.setRoot(pLearner->serialize(&doc));
	pLearner->clear(); // free up some memory, just because we can
	WLearnerLoader ll;
	WSupervisedLearner* pModel = ll.loadLearner(doc.root());
	std::unique_ptr<WSupervisedLearner> hModel(pModel);
	if(!relLabelsBefore.isCompatible(pModel->relLabels()))
		throw Ex("The label relation failed to round-trip. Did your deserializing constructor call the base class constructor?");

	// Test the accuracy again
	double resultsAfter = 1.0 - pModel->sumSquaredError(testFeatures, testLabels) / testFeatures.rows();
	if(printAccuracy)
	{
		std::cerr << "  AccAfterSerial: " << resultsAfter << std::endl;
	}
	if(std::abs(resultsAfter - resultsBefore) > deviation)
		throw Ex("serialization shouldn't influence accuracy this much");
}

void WSupervisedLearner_basicTest1(WSupervisedLearner* pLearner, double minAccuracy, WRand* pRand, double warnRange, double deviation, bool printAccuracy)
{
	WMatrix features(0, 2);
	vector<size_t> vals;
	vals.push_back(3);
	WMatrix labels(vals);
	for(size_t i = 0; i < 2000; i++)
	{
		int c = (int)pRand->next(3);
		WVec& f = features.newRow();
		f[0] = pRand->normal() + (c == 1 ? 2.0 : 0.0);
		f[1] = pRand->normal() + (c == 2 ? 2.0 : 0.0);
		WVec& l = labels.newRow();
		l[0] = (double)c;
	}
	size_t testSize = features.rows() / 2;
	WMatrix testFeatures(features.relation().clone());
	features.splitBySize(testFeatures, testSize);
	WMatrix testLabels(labels.relation().clone());
	labels.splitBySize(testLabels, testSize);
	WSupervisedLearner_basicTestEngine(pLearner, features, labels, testFeatures, testLabels, minAccuracy, pRand, warnRange, deviation, printAccuracy);
}

void WSupervisedLearner_basicTest2(WSupervisedLearner* pLearner, double minAccuracy, WRand* pRand, double warnRange, double deviation, bool printAccuracy)
{
	if(minAccuracy == -1.0)
		return; // skip this test
	vector<size_t> featureVals;
	size_t cols = 10;
	featureVals.reserve(cols);
	for(size_t i = 0; i < cols; i++)
		featureVals.push_back(3);
	WMatrix features(featureVals);
	vector<size_t> labelVals;
	labelVals.push_back(3);
	WMatrix labels(labelVals);
	for(size_t i = 0; i < 1000; i++)
	{
		int c = (int)pRand->next(3);
		WVec& f = features.newRow();
		for(size_t j = 0; j < cols; j++)
		{
			if(pRand->next(2) == 0)
				f[j] = (double)c;
			else
				f[j] = (double)pRand->next(3);
		}
		WVec& l = labels.newRow();
		l[0] = (double)c;
	}
	size_t testSize = features.rows() / 2;
	WMatrix testFeatures(features.relation().clone());
	features.splitBySize(testFeatures, testSize);
	WMatrix testLabels(labels.relation().clone());
	labels.splitBySize(testLabels, testSize);
	WSupervisedLearner_basicTestEngine(pLearner, features, labels, testFeatures, testLabels, minAccuracy, pRand, warnRange, deviation, printAccuracy);
}

void WSupervisedLearner::basicTest(double minAccuracy1, double minAccuracy2, double deviation, bool printAccuracy, double warnRange)
{
	WSupervisedLearner_basicTest1(this, minAccuracy1, &m_rand, warnRange, deviation, printAccuracy);
	WSupervisedLearner_basicTest2(this, minAccuracy2, &m_rand, warnRange, deviation * 2, printAccuracy);
}

// ---------------------------------------------------------------

void WIncrementalLearner::beginIncrementalLearning(const WRelation& featureRel, const WRelation& labelRel)
{
	delete(m_pRelFeatures);
	m_pRelFeatures = featureRel.cloneMinimal();
	delete(m_pRelLabels);
	m_pRelLabels = labelRel.cloneMinimal();
	beginIncrementalLearningInner(featureRel, labelRel);
}

void WIncrementalLearner::beginIncrementalLearning(const WMatrix& features, const WMatrix& labels)
{
	delete(m_pRelFeatures);
	m_pRelFeatures = features.relation().cloneMinimal();
	delete(m_pRelLabels);
	m_pRelLabels = labels.relation().cloneMinimal();
	beginIncrementalLearningInner(features, labels);
}

// ---------------------------------------------------------------

// virtual
WIncrementalTransform* WLearnerLoader::loadIncrementalTransform(const WDomNode* pNode)
{
	const char* szClass = pNode->getString("class");
	if(szClass[0] == 'W')
	{
		if(szClass[1] < 'N')
		{
			if(szClass[1] < 'I')
			{
				if(strcmp(szClass, "WAttributeSelector") == 0)
					return new WAttributeSelector(pNode);
				else if(strcmp(szClass, "WDataAugmenter") == 0)
					return new WDataAugmenter(pNode, *this);
				else if(strcmp(szClass, "WDiscretize") == 0)
					return new WDiscretize(pNode);
			}
			else
			{
				if(strcmp(szClass, "WImputeMissingVals") == 0)
					return new WImputeMissingVals(pNode, *this);
				else if(strcmp(szClass, "WIncrementalTransformChainer") == 0)
					return new WIncrementalTransformChainer(pNode, *this);
			}
		}
		else
		{
			if(szClass[1] < 'P')
			{
				if(strcmp(szClass, "WNoiseGenerator") == 0)
					return new WNoiseGenerator(pNode);
				else if(strcmp(szClass, "WNominalToCat") == 0)
					return new WNominalToCat(pNode);
				else if(strcmp(szClass, "WNormalize") == 0)
					return new WNormalize(pNode);
			}
			else
			{
				if(strcmp(szClass, "WPairProduct") == 0)
					return new WPairProduct(pNode);
				else if(strcmp(szClass, "WPCA") == 0)
					return new WPCA(pNode);
			}
		}
	}
	if(m_throwIfClassNotFound)
		throw Ex("Unrecognized class: ", szClass);
	return nullptr;
}

// virtual
WSupervisedLearner* WLearnerLoader::loadLearner(const WDomNode* pNode)
{
	const char* szClass = pNode->getString("class");
	if(szClass[0] == 'W')
	{
		if(szClass[1] < 'J')
		{
			if(szClass[1] < 'C')
			{
				if(strcmp(szClass, "WAutoFilter") == 0)
					return new WAutoFilter(pNode, *this);
				else if(strcmp(szClass, "WBag") == 0)
					return new WBag(pNode, *this);
				else if(strcmp(szClass, "WBaselineLearner") == 0)
					return new WBaselineLearner(pNode);
				else if(strcmp(szClass, "WBayesianModelAveraging") == 0)
					return new WBayesianModelAveraging(pNode, *this);
				else if(strcmp(szClass, "WBayesianModelCombination") == 0)
					return new WBayesianModelCombination(pNode, *this);
				else if(strcmp(szClass, "WBucket") == 0)
					return new WBucket(pNode, *this);
			}
			else
			{
				if(strcmp(szClass, "WDecisionTree") == 0)
					return new WDecisionTree(pNode);
				else if(strcmp(szClass, "WFeatureFilter") == 0)
					return new WFeatureFilter(pNode, *this);
				else if(strcmp(szClass, "WGaussianProcess") == 0)
					return new WGaussianProcess(pNode);
				else if(strcmp(szClass, "WIdentityFunction") == 0)
					return new WIdentityFunction(pNode);
			}
		}
		else
		{
			if(szClass[1] < 'N')
			{
				if(strcmp(szClass, "WLinearDistribution") == 0)
					return new WLinearDistribution(pNode);
				else if(strcmp(szClass, "WKNN") == 0)
					return new WKNN(pNode);
				else if(strcmp(szClass, "WLabelFilter") == 0)
					return new WLabelFilter(pNode, *this);
				else if(strcmp(szClass, "WLinearRegressor") == 0)
					return new WLinearRegressor(pNode);
				else if(strcmp(szClass, "WMeanMarginsTree") == 0)
					return new WMeanMarginsTree(pNode);
			}
			else
			{
				if(szClass[1] < 'P')
				{
					if(strcmp(szClass, "WNaiveBayes") == 0)
						return new WNaiveBayes(pNode);
					else if(strcmp(szClass, "WNaiveInstance") == 0)
						return new WNaiveInstance(pNode);
					//					else if(strcmp(szClass, "WNeuralNetLearner") == 0)
					//						return new WNeuralNetLearner(pNode);
				}
				else
				{
					if(strcmp(szClass, "WPolynomial") == 0)
						return new WPolynomial(pNode);
					else if(strcmp(szClass, "WRandomForest") == 0)
						return new WRandomForest(pNode, *this);
					else if(strcmp(szClass, "WResamplingAdaBoost") == 0)
						return new WResamplingAdaBoost(pNode, *this);
					else if(strcmp(szClass, "WSparseInstance") == 0) // SAKBox: was missing, so a saved one couldn't be loaded
						return new WSparseInstance(pNode);
				}
			}
		}
	}
	if(m_throwIfClassNotFound)
		throw Ex("Unrecognized class: ", szClass);
	return nullptr;
}

// virtual
WCollaborativeFilter* WLearnerLoader::loadCollaborativeFilter(const WDomNode* pNode)
{
	const char* szClass = pNode->getString("class");
	if(szClass[0] == 'W')
	{
		if(strcmp(szClass, "WBagOfRecommenders") == 0)
			return new WBagOfRecommenders(pNode, *this);
		else if(strcmp(szClass, "WBaselineRecommender") == 0)
			return new WBaselineRecommender(pNode, *this);
		else if(strcmp(szClass, "WMatrixFactorization") == 0)
			return new WMatrixFactorization(pNode, *this);
		//		else if(strcmp(szClass, "WNeuralRecommender") == 0)
		//			return new WNonlinearPCA(pNode, *this);
	}
	if(m_throwIfClassNotFound)
		throw Ex("Unrecognized class: ", szClass);
	return nullptr;
}

// ---------------------------------------------------------------

WFilter::WFilter(WSupervisedLearner* pLearner, bool ownLearner)
: WIncrementalLearner(), m_pLearner(pLearner), m_pIncrementalLearner(nullptr), m_pOriginal(pLearner), m_ownLearner(ownLearner)
{
	if(pLearner->canTrainIncrementally())
		m_pIncrementalLearner = (WIncrementalLearner*)pLearner;
}

WFilter::WFilter(const WDomNode* pNode, WLearnerLoader& ll)
: WIncrementalLearner(pNode), m_pIncrementalLearner(nullptr), m_ownLearner(true)
{
	m_pLearner = ll.loadLearner(pNode->get("learner"));
	if(m_pLearner->canTrainIncrementally())
		m_pIncrementalLearner = (WIncrementalLearner*)m_pLearner;
	m_pOriginal = m_pLearner;
}

// virtual
WFilter::~WFilter()
{
	if(m_pLearner)
	{
		discardIntermediateFilters();
		if(m_ownLearner)
			delete(m_pLearner);
	}
}

// virtual
void WFilter::clear()
{
	m_pLearner->clear();
}

void WFilter::initShellOnly(const WRelation& featureRel, const WRelation& labelRel)
{
	delete(m_pRelFeatures);
	m_pRelFeatures = featureRel.cloneMinimal();
	delete(m_pRelLabels);
	m_pRelLabels = labelRel.cloneMinimal();
}

void WFilter::discardIntermediateFilters()
{
	discardIntermediateFilters_helper(m_pOriginal);
	WAssert(m_pLearner == m_pOriginal);
}

void WFilter::discardIntermediateFilters_helper(WSupervisedLearner* pOriginal)
{
	if(m_pLearner != pOriginal && m_pLearner->isFilter())
	{
		// Recurse
		WFilter* pIntermediateFilter = (WFilter*)m_pLearner;
		pIntermediateFilter->discardIntermediateFilters_helper(pOriginal);

		// Nuke the intermediate filter, and wrap whatever it used to wrap
		WSupervisedLearner* pTemp = pIntermediateFilter->m_pLearner;
		pIntermediateFilter->m_pLearner = nullptr;
		pIntermediateFilter->m_pIncrementalLearner = nullptr;
		pIntermediateFilter->m_pOriginal = nullptr;
		delete(m_pLearner);
		m_pLearner = pTemp;
		if(m_pLearner->canTrainIncrementally())
			m_pIncrementalLearner = (WIncrementalLearner*)m_pLearner;
		else
			m_pIncrementalLearner = nullptr;
	}
}

WDomNode* WFilter::domNode(WDom* pDoc, const char* szClassName) const
{
	WDomNode* pNode = baseDomNode(pDoc, szClassName);
	pNode->add(pDoc, "learner", m_pLearner->serialize(pDoc));
	return pNode;
}

const WVec& WFilter::prefilterFeatures(const WVec& in)
{
	const WVec* pOut = transformedFeatures(in);
	return pOut ? *pOut : in;
}

const WVec& WFilter::prefilterLabels(const WVec& in)
{
	const WVec* pOut = transformedLabels(in);
	return pOut ? *pOut : in;
}

WMatrix* WFilter::prefilterFeatures(const WMatrix& in)
{
	WSupervisedLearner* pInnerLearner = m_pLearner;
	while(pInnerLearner->isFilter())
		pInnerLearner = ((WFilter*)pInnerLearner)->m_pLearner;
	WMatrix* pOut = new WMatrix(pInnerLearner->relFeatures().clone());
	pOut->newRows(in.rows());
	for(size_t i = 0; i < in.rows(); i++)
		pOut->row(i).copy(prefilterFeatures(in[i]));
	return pOut;
}

WMatrix* WFilter::prefilterLabels(const WMatrix& in)
{
	WSupervisedLearner* pInnerLearner = m_pLearner;
	while(pInnerLearner->isFilter())
		pInnerLearner = ((WFilter*)pInnerLearner)->m_pLearner;
	WMatrix* pOut = new WMatrix(pInnerLearner->relLabels().clone());
	pOut->newRows(in.rows());
	for(size_t i = 0; i < in.rows(); i++)
		pOut->row(i).copy(prefilterLabels(in[i]));
	return pOut;
}

// virtual
void WFilter::trainSparse(WSparseMatrix& features, WMatrix& labels)
{
	throw Ex("Sorry, this method has not been implemented");
}

// ---------------------------------------------------------------

WFeatureFilter::WFeatureFilter(WSupervisedLearner* pLearner, WIncrementalTransform* pTransform, bool ownLearner, bool ownTransform)
: WFilter(pLearner, ownLearner), m_pTransform(pTransform), m_ownTransform(ownTransform)
{
}

WFeatureFilter::WFeatureFilter(const WDomNode* pNode, WLearnerLoader& ll)
: WFilter(pNode, ll), m_ownTransform(true)
{
	m_pTransform = ll.loadIncrementalTransform(pNode->get("trans"));
}

// virtual
WFeatureFilter::~WFeatureFilter()
{
	if(m_ownTransform)
		delete(m_pTransform);
}

// virtual
WDomNode* WFeatureFilter::serialize(WDom* pDoc) const
{
	WDomNode* pNode = domNode(pDoc, "WFeatureFilter");
	pNode->add(pDoc, "trans", m_pTransform->serialize(pDoc));
	return pNode;
}

// virtual
void WFeatureFilter::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected features and labels to have the same number of rows");
	m_pTransform->train(features);
	WMatrix temp(m_pTransform->after().clone());
	temp.newRows(features.rows());
	for(size_t i = 0; i < features.rows(); i++)
		m_pTransform->transform(features[i], temp[i]);
	m_pLearner->train(temp, labels);
}

// virtual
void WFeatureFilter::predict(const WVec& in, WVec& out)
{
	m_pTransform->transform(in, m_pTransform->innerBuf());
	m_pLearner->predict(m_pTransform->innerBuf(), out);
}

// virtual
void WFeatureFilter::predictDistribution(const WVec& in, WPrediction* out)
{
	m_pTransform->transform(in, m_pTransform->innerBuf());
	m_pLearner->predictDistribution(m_pTransform->innerBuf(), out);
}

// virtual
void WFeatureFilter::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	m_pTransform->train(featureRel);
	m_pIncrementalLearner->beginIncrementalLearning(m_pTransform->after(), labelRel);
}

// virtual
void WFeatureFilter::beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels)
{
	m_pTransform->train(features);
	m_pIncrementalLearner->beginIncrementalLearning(m_pTransform->after(), labels.relation());
}

// virtual
void WFeatureFilter::trainIncremental(const WVec& in, const WVec& out)
{
	m_pTransform->transform(in, m_pTransform->innerBuf());
	m_pIncrementalLearner->trainIncremental(m_pTransform->innerBuf(), out);
}

// virtual
const WVec* WFeatureFilter::transformedFeatures(const WVec& in)
{
	m_pTransform->transform(in, m_pTransform->innerBuf());
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterFeatures(m_pTransform->innerBuf());
	else
		return &m_pTransform->innerBuf();
}

// virtual
const WVec* WFeatureFilter::transformedLabels(const WVec& in)
{
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterLabels(in);
	else
		return nullptr;
}

// ---------------------------------------------------------------

WLabelFilter::WLabelFilter(WSupervisedLearner* pLearner, WIncrementalTransform* pTransform, bool ownLearner, bool ownTransform)
: WFilter(pLearner, ownLearner), m_pTransform(pTransform), m_ownTransform(ownTransform)
{
}

WLabelFilter::WLabelFilter(const WDomNode* pNode, WLearnerLoader& ll)
: WFilter(pNode, ll), m_ownTransform(true)
{
	m_pTransform = ll.loadIncrementalTransform(pNode->get("trans"));
}

// virtual
WLabelFilter::~WLabelFilter()
{
	if(m_ownTransform)
		delete(m_pTransform);
}

// virtual
WDomNode* WLabelFilter::serialize(WDom* pDoc) const
{
	WDomNode* pNode = domNode(pDoc, "WLabelFilter");
	pNode->add(pDoc, "trans", m_pTransform->serialize(pDoc));
	return pNode;
}

// virtual
void WLabelFilter::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected features and labels to have the same number of rows");
	m_pTransform->train(labels);
	WMatrix temp(m_pTransform->after().clone());
	temp.newRows(labels.rows());
	for(size_t i = 0; i < labels.rows(); i++)
		m_pTransform->transform(labels[i], temp[i]);
	m_pLearner->train(features, temp);
}

// virtual
void WLabelFilter::predict(const WVec& in, WVec& out)
{
	m_pLearner->predict(in, m_pTransform->innerBuf());
	m_pTransform->untransform(m_pTransform->innerBuf(), out);
}

// virtual
void WLabelFilter::predictDistribution(const WVec& in, WPrediction* out)
{
	m_pLearner->predict(in, m_pTransform->innerBuf());
	m_pTransform->untransformToDistribution(m_pTransform->innerBuf(), out);
}

// virtual
void WLabelFilter::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	m_pTransform->train(labelRel);
	m_pIncrementalLearner->beginIncrementalLearning(featureRel, m_pTransform->after());
}

// virtual
void WLabelFilter::beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels)
{
	m_pTransform->train(labels);
	WMatrix* pNewLabels = m_pTransform->transformBatch(labels);
	std::unique_ptr<WMatrix> hNewLabels(pNewLabels);
	m_pIncrementalLearner->beginIncrementalLearning(features, *pNewLabels);
}

// virtual
void WLabelFilter::trainIncremental(const WVec& in, const WVec& out)
{
	m_pTransform->transform(out, m_pTransform->innerBuf());
	m_pIncrementalLearner->trainIncremental(in, m_pTransform->innerBuf());
}

// virtual
const WVec* WLabelFilter::transformedFeatures(const WVec& in)
{
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterFeatures(in);
	else
		return nullptr;
}

// virtual
const WVec* WLabelFilter::transformedLabels(const WVec& in)
{
	m_pTransform->transform(in, m_pTransform->innerBuf());
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterLabels(m_pTransform->innerBuf());
	else
		return &m_pTransform->innerBuf();
}

// ---------------------------------------------------------------

WAutoFilter::WAutoFilter(WSupervisedLearner* pLearner, bool ownLearner)
: WFilter(pLearner, ownLearner)
{
}

WAutoFilter::WAutoFilter(const WDomNode* pNode, WLearnerLoader& ll)
: WFilter(pNode, ll)
{
}

// virtual
WAutoFilter::~WAutoFilter()
{
	for(size_t i = 0; i < m_prefilteredData.size(); i++)
		delete(m_prefilteredData[i]);
}

// virtual
WDomNode* WAutoFilter::serialize(WDom* pDoc) const
{
	WDomNode* pNode = domNode(pDoc, "WAutoFilter");
	return pNode;
}

void WAutoFilter::whatTypesAreNeeded(const WRelation& featureRel, const WRelation& labelRel, bool& hasNominalFeatures, bool& hasContinuousFeatures, bool& hasNominalLabels, bool& hasContinuousLabels)
{
	// Determine what types are present in the feature data
	hasNominalFeatures = false;
	hasContinuousFeatures = false;
	for(size_t i = 0; i < featureRel.size(); i++)
	{
		if(featureRel.valueCount(i) == 0)
		{
			hasContinuousFeatures = true;
			if(hasNominalFeatures)
				break;
		}
		else
		{
			hasNominalFeatures = true;
			if(hasContinuousFeatures)
				break;
		}
	}

	// Determine what types are present in the label data
	hasNominalLabels = false;
	hasContinuousLabels = false;
	for(size_t i = 0; i < labelRel.size(); i++)
	{
		if(labelRel.valueCount(i) == 0)
		{
			hasContinuousLabels = true;
			if(hasNominalLabels)
				break;
		}
		else
		{
			hasNominalLabels = true;
			if(hasContinuousLabels)
				break;
		}
	}
}

void WAutoFilter::setupDataDependentFilters(WSupervisedLearner* pLearner, const WMatrix& features, const WMatrix& labels, bool hasNominalFeatures, bool hasContinuousFeatures, bool hasNominalLabels, bool hasContinuousLabels)
{
	// Impute features if necessary
	if(!pLearner->canImplicitlyHandleMissingFeatures())
	{
		if(features.doesHaveAnyMissingValues())
		{
			WImputeMissingVals* pImputer = new WImputeMissingVals();
			pImputer->setLabels(&labels); // use the labels to help impute missing features
			m_pLearner = new WFeatureFilter(m_pLearner, pImputer);
		}
	}

	// Normalize labels if necessary
	if(pLearner->canImplicitlyHandleContinuousLabels())
	{
		double supportedMin, supportedMax;
		if(!pLearner->supportedLabelRange(&supportedMin, &supportedMax))
		{
			double dataMin = 1e300;
			double dataMax = -1e300;
			for(size_t i = 0; i < labels.cols(); i++)
			{
				if(labels.relation().valueCount(i) != 0)
					continue;
				dataMin = std::min(labels.columnMin(i), dataMin);
				dataMax = std::max(labels.columnMax(i), dataMax);
			}
			// SAKBox: asked this filter (which handles anything) instead of the
			// learner, so it never ran
			if(hasNominalLabels && !pLearner->canImplicitlyHandleNominalLabels())
			{
				supportedMin = std::min(0.0, supportedMin);
				supportedMax = std::max(1.0, supportedMax);
			}
			bool outOfRange = dataMin < supportedMin || dataMax > supportedMax;
			bool usesLittleOfRange = (dataMax - dataMin) >= 1e-12 && (dataMax - dataMin) * 4 < supportedMax - supportedMin;
			if(outOfRange || usesLittleOfRange)
				m_pLearner = new WLabelFilter(m_pLearner, new WNormalize(supportedMin, supportedMax));
		}
	}

	// Normalize features if necessary
	if(pLearner->canImplicitlyHandleContinuousFeatures())
	{
		double supportedMin, supportedMax;
		if(!pLearner->supportedFeatureRange(&supportedMin, &supportedMax))
		{
			double dataMin = 1e300;
			double dataMax = -1e300;
			for(size_t i = 0; i < features.cols(); i++)
			{
				if(features.relation().valueCount(i) != 0)
					continue;
				dataMin = std::min(features.columnMin(i), dataMin);
				dataMax = std::max(features.columnMax(i), dataMax);
			}
			// SAKBox: asked this filter instead of the learner (so it never ran), and
			// about labels: nominal features are what get one-hot coded here
			if(hasNominalFeatures && !pLearner->canImplicitlyHandleNominalFeatures())
			{
				supportedMin = std::min(0.0, supportedMin);
				supportedMax = std::max(1.0, supportedMax);
			}
			bool outOfRange = dataMin < supportedMin || dataMax > supportedMax;
			bool usesLittleOfRange = (dataMax - dataMin) >= 1e-12 && (dataMax - dataMin) * 4 < supportedMax - supportedMin;
			if(outOfRange || usesLittleOfRange)
				m_pLearner = new WFeatureFilter(m_pLearner, new WNormalize(supportedMin, supportedMax));
		}
	}
}

void WAutoFilter::setupBasicFilters(WSupervisedLearner* pLearner, bool hasNominalFeatures, bool hasContinuousFeatures, bool hasNominalLabels, bool hasContinuousLabels)
{
	// Nomcat labels if necessary
	if(hasNominalLabels && !pLearner->canImplicitlyHandleNominalLabels())
	{
		if(!pLearner->canImplicitlyHandleContinuousLabels()) // SAKBox: asked this filter, so never threw
			throw Ex("Expected a learner that can handle either nominal or continuous labels");
		m_pLearner = new WLabelFilter(m_pLearner, new WNominalToCat(16));
	}

	// Nomcat features if necessary
	if(hasNominalFeatures && !pLearner->canImplicitlyHandleNominalFeatures())
	{
		if(!pLearner->canImplicitlyHandleContinuousFeatures()) // SAKBox: asked this filter, so never threw
			throw Ex("Expected a learner that can handle either nominal or continuous features");
		m_pLearner = new WFeatureFilter(m_pLearner, new WNominalToCat(16));
	}

	// Discretize labels if necessary
	if(hasContinuousLabels && !pLearner->canImplicitlyHandleContinuousLabels())
	{
		if(!pLearner->canImplicitlyHandleNominalLabels()) // SAKBox: asked this filter, so never threw
			throw Ex("Expected a learner that can handle either nominal or continuous labels");
		m_pLearner = new WLabelFilter(m_pLearner, new WDiscretize());
	}

	// Discretize features if necessary
	if(hasContinuousFeatures && !pLearner->canImplicitlyHandleContinuousFeatures())
	{
		if(!pLearner->canImplicitlyHandleNominalFeatures()) // SAKBox: asked this filter, so never threw
			throw Ex("Expected a learner that can handle either nominal or continuous features");
		m_pLearner = new WFeatureFilter(m_pLearner, new WDiscretize());
	}
}

void WAutoFilter::resetFilters(const WMatrix& features, const WMatrix& labels)
{
	discardIntermediateFilters();
	WSupervisedLearner* pLearner = m_pLearner;
	bool hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels;
	whatTypesAreNeeded(features.relation(), labels.relation(), hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels);
	setupDataDependentFilters(pLearner, features, labels, hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels);
	setupBasicFilters(pLearner, hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels);

	if(m_pLearner->canTrainIncrementally())
		m_pIncrementalLearner = (WIncrementalLearner*)m_pLearner;
	else
		m_pIncrementalLearner = nullptr;
}

void WAutoFilter::resetFilters(const WRelation& features, const WRelation& labels)
{
	discardIntermediateFilters();
	WSupervisedLearner* pLearner = m_pLearner;
	bool hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels;
	whatTypesAreNeeded(features, labels, hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels);
	setupBasicFilters(pLearner, hasNominalFeatures, hasContinuousFeatures, hasNominalLabels, hasContinuousLabels);

	if(m_pLearner->canTrainIncrementally())
		m_pIncrementalLearner = (WIncrementalLearner*)m_pLearner;
	else
		m_pIncrementalLearner = nullptr;
}

// virtual
const WVec* WAutoFilter::transformedFeatures(const WVec& in)
{
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterFeatures(in);
	else
		return nullptr;
}

// virtual
const WVec* WAutoFilter::transformedLabels(const WVec& in)
{
	if(m_pLearner->isFilter())
		return &((WFilter*)m_pLearner)->prefilterLabels(in);
	else
		return nullptr;
}

void WAutoFilter::prefilterData(const WMatrix* pFeatures1, const WMatrix* pLabels1, const WMatrix* pFeatures2, const WMatrix* pLabels2, const WMatrix* pFeatures3, const WMatrix* pLabels3, const WMatrix* pFeatures4, const WMatrix* pLabels4)
{
	m_prefilteredData.push_back(prefilterFeatures(*pFeatures1));
	m_prefilteredData.push_back(prefilterLabels(*pLabels1));
	if(pFeatures2)
		m_prefilteredData.push_back(prefilterFeatures(*pFeatures2));
	if(pLabels2)
		m_prefilteredData.push_back(prefilterLabels(*pLabels2));
	if(pFeatures3)
		m_prefilteredData.push_back(prefilterFeatures(*pFeatures3));
	if(pLabels3)
		m_prefilteredData.push_back(prefilterLabels(*pLabels3));
	if(pFeatures4)
		m_prefilteredData.push_back(prefilterFeatures(*pFeatures4));
	if(pLabels4)
		m_prefilteredData.push_back(prefilterLabels(*pLabels4));
}

// virtual
void WAutoFilter::trainInner(const WMatrix& features, const WMatrix& labels)
{
	if(features.rows() != labels.rows())
		throw Ex("Expected features and labels to have the same number of rows");
	resetFilters(features, labels);
	m_pLearner->train(features, labels);
}

// virtual
void WAutoFilter::predict(const WVec& in, WVec& out)
{
	m_pLearner->predict(in, out);
}

// virtual
void WAutoFilter::predictDistribution(const WVec& in, WPrediction* out)
{
	m_pLearner->predictDistribution(in, out);
}

// virtual
void WAutoFilter::beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels)
{
	resetFilters(features, labels);
	m_pIncrementalLearner->beginIncrementalLearning(features, labels);
}

// virtual
void WAutoFilter::beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel)
{
	resetFilters(featureRel, labelRel);
	m_pIncrementalLearner->beginIncrementalLearning(featureRel, labelRel);
}

// virtual
void WAutoFilter::trainIncremental(const WVec& in, const WVec& out)
{
	m_pIncrementalLearner->trainIncremental(in, out);
}



// ---------------------------------------------------------------

WBaselineLearner::WBaselineLearner()
: WSupervisedLearner()
{
}

WBaselineLearner::WBaselineLearner(const WDomNode* pNode)
: WSupervisedLearner(pNode)
{
	m_prediction.clear();
	WDomNode* pred = pNode->get("pred");
	WDomListIterator it(pred);
	m_prediction.reserve(it.remaining());
	for([[maybe_unused]] size_t i = 0; it.current(); i++)
	{
		m_prediction.push_back(it.currentDouble());
		it.advance();
	}
}

// virtual
WBaselineLearner::~WBaselineLearner()
{
	clear();
}

// virtual
void WBaselineLearner::clear()
{
	m_prediction.clear();
}

// virtual
WDomNode* WBaselineLearner::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBaselineLearner");
	if(m_prediction.size() == 0)
		throw Ex("Attempted to serialize a model that has not been trained");
	WDomNode* pred = pNode->add(pDoc, "pred", pDoc->newList());
	for(size_t i = 0; i < m_prediction.size(); i++)
		pred->add(pDoc, m_prediction[i]);
	return pNode;
}

// virtual
void WBaselineLearner::trainInner(const WMatrix& features, const WMatrix& labels)
{
	clear();
	size_t labelDims = labels.cols();
	m_prediction.reserve(labelDims);
	for(size_t i = 0; i < labelDims; i++)
		m_prediction.push_back(labels.baselineValue(i));
}

// virtual
void WBaselineLearner::predictDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, this learner cannot predict a distribution");
}

// virtual
void WBaselineLearner::predict(const WVec& in, WVec& out)
{
	size_t i = 0;
	for(vector<double>::iterator it = m_prediction.begin(); it != m_prediction.end(); it++)
		out[i++] = *it;
}

void WBaselineLearner::autoTune(WMatrix& features, WMatrix& labels)
{
	// This model has no parameters to tune
}

// static
void WBaselineLearner::test()
{
	WBaselineLearner bl;
	bl.basicTest(0.33, 0.33);
}

// ---------------------------------------------------------------

WIdentityFunction::WIdentityFunction()
: WSupervisedLearner(), m_labelDims(0), m_featureDims(0)
{
}

WIdentityFunction::WIdentityFunction(const WDomNode* pNode)
: WSupervisedLearner(pNode)
{
	m_labelDims = (size_t)pNode->getInt("labels");
	m_featureDims = (size_t)pNode->getInt("features");
}

// virtual
WIdentityFunction::~WIdentityFunction()
{
}

// virtual
void WIdentityFunction::clear()
{
	m_labelDims = 0;
	m_featureDims = 0;
}

// virtual
WDomNode* WIdentityFunction::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WIdentityFunction");
	pNode->add(pDoc, "labels", m_labelDims);
	pNode->add(pDoc, "features", m_featureDims);
	return pNode;
}

// virtual
void WIdentityFunction::trainInner(const WMatrix& features, const WMatrix& labels)
{
	m_labelDims = labels.cols();
	m_featureDims = features.cols();
}

// virtual
void WIdentityFunction::predictDistribution(const WVec& in, WPrediction* out)
{
	throw Ex("Sorry, not implemented yet");
}

// virtual
void WIdentityFunction::predict(const WVec& in, WVec& out)
{
	if(m_labelDims <= m_featureDims)
		out.copy(in.data(), m_labelDims);
	else
	{
		out.copy(0, in);
		out.fill(0.0, m_featureDims, m_labelDims - m_featureDims);
	}
}

} // namespace WClasses
