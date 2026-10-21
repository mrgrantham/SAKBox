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

#include "WEnsemble.h"
#include "WVec.h"
#include <stdlib.h>
#include "WDistribution.h"
#include "WNeuralNet.h"
#include "WDom.h"
#include "WRand.h"
#include "WHolders.h"
#include "WThread.h"
#include <memory>

using namespace WClasses;
using std::vector;


WWeightedModel::WWeightedModel(WDomNode* pNode, WLearnerLoader& ll)
{
	m_weight = pNode->getDouble("w");
	m_pModel = ll.loadLearner(pNode->get("m"));
}

WWeightedModel::~WWeightedModel()
{
	delete(m_pModel);
}

WDomNode* WWeightedModel::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "w", m_weight);
	pNode->add(pDoc, "m", m_pModel->serialize(pDoc));
	return pNode;
}





WEnsemble::WEnsemble()
: WSupervisedLearner(), m_pLabelRel(nullptr), m_workerThreads(1), m_pPredictMaster(nullptr)
{
}

WEnsemble::WEnsemble(const WDomNode* pNode, WLearnerLoader& ll)
: WSupervisedLearner(pNode), m_pPredictMaster(nullptr)
{
	m_pLabelRel = WRelation::deserialize(pNode->get("labelrel"));
	size_t accumulatorDims = (size_t)pNode->getInt("accum");
	m_accumulator.resize(accumulatorDims);
	m_workerThreads = (size_t)pNode->getInt("threads");
	WDomNode* pModels = pNode->get("models");
	WDomListIterator it(pModels);
	size_t modelCount = it.remaining();
	for(size_t i = 0; i < modelCount; i++)
	{
		WWeightedModel* pWM = new WWeightedModel(it.current(), ll);
		m_models.push_back(pWM);
		it.advance();
	}
}

WEnsemble::~WEnsemble()
{
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
	delete(m_pLabelRel);
	delete(m_pPredictMaster);
}

// virtual
void WEnsemble::serializeBase(WDom* pDoc, WDomNode* pNode) const
{
	pNode->add(pDoc, "labelrel", m_pLabelRel->serialize(pDoc));
	pNode->add(pDoc, "accum", m_accumulator.size());
	pNode->add(pDoc, "threads", m_workerThreads);
	WDomNode* pModels = pNode->add(pDoc, "models", pDoc->newList());
	for(size_t i = 0; i < m_models.size(); i++)
		pModels->add(pDoc, m_models[i]->serialize(pDoc));
}

void WEnsemble::clearBase()
{
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		(*it)->m_pModel->clear();
	delete(m_pLabelRel);
	m_pLabelRel = nullptr;
	m_accumulator.resize(0);
	delete(m_pPredictMaster);
	m_pPredictMaster = nullptr;
}

// virtual
void WEnsemble::trainInner(const WMatrix& features, const WMatrix& labels)
{
	delete(m_pLabelRel);
	m_pLabelRel = labels.relation().clone();

	// Make the accumulator buffer
	size_t labelDims = m_pLabelRel->size();
	size_t nAccumulatorDims = 0;
	for(size_t i = 0; i < labelDims; i++)
	{
		size_t nValues = m_pLabelRel->valueCount(i);
		if(nValues > 0)
			nAccumulatorDims += nValues;
		else
			nAccumulatorDims += 2; // mean and variance
	}
	m_accumulator.resize(nAccumulatorDims);

	trainInnerInner(features, labels);
}

void WEnsemble::normalizeWeights()
{
	double sum = 0.0;
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		sum += (*it)->m_weight;
	double f = 1.0 / sum;
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		(*it)->m_weight *= f;
}

