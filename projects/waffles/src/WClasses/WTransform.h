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

#ifndef __GTRANSFORM_H__
#define __GTRANSFORM_H__

#include "WLearner.h"

namespace WClasses
{

/// This is the base class of algorithms that transform data without supervision
class WTransform
{
public:
	WTransform();
	WTransform(const WDomNode* pNode);
	virtual ~WTransform();

	/// Applies the transformation to pIn and returns the results.
	virtual WMatrix* reduce(const WMatrix& in) = 0;

protected:
	/// Child classes should use this in their implementation of serialize
	virtual WDomNode* baseDomNode(WDom* pDoc, const char* szClassName) const;
};




/// This is the base class of algorithms that can transform data
/// one row at a time without supervision.
class WIncrementalTransform : public WTransform
{
private:
	WRelation* m_pRelationBefore;
	WRelation* m_pRelationAfter;
	WVec m_innerBuf;

public:
	WIncrementalTransform() : WTransform(), m_pRelationBefore(nullptr), m_pRelationAfter(nullptr) {}
	WIncrementalTransform(const WDomNode* pNode);
	~WIncrementalTransform() override;

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	virtual WDomNode* serialize(WDom* pDoc) const = 0;

	/// Trains the transform on the data in pData. (This method may be a no-op
	/// for transformations that always behave in the same manner.)
	void train(const WMatrix& data);

	/// "Trains" the transform without any data.
	///
	/// This method is called to initialize the transform
	/// when it is used with an incremental learner.
	/// Transforms that cannot be trained without available
	/// data may throw an exception in this method.
	void train(const WRelation& pRelation);

	/// Sets the before relation. Takes ownership of pRel.
	void setBefore(WRelation* pRel);

	/// Sets the after relation. Takes ownership of pRel.
	void setAfter(WRelation* pRel);

	/// Returns a relation object describing the data before it is
	/// transformed
	///
	/// train must be called before this method is used
	const WRelation& before() const { return *m_pRelationBefore; }

	/// Returns a relation object describing the data after it is
	/// transformed
	///
	/// train must be called before this method is used
	const WRelation& after() const { return *m_pRelationAfter; }

	/// pIn is the source row. pOut is a buffer that will hold the
	/// transformed row.  train must be called before this method
	/// is used
	virtual void transform(const WVec& in, WVec& out) = 0;

	/// This calls train, then calls transformBatch, and returns the result.
	WMatrix* reduce(const WMatrix& in) override;

	/// This assumes that train has already been called, and
	/// transforms all the rows in in returning the resulting
	/// matrix.  The caller is responsible for deleting the new
	/// matrix.
	virtual WMatrix* transformBatch(const WMatrix& in);

	/// Returns a buffer of sufficient size to store an inner
	/// (transformed) vector. The caller should not to delete
	/// the buffer. The same buffer will be returned each time.
	WVec& innerBuf();

	/// pIn is a previously transformed row, and pOut is a buffer that will hold the untransformed row.
	/// train must be called before this method is used.
	/// This method may throw an exception if this transformation cannot be undone or approximately undone.
	virtual void untransform(const WVec& in, WVec& out) = 0;

	/// Similar to untransform, except it produces a distribution instead of just a vector.
	/// This method may not be implemented in all classes, so it may throw an exception.
	virtual void untransformToDistribution(const WVec& in, WPrediction* pOut) = 0;

	/// This assumes train was previously called, and untransforms all the rows in pIn and returns the results.
	virtual std::unique_ptr<WMatrix> untransformBatch(const WMatrix& in);

protected:
	/// Child classes should use this in their implementation of serialize
	WDomNode* baseDomNode(WDom* pDoc, const char* szClassName) const override;

	/// This method implements the functionality called by train.
	/// The data passed in may be used to guide training.
	/// This method returns a smart-pointer to a relation the represents
	/// the form that the data will take after it is transformed.
	virtual WRelation* trainInner(const WMatrix& data) = 0;

