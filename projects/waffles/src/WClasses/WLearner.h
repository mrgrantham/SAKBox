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

#ifndef __GLEARNER_H__
#define __GLEARNER_H__

#include "WMatrix.h"
#include "WRand.h"

#include <memory>

namespace WClasses
{

class WDom;
class WDomNode;
class WDistribution;
class WCategoricalDistribution;
class WNormalDistribution;
class WUniformDistribution;
class WUnivariateDistribution;
class WIncrementalTransform;
class WSparseMatrix;
class WCollaborativeFilter;
class WNeuralNetLearner;
class WLearnerLoader;

/// This class is used to represent the predicted distribution made by a supervised learning algorithm.
/// (It is just a shallow wrapper around WDistribution.) It is used in conjunction with calls
/// to WSupervisedLearner::predictDistribution. The predicted distributions will be either
/// categorical distributions (for nominal values) or Normal distributions (for continuous values).
class WPrediction
{
protected:
	WUnivariateDistribution* m_pDistribution;

public:
	WPrediction() : m_pDistribution(nullptr)
	{
	}

	~WPrediction();

	/// Converts an array of prediction objects to a vector of most-likely values.
	static void predictionArrayToVector(size_t nOutputCount, WPrediction* pOutputs, WVec& vec);

	/// Converts an array of values to an array of predictions. There's not really
	/// enough information for this conversion, so it simply fabricates the variance
	/// and class-probability information as needed. Only the mean (for normal distributions)
	/// and the most-likely class (for categorical distributions) is reliable after this
	/// conversion.
	static void vectorToPredictionArray(WRelation* pRelation, size_t nOutputCount, WVec& vec, WPrediction* pOutputs);

	/// Returns true if this wraps a normal distribution, false otherwise
	bool isContinuous();

	/// Returns the mode (most likely value). For the Normal distribution, this is the same as the mean.
	double mode();

	/// If the current distribution is not a categorical distribution, then it
	/// replaces it with a new categorical distribution. Then it returns the
	/// current (categorical) distribution.
	WCategoricalDistribution* makeCategorical();

	/// If the current distribution is not a normal distribution, then it
	/// replaces it with a new normal distribution. Then it returns the
	/// current (normal) distribution.
	WNormalDistribution* makeNormal();

	/// Returns the current distribution. Throws if it is not a categorical distribution
	WCategoricalDistribution* asCategorical();

	/// Returns the current distribution. Throws if it is not a normal distribution
	WNormalDistribution* asNormal();
};



// nRep and nFold are zero-indexed
typedef void (*RepValidateCallback)(void* pThis, size_t nRep, size_t nFold, double foldSSE, size_t rows);


/// This is the base class of supervised learning algorithms (that may or may not
/// have an internal model allowing them to generalize rows that were not available
/// at training time). Note that the literature typically refers to supervised learning
/// algorithms that can't generalize (because they lack an internal hypothesis model)
/// as "Semi-supervised". (You cannot generalize with a semi-supervised algorithm--you have to
/// train again with the new rows.)
class WTransducer
{
protected:
	WRand m_rand;

public:
	/// General-purpose constructor.
	WTransducer();

	/// Copy-constructor. Throws an exception to prevent models from being copied by value.
	WTransducer(const WTransducer& that) : m_rand(0)
	{
		throw Ex("This object is not intended to be copied by value");
	}

	virtual ~WTransducer();

	/// Throws an exception to prevent models from being copied by value.
	WTransducer& operator=(const WTransducer& other)
	{
		throw Ex("This object is not intended to be copied by value");
	}


	/// Returns false because semi-supervised learners have no internal
	/// model, so they can't evaluate previously unseen rows.
	virtual bool canGeneralize() { return false; }

	/// Returns false because semi-supervised learners cannot be trained incrementally.
	virtual bool canTrainIncrementally() { return false; }

	/// Predicts a set of labels to correspond with features2, such that these
	/// labels will be consistent with the patterns exhibited by features1 and labels1.
	std::unique_ptr<WMatrix> transduce(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2);

	/// Trains and tests this learner. Returns the sum-squared-error.
	/// if pOutSAE is not NULL, the sum absolute error will be placed there.
	virtual double trainAndTest(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, double* pOutSAE = nullptr);