void WEnsemble::castVote(double weight, const WVec& out)
{
	size_t labelDims = m_pLabelRel->size();
	size_t pos = 0;
	for(size_t i = 0; i < labelDims; i++)
	{
		size_t nValues = m_pLabelRel->valueCount(i);
		if(nValues > 0)
		{
			int nVal = (int)out[i];
			if(nVal >= 0 && nVal < (int)nValues)
				m_accumulator[pos + nVal] += weight;
			pos += nValues;
		}
		else
		{
			double dVal = out[i];
			m_accumulator[pos] += (weight * dVal);
			pos++;
			m_accumulator[pos] += (weight * (dVal * dVal));
			pos++;
		}
	}
}

void WEnsemble::tally(WPrediction* out)
{
	size_t labelDims = m_pLabelRel->size();
	size_t nDims = 0;
	double mean;
	for(size_t i = 0; i < labelDims; i++)
	{
		size_t nValues = m_pLabelRel->valueCount(i);
		if(nValues > 0)
		{
			out[i].makeCategorical()->setValues(nValues, &m_accumulator[nDims]);
			nDims += nValues;
		}
		else
		{
			mean = m_accumulator[nDims];
			out[i].makeNormal()->setMeanAndVariance(mean, m_accumulator[nDims + 1] - (mean * mean));
			nDims += 2;
		}
	}
	WAssert(nDims == m_accumulator.size()); // invalid dim count
}

void WEnsemble::tally(WVec& out)
{
	size_t labelDims = m_pLabelRel->size();
	size_t nDims = 0;
	for(size_t i = 0; i < labelDims; i++)
	{
		size_t nValues = m_pLabelRel->valueCount(i);
		if(nValues > 0)
		{
			out[i] = (double)m_accumulator.indexOfMax(nDims, nDims + nValues) - nDims;
			nDims += nValues;
		}
		else
		{
			out[i] = m_accumulator[nDims];
			nDims += 2;
		}
	}
	WAssert(nDims == m_accumulator.size()); // invalid dim count
}

class WEnsemblePredictWorker : public WWorkerThread
{
protected:
	WEnsemble* m_pEnsemble;
	WVec m_prediction;

public:
	WEnsemblePredictWorker(WMasterThread& master, WEnsemble* pEnsemble, size_t outDims)
	: WWorkerThread(master), m_pEnsemble(pEnsemble)
	{
		m_prediction.resize(outDims);
	}

	~WEnsemblePredictWorker() override
	{
	}

	void doJob(size_t jobId) override
	{
		const WVec& in = *(const WVec*)m_pEnsemble->m_pPredictInput;
		WWeightedModel* pWM = m_pEnsemble->models()[jobId];
		pWM->m_pModel->predict(in, m_prediction);
		WSpinLockHolder lockHolder(m_master.getLock(), "WEnsemblePredictWorker::doJob");
		m_pEnsemble->castVote(pWM->m_weight, m_prediction);
	}
};

// virtual
void WEnsemble::predict(const WVec& in, WVec& out)
{
	m_accumulator.fill(0.0);
	m_pPredictInput = &in;
	if(!m_pPredictMaster)
	{
		m_pPredictMaster = new WMasterThread();
		for(size_t i = 0; i < m_workerThreads; i++)
			m_pPredictMaster->addWorker(new WEnsemblePredictWorker(*m_pPredictMaster, this, m_models[0]->m_pModel->relLabels().size()));
	}
	m_pPredictMaster->doJobs(m_models.size());
	tally(out);
}

// virtual
void WEnsemble::predictDistribution(const WVec& in, WPrediction* out)
{
	WVec tmp(m_pLabelRel->size());
	m_accumulator.fill(0.0);
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
	{
		WWeightedModel* pWM = *it;
		pWM->m_pModel->predict(in, tmp);
		castVote(pWM->m_weight, tmp);
	}
	tally(out);
}







WBag::WBag()
: WEnsemble(), m_pCB(nullptr), m_pThis(nullptr), m_trainSize(1.0)
{
}

WBag::WBag(const WDomNode* pNode, WLearnerLoader& ll)
: WEnsemble(pNode, ll), m_pCB(nullptr), m_pThis(nullptr)
{
	m_trainSize = pNode->getDouble("ts");
}