	/// This method implements the functionality called by train.
	/// This method is called to initialize the transform
	/// when it is used with an incremental learner.
	/// Transforms that cannot be trained without available
	/// data may throw an exception in this method.
	/// The relation passed in represents the form that the input
	/// data will have.
	/// This method returns a smart-pointer to a relation the represents
	/// the form that the data will take after it is transformed.
	virtual WRelation* trainInner(const WRelation& relation) = 0;
};




/// This class facilitates automatic data preprocessing.
/// The constructor requires a dataset and some parameters that specify which portion of the data to use, and how the data should be transformed.
/// A transform will automatically be generated to preprocess the data to meet the specified requirements, and the data will be transformed.
/// Additional datasets can be added with the "add" method.
/// These will be processed by the same transform.
/// The processed data can be retrieved with the "get" method, with an index corresponding to the order of the data.
class WDataPreprocessor
{
protected:
	WIncrementalTransform* m_pTransform;
	std::vector<WMatrix*> m_processedData;

public:
	/// Generates a transform and preprocesses the specified portion of source to meet the specified data requirements.
	WDataPreprocessor(const WMatrix& source, size_t rowStart = 0, size_t colStart = 0, size_t rowCount = (size_t)-1, size_t colCount = (size_t)-1, bool allowMissing = false, bool allowNominal = false, bool allowContinuous = true, double minVal = -1.0, double maxVal = 1.0);

	/// Adds another dataset, and transforms it with the same transform used to preprocess the original data.
	void add(const WMatrix& source, size_t rowStart = 0, size_t colStart = 0, size_t rowCount = (size_t)-1, size_t colCount = (size_t)-1);

	/// Returns the preprocessed version of the data.
	/// index specifies which dataset, such that get(0) returns the preprocessed form of the original
	/// data passed to the constructor, get(1) returns the preprocessed form of the first dataset added
	/// by a call to "add", get(2) is the preprocessed form of the second dataset added, and so forth.
	WMatrix& get(size_t index) { return *m_processedData[index]; }

	/// Automatically prepares and trains a minimal transform for the provided data to prep it to meet specified requirements.
	/// Returns nullptr if no transformation is needed.
	static WIncrementalTransform* autoTrans(const WMatrix& data, bool allowMissing = false, bool allowNominal = false, bool allowContinuous = true, double minVal = -1.0, double maxVal = 1.0);
};




/// This wraps two two-way-incremental-transforms to form a single combination transform
class WIncrementalTransformChainer : public WIncrementalTransform
{
protected:
	WIncrementalTransform* m_pFirst;
	WIncrementalTransform* m_pSecond;

public:
	/// General-purpose constructor
	WIncrementalTransformChainer(WIncrementalTransform* pFirst, WIncrementalTransform* pSecond);

	/// Deserializing constructor
	WIncrementalTransformChainer(const WDomNode* pNode, WLearnerLoader& ll);
	~WIncrementalTransformChainer() override;

	/// If pA or pB is nullptr, returns the other one, else chains them together.
	static WIncrementalTransform* chain(WIncrementalTransform* pA, WIncrementalTransform* pB);

	/// See the comment for WIncrementalTransform::serialize
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::train
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WRelation& relation) override;
};






/// Principal Component Analysis. (Computes the principal components about
/// the mean of the data when you call train. The transformed (reduced-dimensional)
/// data will have a mean about the origin.)
class WPCA : public WIncrementalTransform
{
protected:
	size_t m_targetDims;
	WMatrix* m_pBasisVectors;
	WVec m_eigVals;
	bool m_aboutOrigin;
	WRand m_rand;

public:
	WPCA(size_t targetDims);

	/// Load from a DOM.
	WPCA(const WDomNode* pNode);

	~WPCA() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// Specify to compute the eigenvalues during training. This
	/// method must be called before train is called.
	void computeEigVals();

	/// Specify to compute the principal components about the origin (instead
	/// of computing them about the mean).
	void aboutOrigin() { m_aboutOrigin = true; }

	/// Returns the eigenvalues. Returns NULL if computeEigVals was not called.
	WVec& eigVals() { return m_eigVals; }

	/// Returns the number of principal components that it will find.
	size_t targetDims() { return m_targetDims; }

	/// Returns the centroid about which the principal components were calculated.
	WVec& centroid() { return m_pBasisVectors->row(0); }

	/// Returns a dataset where the first row is the centroid, and the remaining
	/// rows are the principal component vectors in order of decreasing eigenvalue.
	WMatrix* components() { return m_pBasisVectors; }

	/// See the comment for WIncrementalTransform::transform.
	/// Projects the specified point into fewer dimensions.
	void transform(const WVec& in, WVec& out) override;

	/// Computes a (lossy) high-dimensional point that corresponds with the
	/// specified low-dimensional coordinates.
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

	/// Returns a reference to the pseudo-random number generator used by this object.
	WRand& rand() { return m_rand; }

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// Throws an exception (because this transform cannot be trained without data)
	WRelation* trainInner(const WRelation& relation) override;
};




/// Just generates Gaussian noise
class WNoiseGenerator : public WIncrementalTransform
{
protected:
	WRand m_rand;
	double m_mean, m_deviation;

public:
	WNoiseGenerator();