	/// Makes a confusion matrix for a transduction algorithm
	void transductiveConfusionMatrix(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, std::vector<WMatrix*>& stats);

	/// Perform n-fold cross validation on pData. Returns sum-squared error.
	/// Uses trainAndTest for each fold. pCB is an optional callback method for reporting
	/// intermediate stats. It can be NULL if you don't want intermediate reporting.
	/// nRep is just the rep number that will be passed to the callback.
	/// pThis is just a pointer that will be passed to the callback for you
	/// to use however you want. It doesn't affect this method.
	/// if pOutSAE is not NULL, the sum absolute error will be placed there.
	double crossValidate(const WMatrix& features, const WMatrix& labels, size_t nFolds, double* pOutSAE = nullptr, RepValidateCallback pCB = nullptr, size_t nRep = 0, void* pThis = nullptr);

	/// Perform cross validation "nReps" times and return the
	/// average score. pCB is an optional callback method for reporting intermediate stats
	/// It can be NULL if you don't want intermediate reporting.
	/// pThis is just a pointer that will be passed to the callback for you
	/// to use however you want. It doesn't affect this method.
	/// if pOutSAE is not NULL, the sum absolute error will be placed there.
	double repValidate(const WMatrix& features, const WMatrix& labels, size_t reps, size_t nFolds, double* pOutSAE = nullptr, RepValidateCallback pCB = nullptr, void* pThis = nullptr);

	/// Returns a reference to the random number generator associated with this object.
	/// For example, you could use it to change the random seed, to make this algorithm behave differently.
	/// This might be important, for example, in an ensemble of learners.
	WRand& rand() { return m_rand; }

	/// Returns true iff this algorithm can implicitly handle nominal features. If it
	/// cannot, then the WNominalToCat transform will be used to convert nominal
	/// features to continuous values before passing them to it.
	virtual bool canImplicitlyHandleNominalFeatures() { return true; }

	/// Returns true iff this algorithm can implicitly handle continuous features. If it
	/// cannot, then the WDiscretize transform will be used to convert continuous
	/// features to nominal values before passing them to it.
	virtual bool canImplicitlyHandleContinuousFeatures() { return true; }

	/// Returns true if this algorithm supports any feature value, or if it does not
	/// implicitly handle continuous features. If a limited range of continuous values is
	/// supported, returns false and sets pOutMin and pOutMax to specify the range.
	virtual bool supportedFeatureRange(double* pOutMin, double* pOutMax) { return true; }

	/// Returns true iff this algorithm supports missing feature values. If it cannot,
	/// then an imputation filter will be used to predict missing values before any
	/// feature-vectors are passed to the algorithm.
	virtual bool canImplicitlyHandleMissingFeatures() { return true; }

	/// Returns true iff this algorithm can implicitly handle nominal labels (a.k.a.
	/// classification). If it cannot, then the WNominalToCat transform will be
	/// used during training to convert nominal labels to continuous values, and
	/// to convert categorical predictions back to nominal labels.
	virtual bool canImplicitlyHandleNominalLabels() { return true; }

	/// Returns true iff this algorithm can implicitly handle continuous labels (a.k.a.
	/// regression). If it cannot, then the WDiscretize transform will be
	/// used during training to convert nominal labels to continuous values, and
	/// to convert nominal predictions back to continuous labels.
	virtual bool canImplicitlyHandleContinuousLabels() { return true; }

	/// Returns true if this algorithm supports any label value, or if it does not
	/// implicitly handle continuous labels. If a limited range of continuous values is
	/// supported, returns false and sets pOutMin and pOutMax to specify the range.
	virtual bool supportedLabelRange(double* pOutMin, double* pOutMax) { return true; }

protected:
	/// This is the algorithm's implementation of transduction. (It is called by the transduce method.)
	virtual std::unique_ptr<WMatrix> transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2) = 0;
};


/// This is the base class of algorithms that learn with supervision and
/// have an internal hypothesis model that allows them to generalize
/// rows that were not available at training time.
class WSupervisedLearner : public WTransducer
{
protected:
	WRelation* m_pRelFeatures;
	WRelation* m_pRelLabels;

public:
	/// General-purpose constructor.
	WSupervisedLearner();