WBag::~WBag()
{
}

// virtual
WDomNode* WBag::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBag");
	serializeBase(pDoc, pNode);
	pNode->add(pDoc, "ts", m_trainSize);
	return pNode;
}

void WBag::clear()
{
	clearBase();
}

void WBag::flush()
{
	clear();
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
	m_models.clear();
}

void WBag::addLearner(WSupervisedLearner* pLearner)
{
	pLearner->rand().setSeed(m_rand.next()); // Ensure that every learner has a different seed
	WWeightedModel* pWM = new WWeightedModel(0.0, pLearner); // The weight will be fixed later
	m_models.push_back(pWM);
}

class WBagTrainWorker : public WWorkerThread
{
protected:
	WBag* m_pBag;
	const WMatrix& m_features;
	const WMatrix& m_labels;
	WMatrix m_drawnFeatures;
	WMatrix m_drawnLabels;
	size_t m_drawSize;
	WRand m_rand;

public:
	WBagTrainWorker(WMasterThread& master, WBag* pBag, const WMatrix& features, const WMatrix& labels, double trainSize, size_t seed)
	: WWorkerThread(master),
	  m_pBag(pBag),
	  m_features(features),
	  m_labels(labels),
	  m_drawnFeatures(features.relation().clone()),
	  m_drawnLabels(labels.relation().clone()),
	  m_rand(seed)
	{
		WAssert(m_features.rows() > 0);
		m_drawSize = size_t(trainSize * features.rows());
		m_drawnFeatures.reserve(m_drawSize);
		m_drawnLabels.reserve(m_drawSize);
	}

	~WBagTrainWorker() override
	{
	}

	void doJob(size_t jobId) override
	{
		// Randomly draw some data (with replacement)
		WReleaseDataHolder hDrawnFeatures(&m_drawnFeatures);
		WReleaseDataHolder hDrawnLabels(&m_drawnLabels);
		for(size_t j = 0; j < m_drawSize; j++)
		{
			size_t r = (size_t)m_rand.next(m_features.rows());
			m_drawnFeatures.takeRow((WVec*)&m_features[r]); // This case is only okay because we only use drawFeatures as a const WMatrix
			m_drawnLabels.takeRow((WVec*)&m_labels[r]); // This case is only okay because we only use drawnLabels as a const WMatrix
		}

		// Train the learner with the drawn data
		m_pBag->models()[jobId]->m_pModel->train(m_drawnFeatures, m_drawnLabels);
	}
};

// virtual
void WBag::trainInnerInner(const WMatrix& features, const WMatrix& labels)
{
	/*
	// Train all the models
	size_t nLearnerCount = m_models.size();
	size_t nDrawSize = size_t(m_trainSize * features.rows());
	WMatrix drawnFeatures(features.relation().clone());
	WMatrix drawnLabels(labels.relation().clone());
	drawnFeatures.reserve(nDrawSize);
	drawnLabels.reserve(nDrawSize);
	{
		for(size_t i = 0; i < nLearnerCount; i++)
		{
			if(m_pCB)
				m_pCB(m_pThis, i, nLearnerCount);

			// Randomly draw some data (with replacement)
			WReleaseDataHolder hDrawnFeatures(&drawnFeatures);
			WReleaseDataHolder hDrawnLabels(&drawnLabels);
			for(size_t j = 0; j < nDrawSize; j++)
			{
				size_t r = (size_t)m_rand.next(features.rows());
				drawnFeatures.takeRow((double*)features[r]); // This case is only okay because we only use drawFeatures as a const WMatrix
				drawnLabels.takeRow((double*)labels[r]); // This case is only okay because we only use drawnLabels as a const WMatrix
			}

			// Train the learner with the drawn data
			m_models[i]->m_pModel->train(drawnFeatures, drawnLabels);
		}
		if(m_pCB)
			m_pCB(m_pThis, nLearnerCount, nLearnerCount);
	}

	// Determine the weights
	determineWeights(features, labels);
	normalizeWeights();
*/

	WMasterThread trainMaster;
	for(size_t i = 0; i < m_workerThreads; i++)
		trainMaster.addWorker(new WBagTrainWorker(trainMaster, this, features, labels, m_trainSize, (size_t)m_rand.next()));
	trainMaster.doJobs(m_models.size());
	determineWeights(features, labels);
	normalizeWeights();
}