	/// Load from a DOM.
	WNoiseGenerator(const WDomNode* pNode);

	~WNoiseGenerator() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// Sets the mean and deviation of the noise
	void setMeanAndDeviation(double m, double d)
	{
		m_mean = m;
		m_deviation = d;
	}

	/// Throws an exception (because this transform cannot be reversed).
	void untransform(const WVec& in, WVec& out) override
	{ throw Ex("This transformation cannot be reversed"); }

	/// Throws an exception (because this transform cannot be undone).
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override
	{ throw Ex("This transformation cannot be reversed"); }

	/// Returns a reference to the rand object used to generate noise.
	WRand& rand() { return m_rand; }

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WRelation& relation) override;
};



/// Generates data by computing the product of each pair of
/// attributes. This is useful for augmenting data.
class WPairProduct : public WIncrementalTransform
{
protected:
	size_t m_maxDims;

public:
	WPairProduct(size_t nMaxDims);

	/// Load from a DOM.
	WPairProduct(const WDomNode* pNode);

	~WPairProduct() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// Throws an exception (because this transform cannot be reversed).
	void untransform(const WVec& in, WVec& out) override
	{ throw Ex("This transformation cannot be reversed"); }

	/// Throws an exception (because this transform cannot be undone).
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override
	{ throw Ex("This transformation cannot be reversed"); }

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WRelation& relation) override;
};


/// This class augments data. That is, it transforms the data according to
/// some provided transformation, and attaches the transformed data as new
/// attributes to the existing data.
class WDataAugmenter : public WIncrementalTransform
{
protected:
	WIncrementalTransform* m_pTransform;

public:
	/// Generic constructor. pTransform specifies any data transformation. The transformed
	/// data will be attached to the existing data in order to augment it.
	/// This method takes ownership of pTransform.
	WDataAugmenter(WIncrementalTransform* pTransform);

	/// Load from a DOM
	WDataAugmenter(const WDomNode* pNode, WLearnerLoader& ll);

	~WDataAugmenter() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// Throws an exception
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WRelation& relation) override;
};


/// Generates subsets of data that contain only the most relevant features for predicting the labels.
/// The train method of this class produces a ranked ordering of the feature attributes by training
/// a single-layer neural network, and deselecting the weakest attribute until all attributes have been
/// deselected. The transform method uses only the highest-ranked attributes.
class WAttributeSelector : public WIncrementalTransform
{
protected:
	size_t m_labelDims;
	size_t m_targetFeatures;
	std::vector<size_t> m_ranks;
	size_t m_seed;

public:
	WAttributeSelector(size_t labelDims, size_t targetFeatures) : WIncrementalTransform(), m_labelDims(labelDims), m_targetFeatures(targetFeatures), m_seed(1234567)
	{
	}

	WAttributeSelector(const WDomNode* pNode);

	~WAttributeSelector() override
	{
	}

	static void test();

	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// Specifies the number of features to select. (This method must be called
	/// after train.)
	WRelation* setTargetFeatures(size_t n);

	/// Returns a list of attributes in ranked-order. Most important attributes are first. Weakest attributes are last.
	/// (The results are undefined until after train is called.)
	std::vector<size_t>& ranks() { return m_ranks; }

	/// Throws an exception (because this transform cannot be reversed).
	void untransform(const WVec& in, WVec& out) override
	{ throw Ex("This transformation cannot be reversed"); }

	/// Throws an exception (because this transform cannot be reversed).
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override
	{ throw Ex("This transformation cannot be reversed"); }

	/// Sets a random seed to use with this attribute selector
	void setSeed(size_t seed) { m_seed = seed; }

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// Throws an exception (because this transform cannot be trained without data)
	WRelation* trainInner(const WRelation& relation) override;
};


/// This is sort-of the opposite of discretize. It converts each nominal attribute to a categorical
/// distribution by representing each value using the corresponding row of the identity matrix. For
/// example, if a certain nominal attribute has 4 possible values, then a value of 3 would be encoded
/// as the vector 0 0 1 0. When predictions are converted back to nominal values, the mode of the
/// categorical distribution is used as the predicted value. (This is similar to Weka's
/// NominalToBinaryFilter.)
class WNominalToCat : public WIncrementalTransform
{
protected:
	size_t m_valueCap;
	WRand* m_pRand;
	std::vector<size_t> m_ranks;
	bool m_preserveUnknowns;
	double m_lo, m_hi;
public:
	WNominalToCat(size_t valueCap = 12, double lo = 0.0, double hi = 1.0);

	/// Load from a DOM.
	WNominalToCat(const WDomNode* pNode);

