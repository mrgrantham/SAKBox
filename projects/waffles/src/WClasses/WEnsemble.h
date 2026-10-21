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

#ifndef __GENSEMBLE_H__
#define __GENSEMBLE_H__

#include "WLearner.h"
#include <vector>
#include <exception>

namespace WClasses
{

class WRelation;
class WRand;
class WMasterThread;
class WNeuralNetLearner;


typedef void (*EnsembleProgressCallback)(void* pThis, size_t i, size_t n);

/// This is a helper-class used by WBag
class WWeightedModel
{
public:
	double m_weight;
	WSupervisedLearner* m_pModel;

	/// General-purpose constructor
	WWeightedModel(double weight, WSupervisedLearner* pModel)
	: m_weight(weight), m_pModel(pModel)
	{
	}

	/// Load from a DOM.
	WWeightedModel(WDomNode* pNode, WLearnerLoader& ll);
	~WWeightedModel();

	/// Sets the weight of this model
	void setWeight(double w) { m_weight = w; }

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const;
};


/// This is a base-class for ensembles that combine the
/// predictions from multiple weighed models.
class WEnsemble : public WSupervisedLearner
{
protected:
	WRelation* m_pLabelRel;
	std::vector<WWeightedModel*> m_models;
	WVec m_accumulator; // a buffer for tallying votes (ballot box?)

	size_t m_workerThreads;
	WMasterThread* m_pPredictMaster;
public:
	volatile const WVec* m_pPredictInput;

	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WEnsemble();

	/// Deserializing constructor.
	WEnsemble(const WDomNode* pNode, WLearnerLoader& ll);

	~WEnsemble() override;

	/// Returns a reference to the models in the ensemble
	std::vector<WWeightedModel*>& models() { return m_models; }

	/// Adds the vote from one of the models. (This is called internally. Users typically
	/// do not need to call it.)
	void castVote(double weight, const WVec& label);

	/// Specify the number of worker threads to use. If count is 1,
	/// then no additional threads will be spawned, but the work will
	/// all be done by the same thread. If count is 2 or more, that
	/// number of worker threads will be spawned. (Note that with fast models,
	/// the overhead associated with worker threads is often too high to be
	/// worthwhile.) The worker threads are spawned when the first prediction
	/// is made. They are kept alive until clear() is called or this object is
	/// deleted. If you only want to use worker threads during training, but
	/// not when making predictions, you can call this method again to set it back
	/// to 1 after training is complete. Since the inheriting class is
	/// responsible to implement the train method, some child classes may not
	/// implement multi-threaded training. WBag, WBomb, WBayesianModelAveraging,
	/// and WBayesianModelCombination all implement multi-threaded training.
	void setWorkerThreads(size_t count) { m_workerThreads = count; }

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// Base classes should call this method to serialize the base object
	/// as part of their implementation of the serialize method.
	virtual void serializeBase(WDom* pDoc, WDomNode* pNode) const;

	/// Calls clear on all of the models, and resets the accumulator buffer
	virtual void clearBase();

	/// Sets up the accumulator buffer (ballot box) then calls trainInnerInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// Implement this method to train the ensemble.
	virtual void trainInnerInner(const WMatrix& features, const WMatrix& labels) = 0;

	/// Scales the weights of all the models so they sum to 1.0.
	void normalizeWeights();

	/// Counts all the votes from the models in the bag, assuming you are
	/// interested in knowing the distribution.
	void tally(WPrediction* pOut);

	/// Counts all the votes from the models in the bag, assuming you only
	/// care to know the winner, and do not care about the distribution.
	void tally(WVec& label);
};



/// BAG stands for bootstrap aggregator. It represents an ensemble
/// of voting models. Each model is trained with a slightly different
/// training set, which is produced by drawing randomly from the original
/// training set with replacement until we have a new training set of
/// the same size. Each model is given equal weight in the vote.
class WBag : public WEnsemble
{
protected:
	EnsembleProgressCallback m_pCB;
	void* m_pThis;
	double m_trainSize;

public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WBag();

	/// Deserializing constructor.
	WBag(const WDomNode* pNode, WLearnerLoader& ll);

	~WBag() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Calls clears on all of the learners, but does not delete them.
	void clear() override;

	/// Removes and deletes all the learners.
	void flush();

	/// Adds a learner to the bag. This takes ownership of pLearner (so
	/// it will delete it when it's done with it)
	void addLearner(WSupervisedLearner* pLearner);

	/// If you want to be notified when another instance begins training, you can set this callback
	void setProgressCallback(EnsembleProgressCallback pCB, void* pThis)
	{
		m_pCB = pCB;
		m_pThis = pThis;
	}

protected:
	/// See the comment for WEnsemble::trainInnerInner
	void trainInnerInner(const WMatrix& features, const WMatrix& labels) override;

	/// Assigns uniform weight to all models. (This method is deliberately
	/// virtual so that you can overload it if you want non-uniform weighting.)
	virtual void determineWeights(const WMatrix& features, const WMatrix& labels);
};


/// This is an ensemble that uses the bagging approach for training, and Bayesian
/// Model Averaging to combine the models. That is, it trains each model with data
/// drawn randomly with replacement from the original training data. It combines
/// the models with weights proporitional to their likelihood as computed using
/// Bayes' law.
class WBayesianModelAveraging : public WBag
{
public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WBayesianModelAveraging() : WBag() {}

	/// Deserializing constructor.
	WBayesianModelAveraging(const WDomNode* pNode, WLearnerLoader& ll) : WBag(pNode, ll) {}