// virtual
void WBag::determineWeights(const WMatrix& features, const WMatrix& labels)
{
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		(*it)->m_weight = 1.0;
}

#include "WDecisionTree.h"
// static
void WBag::test()
{
	WBag bag;
	for(size_t i = 0; i < 64; i++)
	{
		WDecisionTree* pTree = new WDecisionTree();
		pTree->useRandomDivisions();
		bag.addLearner(pTree);
	}
	bag.basicTest(0.764, 0.93, 0.01);
}






// virtual
void WBayesianModelAveraging::determineWeights(const WMatrix& features, const WMatrix& labels)
{
	double m = -1e38;
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
	{
		double d = 1.0 - ((*it)->m_pModel->sumSquaredError(features, labels) / labels.rows());
		double logProbHypothGivenData;
		if(d <= 0.0)
			logProbHypothGivenData = -1e38;
		else if(d == 1.0)
			logProbHypothGivenData = 0.0;
		else
			logProbHypothGivenData = features.rows() * (d * log(d) + (1.0 - d) * log(1.0 - d));
		m = std::max(m, logProbHypothGivenData);
		(*it)->m_weight = logProbHypothGivenData;
	}
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
	{
		double logProbHypothGivenData = (*it)->m_weight;
		(*it)->m_weight = exp(logProbHypothGivenData - m);
	}
}

// virtual
WDomNode* WBayesianModelAveraging::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBayesianModelAveraging");
	serializeBase(pDoc, pNode);
	pNode->add(pDoc, "ts", m_trainSize);
	return pNode;
}

// static
void WBayesianModelAveraging::test()
{
	WBayesianModelAveraging bma;
	for(size_t i = 0; i < 32; i++)
	{
		WDecisionTree* pTree = new WDecisionTree();
		pTree->useRandomDivisions();
		bma.addLearner(pTree);
	}
	bma.basicTest(0.708, 0.816, 0.01);
}







WBayesianModelCombination::WBayesianModelCombination(const WDomNode* pNode, WLearnerLoader& ll)
: WBag(pNode, ll)
{
	m_samples = (size_t)pNode->getInt("samps");
}

// virtual
void WBayesianModelCombination::determineWeights(const WMatrix& features, const WMatrix& labels)
{
	WQUICKVEC(weights, m_models.size());
	weights.fill(0.0);
	double sumWeight = 0.0;
	double maxLogProb = -1e38;
	for(size_t i = 0; i < m_samples; i++)
	{
		// Set weights randomly from a dirichlet distribution with unifrom probabilities
		for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
			(*it)->m_weight = m_rand.exponential();
		normalizeWeights();

		// Evaluate accuracy
		double d = 1.0 - (sumSquaredError(features, labels) / labels.rows());
		double logProbEnsembleGivenData;
		if(d <= 0.0)
			logProbEnsembleGivenData = -1e38;
		else if(d == 1.0)
			logProbEnsembleGivenData = 0.0;
		else
			logProbEnsembleGivenData = features.rows() * (d * log(d) + (1.0 - d) * log(1.0 - d));

		// Update the weights
		if(logProbEnsembleGivenData > maxLogProb)
		{
			weights *= exp(maxLogProb - logProbEnsembleGivenData);
			maxLogProb = logProbEnsembleGivenData;
		}
		double w = exp(logProbEnsembleGivenData - maxLogProb);
		weights *= (sumWeight / (sumWeight + w));
		size_t pos = 0;
		for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
			weights[pos++] += (w * (*it)->m_weight);
		sumWeight += w;
	}
	size_t pos = 0;
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		(*it)->m_weight = weights[pos++];
}