	/// Deserialization constructor
	WSupervisedLearner(const WDomNode* pNode);

	/// Destructor
	~WSupervisedLearner() override;

	/// Marshal this object into a DOM that can be converted to a variety
	/// of formats. (Implementations of this method should use baseDomNode.)
	virtual WDomNode* serialize(WDom* pDoc) const = 0;

	/// Returns true because fully supervised learners have an internal
	/// model that allows them to generalize previously unseen rows.
	bool canGeneralize() override { return true; }

	/// Returns a reference to the feature relation (meta-data about the input attributes).
	const WRelation& relFeatures();

	/// Returns a reference to the label relation (meta-data about the output attributes).
	const WRelation& relLabels();

	/// Returns false
	virtual bool isFilter() { return false; }

	/// Call this method to train the model.
	void train(const WMatrix& features, const WMatrix& labels);


	/// Evaluate pIn to compute a prediction for pOut. The model must be trained
	/// (by calling train) before the first time that this method is called.
	/// pIn and pOut should point to arrays of doubles of the same size as the
	/// number of columns in the training matrices that were passed to the train
	/// method.
	virtual void predict(const WVec& in, WVec& out) = 0;

	/// Evaluate pIn and compute a prediction for pOut. pOut is expected
	/// to point to an array of WPrediction objects which have already been
	/// allocated. There should be labelDims() elements in this array.
	/// The distributions will be more accurate if the model is calibrated
	/// before the first time that this method is called.
	virtual void predictDistribution(const WVec& in, WPrediction* pOut) = 0;

	/// Discards all training for the purpose of freeing memory.
	/// If you call this method, you must train before making any predictions.
	/// No settings or options are discarded, so you should be able to
	/// train again without specifying any other parameters and still get
	/// a comparable model.
	virtual void clear() = 0;

	/// Generates a confusion matrix containing the total counts of the number of times
	/// each value was expected and predicted. (Rows represent target values, and columns
	/// represent predicted values.) stats should be an empty vector. This method will
	/// resize stats to the number of dimensions in the label vector. The caller is
	/// responsible to delete all of the matrices that it puts in this vector. For
	/// continuous labels, the value will be NULL.
	void confusion(WMatrix& features, WMatrix& labels, std::vector<WMatrix*>& stats);

	/// Computes the sum-squared-error for predicting the labels from the features.
	/// For categorical labels, Hamming distance is used.
	double sumSquaredError(const WMatrix& features, const WMatrix& labels, double* pOutSAE = nullptr);

	/// label specifies which output to measure. (It should be 0 if there is only one label dimension.)
	/// The measurement will be performed "nReps" times and results averaged together
	/// nPrecisionSize specifies the number of points at which the function is sampled
	/// pOutPrecision should be an array big enough to hold nPrecisionSize elements for every possible
	/// label value. (If the attribute is continuous, it should just be big enough to hold nPrecisionSize elements.)
	/// If bLocal is true, it computes the local precision instead of the global precision.
	void precisionRecall(double* pOutPrecision, size_t nPrecisionSize, WMatrix& features, WMatrix& labels, size_t label, size_t nReps);

	/// Trains and tests this learner. Returns sum-squared-error.
	double trainAndTest(const WMatrix& trainFeatures, const WMatrix& trainLabels, const WMatrix& testFeatures, const WMatrix& testLabels, double* pOutSAE = nullptr) override;

	/// This is a helper method used by the unit tests of several model learners
	void basicTest(double minAccuracy1, double minAccuracy2, double deviation = 1e-6, bool printAccuracy = false, double warnRange = 0.035);

	/// Runs some unit tests related to supervised learning. Throws an exception if any problems are found.
	static void test();
protected:
	/// Adds the function pIn to pOut after interpolating pIn to be the same size as pOut.
	/// (This is a helper-function used by precisionRecall.)
	static void addInterpolatedFunction(double* pOut, size_t nOutVals, double* pIn, size_t nInVals);

	/// This is the implementation of the model's training algorithm. (This method is called by train).
	virtual void trainInner(const WMatrix& features, const WMatrix& labels) = 0;

	/// See WTransducer::transduce
	std::unique_ptr<WMatrix> transduceInner(const WMatrix& features1, const WMatrix& labels1, const WMatrix& features2) override;