	~WNominalToCat() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

	/// Makes a mapping from the post-transform attribute indexes to the pre-transform attribute indexes
	void reverseAttrMap(std::vector<size_t>& rmap);

	/// Specify to preserve unknown values. That is, an unknown nominal value will be
	/// converted to a distribution of all unknown real values.
	void preserveUnknowns() { m_preserveUnknowns = true; }

protected:
	/// Whether a column with nValues values becomes one column per value.
	/// (Numbers, columns with 2 values and columns with m_valueCap or more
	/// stay a single column.)
	bool oneHot(size_t nValues) const { return nValues >= 3 && nValues < m_valueCap; }

	WRelation* init();

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WRelation& relation) override;
};



/// This transform scales and shifts continuous values
/// to make them fall within a specified range.
class WNormalize : public WIncrementalTransform
{
protected:
	double m_min, m_max;
	WVec m_mins;
	WVec m_ranges;

public:
	/// min and max specify the target range. (The input domain is determined
	/// automatically when train is called.)
	WNormalize(double min = 0.0, double max = 1.0);

	/// Load from a DOM.
	WNormalize(const WDomNode* pNode);

	~WNormalize() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

	/// Specify the input min and range values for each attribute
	void setMinsAndRanges(const WRelation& pRel, const WVec& mins, const WVec& ranges);

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// Throws an exception (because this transform cannot be trained without data)
	WRelation* trainInner(const WRelation& relation) override;
};



/// This transform uses buckets to convert continuous data into discrete data.
/// It is common to use WFilter to combine this with your favorite modeler
/// (which only supports discrete values) to create a modeler that can also support
/// continuous values as well.
class WDiscretize : public WIncrementalTransform
{
protected:
	size_t m_bucketsIn, m_bucketsOut;
	WVec m_mins;
	WVec m_ranges;

public:
	/// if buckets is less than 0, then it will use the floor of the square root of the number of rows in the data
	WDiscretize(size_t buckets = INVALID_INDEX);

	/// Load from a DOM.
	WDiscretize(const WDomNode* pNode);

	~WDiscretize() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// Throws an exception (because this transform cannot be trained without data)
	WRelation* trainInner(const WRelation& relation) override;
};



class WImputeMissingVals : public WIncrementalTransform
{
protected:
	WCollaborativeFilter* m_pCF;
	WNominalToCat* m_pNTC;
	const WMatrix* m_pLabels;
	WMatrix* m_pBatch;

public:
	/// General-purpose constructor
	WImputeMissingVals();

	/// Deserializing constructor
	WImputeMissingVals(const WDomNode* pNode, WLearnerLoader& ll);

	~WImputeMissingVals() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

	/// Sets the collaborative filter used to impute missing values. Takes
	/// ownership of pCF. If no collaborative filter is set, the default is to use
	/// matrix factorization with some typical parameters.
	void setCollaborativeFilter(WCollaborativeFilter* pCF);

	/// Specify a label matrix that should be appended with the training data.
	/// This object will not delete pLabels. It is expected that pLabels will remain
	/// valid at least until after train is next called.
	void setLabels(const WMatrix* pLabels);

	/// Unlike most other transforms, this one assumes that the matrix passed
	/// to this method is the same that was used to train it. (This assumption
	/// is necessary in order to utilize the additional label information that
	/// may be available at training time, which can be important for imputation.)
	WMatrix* transformBatch(const WMatrix& in) override;

protected:
	/// See the comment for WIncrementalTransform::train
	WRelation* trainInner(const WMatrix& data) override;

	/// Throws an exception (because this transform cannot be trained without data)
	WRelation* trainInner(const WRelation& relation) override;
};


/// This transform converts continuous values into logarithmic space.
class WLogify : public WIncrementalTransform
{
public:
	/// General purpose constructor
	WLogify();

	/// Load from a DOM.
	WLogify(const WDomNode* pNode);

	~WLogify() override;

	/// Marshal this object into a DOM, which can then be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const override;

	/// See the comment for WIncrementalTransform::transform
	void transform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransform
	void untransform(const WVec& in, WVec& out) override;

	/// See the comment for WIncrementalTransform::untransformToDistribution
	void untransformToDistribution(const WVec& in, WPrediction* pOut) override;

protected:
	/// See the comment for WIncrementalTransform::trainInner
	WRelation* trainInner(const WMatrix& data) override;

	/// See the comment for WIncrementalTransform::trainInner
	WRelation* trainInner(const WRelation& relation) override;
};

} // namespace WClasses

#endif // __GTRANSFORM_H__