// virtual
WDomNode* WBayesianModelCombination::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBayesianModelCombination");
	serializeBase(pDoc, pNode);
	pNode->add(pDoc, "ts", m_trainSize);
	pNode->add(pDoc, "samps", m_samples);
	return pNode;
}

// static
void WBayesianModelCombination::test()
{
	WBayesianModelCombination bmc;
	for(size_t i = 0; i < 32; i++)
	{
		WDecisionTree* pTree = new WDecisionTree();
		pTree->useRandomDivisions();
		bmc.addLearner(pTree);
	}
	bmc.basicTest(0.76, 0.928, 0.01);
}





WResamplingAdaBoost::WResamplingAdaBoost(WSupervisedLearner* pLearner, bool ownLearner, WLearnerLoader* pLoader)
: WEnsemble(), m_pLearner(pLearner), m_ownLearner(ownLearner), m_pLoader(pLoader), m_trainSize(1.0), m_ensembleSize(30)
{
}

WResamplingAdaBoost::WResamplingAdaBoost(const WDomNode* pNode, WLearnerLoader& ll)
: WEnsemble(pNode, ll), m_pLearner(nullptr), m_ownLearner(false), m_pLoader(nullptr)
{
	m_trainSize = pNode->getDouble("ts");
	m_ensembleSize = (size_t)pNode->getInt("es");
}

// virtual
WResamplingAdaBoost::~WResamplingAdaBoost()
{
	clear();
	if(m_ownLearner)
		delete(m_pLearner);
	delete(m_pLoader);
}

// virtual
WDomNode* WResamplingAdaBoost::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WResamplingAdaBoost");
	serializeBase(pDoc, pNode);
	pNode->add(pDoc, "es", m_ensembleSize);
	pNode->add(pDoc, "ts", m_trainSize);
	return pNode;
}

// virtual
void WResamplingAdaBoost::clear()
{
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
	m_models.clear();
	if(m_pLearner)
		m_pLearner->clear();
}

// virtual
void WResamplingAdaBoost::trainInnerInner(const WMatrix& features, const WMatrix& labels)
{
	clear();

	// Initialize all instances with uniform weights
	WVec pDistribution(features.rows());
	pDistribution.fill(1.0 / features.rows());
	size_t drawRows = size_t(m_trainSize * features.rows());
	size_t* pDrawnIndexes = new size_t[drawRows];
	std::unique_ptr<size_t[]> hDrawnIndexes(pDrawnIndexes);

	// Train the ensemble
	size_t labelDims = labels.cols();
	double penalty = 1.0 / labelDims;
	WVec prediction(labelDims);
	for(size_t es = 0; es < m_ensembleSize; es++)
	{
		// Draw a training set from the distribution
		WCategoricalSamplerBatch csb(features.rows(), pDistribution, m_rand);
		csb.draw(drawRows, pDrawnIndexes);
		WMatrix drawnFeatures(features.relation().clone());
		WReleaseDataHolder hDrawnFeatures(&drawnFeatures);
		WMatrix drawnLabels(labels.relation().clone());
		WReleaseDataHolder hDrawnLabels(&drawnLabels);
		size_t* pIndex = pDrawnIndexes;
		for(size_t i = 0; i < drawRows; i++)
		{
			drawnFeatures.takeRow((WVec*)&features[*pIndex]);
			drawnLabels.takeRow((WVec*)&labels[*pIndex]);
			pIndex++;
		}

		// Train an instance of the model and store a clone of it
		m_pLearner->train(drawnFeatures, drawnLabels);
		WDom doc;
		WSupervisedLearner* pClone = m_pLoader->loadLearner(m_pLearner->serialize(&doc));

		// Compute model weight
		double err = 0.5;
		for(size_t i = 0; i < features.rows(); i++)
		{
			pClone->predict(features[i], prediction);
			const WVec& target = labels[i];
			for(size_t j = 0; j < labelDims; j++)
			{
				if((int)target[j] != (int)prediction[j])
					err += penalty;
			}
		}
		err /= features.rows();
		if(err >= 0.5)
		{
			delete(pClone);
			continue;
		}
		double weight = 0.5 * log((1.0 - err) / err);
		m_models.push_back(new WWeightedModel(weight, pClone));

		// Update the distribution to favor mis-classified instances
		for(size_t i = 0; i < features.rows(); i++)
		{
			err = 0.0;
			pClone->predict(features[i], prediction);
			const WVec& target = labels[i];
			for(size_t j = 0; j < labelDims; j++)
			{
				if((int)target[j] != (int)prediction[j])
					err += penalty;
			}
			err /= labelDims;
			pDistribution[i] *= exp(weight * (err * 2.0 - 1.0));
		}
		pDistribution.sumToOne();
	}
	if(m_models.empty())
	{
		throw Ex("No valid models found for AdaBoost");
	}
	normalizeWeights();
}