	/// This method determines which data filters (normalize, discretize,
	/// and/or nominal-to-cat) are needed and trains them.
	void setupFilters(const WMatrix& features, const WMatrix& labels);

	/// This is a helper method used by precisionRecall.
	size_t precisionRecallNominal(WPrediction* pOutput, double* pFunc, WMatrix& trainFeatures, WMatrix& trainLabels, WMatrix& testFeatures, WMatrix& testLabels, size_t label, int value);

	/// This is a helper method used by precisionRecall.
	size_t precisionRecallContinuous(WPrediction* pOutput, double* pFunc, WMatrix& trainFeatures, WMatrix& trainLabels, WMatrix& testFeatures, WMatrix& testLabels, size_t label);

	/// Child classes should use this in their implementation of serialize
	WDomNode* baseDomNode(WDom* pDoc, const char* szClassName) const;
};

///\brief Converts a WSupervisedLearner to a string
std::string to_str(const WSupervisedLearner& learner);





/// This is the base class of supervised learning algorithms that
/// can learn one row at a time.
class WIncrementalLearner : public WSupervisedLearner
{
public:
	/// General-purpose constructor.
	WIncrementalLearner()
	: WSupervisedLearner()
	{
	}

	/// Deserialization constructor
	WIncrementalLearner(const WDomNode* pNode)
	: WSupervisedLearner(pNode)
	{
	}

	/// Destructor
	~WIncrementalLearner() override
	{
	}

	/// Only the WFilter class should return true to this method
	bool isFilter() override { return false; }

	/// Returns true
	bool canTrainIncrementally() override { return true; }

	/// You must call this method before you call trainIncremental.
	void beginIncrementalLearning(const WRelation& featureRel, const WRelation& labelRel);

	/// A version of beginIncrementalLearning that supports data-dependent filters.
	void beginIncrementalLearning(const WMatrix& features, const WMatrix& labels);

	/// Pass a single input row and the corresponding label to incrementally train this model.
	virtual void trainIncremental(const WVec& in, const WVec& out) = 0;

	/// Train using a sparse feature matrix. (A Typical implementation of this
	/// method will first call beginIncrementalLearning, then it will
	/// iterate over all of the feature rows, and for each row it
	/// will convert the sparse row to a dense row, call trainIncremental
	/// using the dense row, then discard the dense row and proceed to the next row.)
	virtual void trainSparse(WSparseMatrix& features, WMatrix& labels) = 0;

protected:
	/// Prepare the model for incremental learning.
	virtual void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) = 0;

	virtual void beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels)
	{
		beginIncrementalLearningInner(features.relation(), labels.relation());
	}
};



/// This class is for loading various learning algorithms from a DOM. When any
/// learning algorithm is saved, it calls baseDomNode, which creates (among other
/// things) a field named "class" which specifies the class name of the algorithm.
/// This class contains methods that will recognize any of the classes in this library
/// and load them. If it doesn't recognize a class, it will either return NULL or throw
/// and exception, depending on the flags you pass to the constructor.
/// Obviously this loader won't recognize any classes that you make. Therefore, you should
/// overload the corresponding method in this class with a new method that will first
/// recognize and load your classes, and then call these methods to handle other types.
class WLearnerLoader
{
protected:
	bool m_throwIfClassNotFound;

public:
	/// Constructor. If throwIfClassNotFound is true, then all of the methods in this
	/// class will throw an exception of the DOM refers to an unrecognized class.
	/// If throwIfClassNotFound is false, then NULL will be returned if the class
	/// is not recognized.
	WLearnerLoader(bool throwIfClassNotFound = true)
	: m_throwIfClassNotFound(throwIfClassNotFound)
	{
	}

	virtual ~WLearnerLoader() {}

	/// Loads an incremental transform (or a two-way incremental transform) from a DOM.
	virtual WIncrementalTransform* loadIncrementalTransform(const WDomNode* pNode);

	/// Loads a learning algorithm from a DOM.
	virtual WSupervisedLearner* loadLearner(const WDomNode* pNode);

	/// Loads a collaborative filtering algorithm from a DOM.
	virtual WCollaborativeFilter* loadCollaborativeFilter(const WDomNode* pNode);
};