	~WBayesianModelAveraging() override {}

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

protected:
	/// See the comment for WLearner::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override
	{
		return false;
	}

	/// Determines the weights in the manner of Bayesian model averaging,
	/// with the assumption of uniform priors.
	void determineWeights(const WMatrix& features, const WMatrix& labels) override;
};




class WBayesianModelCombination : public WBag
{
protected:
	size_t m_samples;

public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WBayesianModelCombination() : WBag(), m_samples(100) {}

	/// Deserializing constructor.
	WBayesianModelCombination(const WDomNode* pNode, WLearnerLoader& ll);

	~WBayesianModelCombination() override {}

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Returns the number of samples from which to estimate the combination weights
	size_t samples() { return m_samples; }

	/// Sets the number of samples to use to estimate the combination weights
	void setSamples(size_t n) { m_samples = n; }

protected:
	/// See the comment for WLearner::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override
	{
		return false;
	}

	/// Determines the weights in the manner of Bayesian model averaging,
	/// with the assumption of uniform priors.
	void determineWeights(const WMatrix& features, const WMatrix& labels) override;
};



/// This is an implementation of AdaBoost, except instead of using weighted samples,
/// it resamples the training set by giving each sample a probability proportional to
/// its weight. This difference enables it to work with algorithms that do not
/// support weighted samples.
class WResamplingAdaBoost : public WEnsemble
{
protected:
	WSupervisedLearner* m_pLearner;
	bool m_ownLearner;
	WLearnerLoader* m_pLoader;
	double m_trainSize;
	size_t m_ensembleSize;

public:
	/// General purpose constructor. pLearner is the learning algorithm
	/// that you wish to boost. If ownLearner is true, then this object
	/// will delete pLearner when it is deleted.
	/// pLoader is a WLearnerLoader that can load the model you wish to boost.
	/// (If it is a custom model, then you also need to make a class that inherits
	/// from WLearnerLoader that can load your custom class.) Takes ownership
	/// of pLoader (meaning this object will delete pLoader when it is deleted).
	WResamplingAdaBoost(WSupervisedLearner* pLearner, bool ownLearner, WLearnerLoader* pLoader);

	/// Deserializing constructor
	WResamplingAdaBoost(const WDomNode* pNode, WLearnerLoader& ll);

	~WResamplingAdaBoost() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Deletes all of the models in this ensemble, and calls clear on the base learner.
	void clear() override;

	/// Specify the size of the drawn set to train with (as a portion of the training
	/// set). The default is 1.0.
	void setTrainSize(double d) { m_trainSize = d; }

	/// Specify the maximum number of learners to ensemble. The default is 30.
	void setSize(size_t n) { m_ensembleSize = n; }

protected:
	/// See the comment for WLearner::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }

	/// See the comment for WEnsemble::trainInnerInner
	void trainInnerInner(const WMatrix& features, const WMatrix& labels) override;
};



class WGradBoost : public WEnsemble
{
protected:
	WSupervisedLearner* m_pLearner;
	bool m_ownLearner;
	WLearnerLoader* m_pLoader;
	double m_trainSize;
	size_t m_ensembleSize;
	WVec m_labelCentroid;

public:
	/// General purpose constructor. pLearner is the learning algorithm
	/// that you wish to boost. If ownLearner is true, then this object
	/// will delete pLearner when it is deleted.
	/// pLoader is a WLearnerLoader that can load the model you wish to boost.
	/// (If it is a custom model, then you also need to make a class that inherits
	/// from WLearnerLoader that can load your custom class.) Takes ownership
	/// of pLoader (meaning this object will delete pLoader when it is deleted).
	WGradBoost(WSupervisedLearner* pLearner, bool ownLearner, WLearnerLoader* pLoader);

	/// Deserializing constructor
	WGradBoost(const WDomNode* pNode, WLearnerLoader& ll);

	~WGradBoost() override;

	// todo: this method does not yet benefit from multiple threads
	void predict(const WVec& in, WVec& out) override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Deletes all of the models in this ensemble, and calls clear on the base learner.
	void clear() override;

	/// Specify the portion of the training set to be used for training each model. The default is 1.0.
	void setTrainSize(double d) { m_trainSize = d; }

	/// Specify the maximum number of learners to ensemble. The default is 30.
	void setSize(size_t n) { m_ensembleSize = n; }

protected:
	/// See the comment for WLearner::canImplicitlyHandleContinuousLabels
	bool canImplicitlyHandleContinuousLabels() override { return false; }

	/// See the comment for WEnsemble::trainInnerInner
	void trainInnerInner(const WMatrix& features, const WMatrix& labels) override;
};


/// When Train is called, this performs cross-validation on the training
/// set to determine which learner is the best. It then trains that learner
/// with the entire training set.
class WBucket : public WSupervisedLearner
{
protected:
	size_t m_nBestLearner;
	std::vector<WSupervisedLearner*> m_models;

public:
	/// General-purpose constructor. See also the comment for WSupervisedLearner::WSupervisedLearner.
	WBucket();

	/// Deserializing constructor
	WBucket(const WDomNode* pNode, WLearnerLoader& ll);

	~WBucket() override;

	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Removes and deletes all the learners
	void flush();

	/// Adds a modeler to the list. This takes ownership of pLearner (so
	/// it will delete it when it's done with it)
	void addLearner(WSupervisedLearner* pLearner);

	/// Returns the modeler that did the best with the training set. It is
	/// your responsibility to delete the modeler this returns. Throws if
	/// you haven't trained yet.
	WSupervisedLearner* releaseBestModeler();

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;
};


} // namespace WClasses

#endif // __GENSEMBLE_H__