// static
void WResamplingAdaBoost::test()
{
	WDecisionTree* pLearner = new WDecisionTree();
	pLearner->useRandomDivisions();
	WResamplingAdaBoost boost(pLearner, true, new WLearnerLoader());
	boost.basicTest(0.753, 0.92);
}









WGradBoost::WGradBoost(WSupervisedLearner* pLearner, bool ownLearner, WLearnerLoader* pLoader)
: WEnsemble(), m_pLearner(pLearner), m_ownLearner(ownLearner), m_pLoader(pLoader), m_trainSize(1.0), m_ensembleSize(30)
{
}

WGradBoost::WGradBoost(const WDomNode* pNode, WLearnerLoader& ll)
: WEnsemble(pNode, ll), m_pLearner(nullptr), m_ownLearner(false), m_pLoader(nullptr)
{
	m_trainSize = pNode->getDouble("ts");
	m_ensembleSize = (size_t)pNode->getInt("es");
}

// virtual
WGradBoost::~WGradBoost()
{
	clear();
	if(m_ownLearner)
		delete(m_pLearner);
	delete(m_pLoader);
}

// virtual
WDomNode* WGradBoost::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WGradBoost");
	serializeBase(pDoc, pNode);
	pNode->add(pDoc, "es", m_ensembleSize);
	pNode->add(pDoc, "ts", m_trainSize);
	return pNode;
}