class WFilter : public WIncrementalLearner
{
protected:
	WSupervisedLearner* m_pLearner;
	WIncrementalLearner* m_pIncrementalLearner;
	WSupervisedLearner* m_pOriginal;
	bool m_ownLearner;


	WFilter(WSupervisedLearner* pLearner, bool ownLearner = true);

	/// Deserialization constructor
	WFilter(const WDomNode* pNode, WLearnerLoader& ll);

	~WFilter() override;

	bool canTrainIncrementally() override { return m_pLearner->canTrainIncrementally(); }

	/// Discards any filters between this filter and the base learner
	void discardIntermediateFilters();

	/// Recursive method used by discardIntermediateFilters
	void discardIntermediateFilters_helper(WSupervisedLearner* pOriginal);

	/// Helper function for serialization
	WDomNode* domNode(WDom* pDoc, const char* szClassName) const;

public:
	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// Returns true
	bool isFilter() override { return true; }

	/// Initialize (or train) this filter without calling train on any of the
	/// interior components. (This might be used when filtering a learner that
	/// has already been trained with a transform that has also already been trained.)
	void initShellOnly(const WRelation& featureRel, const WRelation& labelRel);

	/// Transform a feature vector to the form for presenting to the inner
	/// learner. The result is either in itself (when there is nothing to
	/// transform) or a buffer in a filter, valid until the next call.
	const WVec& prefilterFeatures(const WVec& in);

	/// Transform a label vector to the form for presenting to the inner
	/// learner. The result is as for prefilterFeatures.
	const WVec& prefilterLabels(const WVec& in);

	/// Not for temporaries: the result can be the input itself, which would
	/// be gone before the result was used.
	const WVec& prefilterFeatures(WVec&& in) = delete;
	const WVec& prefilterLabels(WVec&& in) = delete;

	/// Transform a feature matrix to the form for presenting to the inner learner
	WMatrix* prefilterFeatures(const WMatrix& in);

	/// Transform a label matrix to the form for presenting to the inner learner
	WMatrix* prefilterLabels(const WMatrix& in);

	/// Returns a pointer to the inner learner (could be another WFilter)
	WSupervisedLearner* innerLearner() { return m_pLearner; }

protected:
	/// SAKBox: what each filter implements for prefilterFeatures and
	/// prefilterLabels: the transformed vector, or nullptr when in is passed
	/// on unchanged (WFilter then returns in itself). Filters don't return
	/// their parameter, and don't declare prefilterFeatures or
	/// prefilterLabels, which would hide the deleted overloads above.
	virtual const WVec* transformedFeatures(const WVec& in) = 0;
	virtual const WVec* transformedLabels(const WVec& in) = 0;

public:
	/// Returns a pointer to the base larner
	WSupervisedLearner* baseLearner() { return m_pOriginal; }
	/// Throws an exception
	void trainSparse(WSparseMatrix& features, WMatrix& labels) override;
};


class WFeatureFilter : public WFilter
{
protected:
	WIncrementalTransform* m_pTransform;
	bool m_ownTransform;

public:
	/// This takes ownership of pLearner and pTransform.
	WFeatureFilter(WSupervisedLearner* pLearner, WIncrementalTransform* pTransform, bool ownLearner = true, bool ownTransform = true);

	/// Deserialization constructor
	WFeatureFilter(const WDomNode* pNode, WLearnerLoader& ll);

	/// Deletes the supervised learner and the transform
	~WFeatureFilter() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistributionInner
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WIncrementalLearner::trainIncremental
	void trainIncremental(const WVec& in, const WVec& out) override;

protected:
	const WVec* transformedFeatures(const WVec& in) override;
	const WVec* transformedLabels(const WVec& in) override;

	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels) override;
};



class WLabelFilter : public WFilter
{
protected:
	WIncrementalTransform* m_pTransform;
	bool m_ownTransform;

public:
	/// This takes ownership of pLearner and pTransform.
	WLabelFilter(WSupervisedLearner* pLearner, WIncrementalTransform* pTransform, bool ownLearner = true, bool ownTransform = true);

	/// Deserialization constructor
	WLabelFilter(const WDomNode* pNode, WLearnerLoader& ll);