// virtual
void WGradBoost::clear()
{
	for(vector<WWeightedModel*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
	m_models.clear();
	if(m_pLearner)
		m_pLearner->clear();
}

// virtual
void WGradBoost::trainInnerInner(const WMatrix& features, const WMatrix& labels)
{
	clear();

	// Compute label centroid
	m_labelCentroid.resize(labels.cols());
	for(size_t i = 0; i < m_labelCentroid.size(); i++)
		m_labelCentroid[i] = labels.columnMean(i);

	// Train the ensemble
	size_t drawRows = (size_t)(m_trainSize * features.rows());
	WVec prediction(m_labelCentroid.size());
	for(size_t es = 0; es < m_ensembleSize; es++)
	{
		// Draw a training set from the distribution
		WMatrix drawnFeatures(features.relation().clone());
		WReleaseDataHolder hDrawnFeatures(&drawnFeatures);
		WMatrix residualLabels(labels.relation().clone());
		for(size_t i = 0; i < drawRows; i++)
		{
			size_t index = m_rand.next(features.rows());
			drawnFeatures.takeRow((WVec*)&features[index]);
			WVec& lab = residualLabels.newRow();
			lab.copy(labels[index]);
			predict(features[index], prediction);
			lab -= prediction;
		}

		// Train an instance of the model and store a clone of it
		m_pLearner->train(drawnFeatures, residualLabels);
		WDom doc;
		WSupervisedLearner* pClone = m_pLoader->loadLearner(m_pLearner->serialize(&doc));
		m_models.push_back(new WWeightedModel(1.0, pClone));
	}
}

// virtual
void WGradBoost::predict(const WVec& in, WVec& out)
{
	out.copy(m_labelCentroid);
	for(size_t i = 0; i < m_models.size(); i++)
	{
		m_models[i]->m_pModel->predict(in, m_accumulator);
		out += m_accumulator;
	}
}










WBucket::WBucket()
: WSupervisedLearner()
{
	m_nBestLearner = INVALID_INDEX;
}

WBucket::WBucket(const WDomNode* pNode, WLearnerLoader& ll)
: WSupervisedLearner(pNode)
{
	WDomNode* pModels = pNode->get("models");
	WDomListIterator it(pModels);
	size_t modelCount = it.remaining();
	for(size_t i = 0; i < modelCount; i++)
	{
		m_models.push_back(ll.loadLearner(it.current()));
		it.advance();
	}
	m_nBestLearner = (size_t)pNode->getInt("best");
}

WBucket::~WBucket()
{
	for(vector<WSupervisedLearner*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
}

// virtual
WDomNode* WBucket::serialize(WDom* pDoc) const
{
	WDomNode* pNode = baseDomNode(pDoc, "WBucket");
	WDomNode* pModels = pNode->add(pDoc, "models", pDoc->newList());
	pModels->add(pDoc, m_models[m_nBestLearner]->serialize(pDoc));
	pNode->add(pDoc, "best", 0ll);
	return pNode;
}

void WBucket::clear()
{
	for(vector<WSupervisedLearner*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		(*it)->clear();
}

void WBucket::flush()
{
	for(vector<WSupervisedLearner*>::iterator it = m_models.begin(); it != m_models.end(); it++)
		delete(*it);
	m_models.clear();
}

void WBucket::addLearner(WSupervisedLearner* pLearner)
{
	pLearner->rand().setSeed(m_rand.next()); // Ensure that every learner has a different seed
	m_models.push_back(pLearner);
}

// virtual
void WBucket::trainInner(const WMatrix& features, const WMatrix& labels)
{
	size_t nLearnerCount = m_models.size();
	double dBestError = 1e200;
	WSupervisedLearner* pLearner;
	m_nBestLearner = (size_t)m_rand.next(nLearnerCount);
	double err;
	for(size_t i = 0; i < nLearnerCount; i++)
	{
		pLearner = m_models[i];
		err = pLearner->crossValidate(features, labels, 2);
		if(err < dBestError)
		{
			dBestError = err;
			m_nBestLearner = i;
		}
		pLearner->clear();
	}
	pLearner = m_models[m_nBestLearner];
	pLearner->train(features, labels);
}

WSupervisedLearner* WBucket::releaseBestModeler()
{
	if(m_nBestLearner == INVALID_INDEX)
		throw Ex("Not trained yet");
	WSupervisedLearner* pModeler = m_models[m_nBestLearner];
	m_models[m_nBestLearner] = m_models[m_models.size() - 1];
	m_models.pop_back();
	m_nBestLearner = -1;
	return pModeler;
}

// virtual
void WBucket::predict(const WVec& in, WVec& out)
{
	if(m_nBestLearner == INVALID_INDEX)
		throw Ex("not trained yet");
	m_models[m_nBestLearner]->predict(in, out);
}

// virtual
void WBucket::predictDistribution(const WVec& in, WPrediction* out)
{
	if(m_nBestLearner == INVALID_INDEX)
		throw Ex("not trained yet");
	m_models[m_nBestLearner]->predictDistribution(in, out);
}

#include "WDecisionTree.h"
// static
void WBucket::test()
{
	WBucket bucket;
	bucket.addLearner(new WBaselineLearner());
	bucket.addLearner(new WDecisionTree());
	bucket.addLearner(new WAutoFilter(new WMeanMarginsTree()));
	bucket.basicTest(0.695, 0.918);
}