	/// Deletes the supervised learner and the transform
	~WLabelFilter() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WIncrementalLearner::trainIncremental
	void trainIncremental(const WVec& in, const WVec& out) override;

protected:
	const WVec* transformedFeatures(const WVec& in) override;
	const WVec* transformedLabels(const WVec& in) override;

	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels) override;
};




class WAutoFilter : public WFilter
{
protected:
	std::vector<WMatrix*> m_prefilteredData;

public:
	/// General-purpose constructor. If ownLearner is true, then this will delete pLearner when this object is deleted.
	WAutoFilter(WSupervisedLearner* pLearner, bool ownLearner = true);

	/// Deserialization constructor
	WAutoFilter(const WDomNode* pNode, WLearnerLoader& ll);

	/// Deletes the supervised learner and the transform
	~WAutoFilter() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

	/// See the comment for WIncrementalLearner::trainIncremental
	void trainIncremental(const WVec& in, const WVec& out) override;

protected:
	const WVec* transformedFeatures(const WVec& in) override;
	const WVec* transformedLabels(const WVec& in) override;

public:
	/// Prefilters multiple datasets and stores the results in the same order in a vector that can be retrieved by calling data().
	void prefilterData(const WMatrix* pFeatures1, const WMatrix* pLabels1, const WMatrix* pFeatures2 = nullptr, const WMatrix* pLabels2 = nullptr, const WMatrix* pFeatures3 = nullptr, const WMatrix* pLabels3 = nullptr, const WMatrix* pFeatures4 = nullptr, const WMatrix* pLabels4 = nullptr);

	/// Returns a reference to a vector of prefiltered datasets. (This vector is populated by calling prefilterData.)
	std::vector<WMatrix*>& data() { return m_prefilteredData; }

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WRelation& featureRel, const WRelation& labelRel) override;

	/// See the comment for WIncrementalLearner::beginIncrementalLearningInner
	void beginIncrementalLearningInner(const WMatrix& features, const WMatrix& labels) override;

	void whatTypesAreNeeded(const WRelation& featureRel, const WRelation& labelRel, bool& hasNominalFeatures, bool& hasContinuousFeatures, bool& hasNominalLabels, bool& hasContinuousLabels);
	void setupDataDependentFilters(WSupervisedLearner* pLearner, const WMatrix& features, const WMatrix& labels, bool hasNominalFeatures, bool hasContinuousFeatures, bool hasNominalLabels, bool hasContinuousLabels);
	void setupBasicFilters(WSupervisedLearner* pLearner, bool hasNominalFeatures, bool hasContinuousFeatures, bool hasNominalLabels, bool hasContinuousLabels);
	void resetFilters(const WMatrix& features, const WMatrix& labels);
	void resetFilters(const WRelation& features, const WRelation& labels);
};




/// Always outputs the label mean (for continuous labels) and the most common
/// class (for nominal labels).
class WBaselineLearner : public WSupervisedLearner
{
protected:
	std::vector<double> m_prediction;

public:
	/// General-purpose constructor
	WBaselineLearner();

	/// Deserialization constructor
	WBaselineLearner(const WDomNode* pNode);

	/// Destructor
	~WBaselineLearner() override;

	/// Run unit tests for this class.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// This model has no parameters to tune, so this method is a noop.
	void autoTune(WMatrix& features, WMatrix& labels);

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;
};


/// This is an implementation of the identity function. It might be
/// useful, for example, as the observation function in a WRecurrentModel
/// if you want to create a Jordan network.
class WIdentityFunction : public WSupervisedLearner
{
protected:
	size_t m_labelDims;
	size_t m_featureDims;

public:
	/// General-purpose constructor
	WIdentityFunction();

	/// Deserialization constructor
	WIdentityFunction(const WDomNode* pNode);

	/// Destructor
	~WIdentityFunction() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WSupervisedLearner::clear
	void clear() override;

	/// See the comment for WSupervisedLearner::predict
	void predict(const WVec& in, WVec& out) override;

	/// See the comment for WSupervisedLearner::predictDistribution
	void predictDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WSupervisedLearner::trainInner
	void trainInner(const WMatrix& features, const WMatrix& labels) override;
};


} // namespace WClasses

#endif // __GLEARNER_H__

