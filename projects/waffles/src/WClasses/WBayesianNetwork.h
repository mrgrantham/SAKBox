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

#ifndef __GBN_H__
#define __GBN_H__

#include <vector>
#include <cstddef>
#include "WHeap.h"
#include "WRand.h"

namespace WClasses
{

class WRand;
class WBNCategorical;
class WBNVariable;


/// The base class of all nodes in a Bayesian belief network
class WBNNode
{
protected:
	bool m_observed;
	double m_observedValue;

public:
	WBNNode();
	virtual ~WBNNode();

	/// Returns the current value of this node. If this node represents a
	/// random variable, then it returns the value most-recently sampled
	/// from its distribution.
	virtual double currentValue() = 0;

	/// This method is called when this node is added as the parent to pChild.
	/// It gives this node a chance to link back to the child if needed.
	virtual void onNewChild(WBNVariable* pChild) = 0;
};


/// A node in a belief network that represents a constant value.
class WBNConstant : public WBNNode
{
public:
	/// General-purpose constructor
	WBNConstant(double val) : WBNNode()
	{
		m_observed = true;
		m_observedValue = val;
	}
	~WBNConstant() override {}

	/// Returns the constant value
	double currentValue() override { return m_observedValue; }

	/// This method is a no-op.
	void onNewChild(WBNVariable* pChild) override {}
};


/// A node in a belief network that always returns the sum of its parent nodes.
class WBNSum : public WBNNode
{
protected:
	std::vector<WBNNode*> m_parents;
	bool m_gotChildren;

public:
	/// General-purpose constructor
	WBNSum() : WBNNode(), m_gotChildren(false) { m_observed = true; }
	~WBNSum() override {}

	/// Adds a new parent node to this one, to be included in the summation.
	/// All parents must be added before any child nodes are added.
	void addParent(WBNNode* pNode);

	/// Returns the sum of the current values of all the parents of this node.
	double currentValue() override;

	/// This method links all the parent nodes of this node back to the child.
	void onNewChild(WBNVariable* pChild) override;
};


/// A node in a belief network that always returns the product of its parent nodes.
class WBNProduct : public WBNNode
{
protected:
	std::vector<WBNNode*> m_parents;
	bool m_gotChildren;

public:
	/// General-purpose constructor
	WBNProduct() : WBNNode(), m_gotChildren(false) { m_observed = true; }
	~WBNProduct() override {}

	/// Adds a new parent node to this one, to be included in the summation
	void addParent(WBNNode* pNode);

	/// Returns the sum of the current values of all the parents of this node.
	double currentValue() override;

	/// This method links all the parent nodes of this node back to the child.
	void onNewChild(WBNVariable* pChild) override;
};


/// A node in a belief network that applies some math operation to the output of its one parent node.
class WBNMath : public WBNNode
{
public:
	enum math_op
	{
		NEGATE,
		RECIPROCAL,
		SQUARE_ROOT,
		SQUARE,
		LOG_E,
		EXP,
		TANH,
		GAMMA,
		ABS,
	};

protected:
	WBNNode* m_parent;
	math_op m_op;

public:
	/// General-purpose constructor
	WBNMath(WBNNode* pParent, math_op operation) : WBNNode(), m_parent(pParent), m_op(operation) { m_observed = true; }
	~WBNMath() override {}

	/// Returns the sum of the current values of all the parents of this node.
	double currentValue() override;

	/// This method links the parent node of this node back to the child.
	void onNewChild(WBNVariable* pChild) override;
};


/// The base class of nodes in a belief network that represent variable values.
class WBNVariable : public WBNNode
{
protected:
	std::vector<WBNCategorical*> m_catParents;
	std::vector<WBNVariable*> m_children;

public:
	WBNVariable();
	~WBNVariable() override;

	/// Returns all nodes that are known to depend on this one.
	const std::vector<WBNVariable*>& children() { return m_children; }

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful after you call this method.
	virtual void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) = 0;

	/// Compute the likelihood of drawing the value "x" from the distribution represented
	/// in this node given the current values in all of the parent nodes. If "x" is not
	/// a supported value, then this should return 0.0;
	virtual double likelihood(double x) = 0;

	/// Return the most recent value sampled from this node.
	double currentValue() override = 0;

	/// Draw a new Gibbs sample for this node given the current values of all other nodes
	/// it its Markov blanket.
	virtual void sample(WRand* pRand) = 0;

	/// Set this node to an observed value. After calling this, subsequent calls to
	/// sample will not change its value.
	void setObserved(double value)
	{
		m_observed = true;
		m_observedValue = value;
	}

	/// Set this node to an unobserved value. After calling this, subsequent calls to
	/// sample will draw new values for this node.
	void setUnobserved() { m_observed = false; }

	/// Returns the total number of combinations of categorical values supported by
	/// the categorical parents of this node. This node is expected to specify a
	/// distribution for each of these categorical combinations.
	size_t catCount();

	/// Queries the categorical parent nodes to determine their current values, and
	/// combines them to produce a single index value that is unique for this combination
	/// of categorical values. The value this returns will be from 0 to catCount()-1.
	/// The values are organized in little-endian manner. That is, it cycles through
	/// all values supported by the first categorical parent before moving on to the
	/// next value of the next parent.
	size_t currentCatIndex();

	/// This method links back to the child node that just added this node as a parent.
	void onNewChild(WBNVariable* pChild) override;
};




/// A node in a belief network that represents a categorical distribution.
/// Instances of this class can serve as parent-nodes to any WBNVariable node. The child
/// node must specify distribution parameters for every category that the parent node
/// supports (or for every combination of categories if there are multiply categorical parents).
class WBNCategorical : public WBNVariable
{
protected:
	size_t m_categories;
	size_t m_val;
	std::vector<WBNNode*> m_weights;

public:
	/// General-purpose constructor. All of the categories will initially be given a weight
	/// of pDefaultWeight. Typically, you will want to change these (by calling setWeights)
	/// after you construct a node.
	WBNCategorical(size_t categories, WBNNode* pDefaultWeight);
	~WBNCategorical() override {}

	/// Returns the number of categories supported by this distribution.
	size_t categories() { return m_categories; }

	/// Returns the most recent value sampled from this node.
	double currentValue() override;

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setWeights) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the weights for one of the categorical distributions of this node.
	/// If there are n categories in this node, then the first n parameters should be non-NULL.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this categorical distribution is being specified.
	void setWeights(size_t cat, WBNNode* pW1, WBNNode* pW2, WBNNode* pW3 = nullptr, WBNNode* pW4 = nullptr, WBNNode* pW5 = nullptr, WBNNode* pW6 = nullptr, WBNNode* pW7 = nullptr, WBNNode* pW8 = nullptr);

	/// Draws a new Gibbs sample for this node given the current values of all other nodes
	/// it its Markov blanket.
	void sample(WRand* pRand) override;

	/// Computes the likelihood that the specified value (after being truncated to an integer)
	/// would be drawn from this categorical distribution given the current values of all the
	/// parent nodes of this class.
	double likelihood(double x) override;
};



/// This is the base class for nodes in a belief network that are sampled using
/// the Metropolis algorithm.
class WBNMetropolisNode : public WBNVariable
{
protected:
	double m_currentMean;
	double m_sumOfValues, m_sumOfSquaredValues;

public:
	/// General-purpose constructor.
	WBNMetropolisNode();
	~WBNMetropolisNode() override {}

	/// Returns the most recent value sampled from this node.
	double currentValue() override;

	/// Draws a new Gibbs sample for this node given the current values of all other nodes
	/// it its Markov blanket. Uses the Metropolis algorithm to do so.
	void sample(WRand* pRand) override;

	/// This should return true iff this node supports only discrete values
	virtual bool isDiscrete() = 0;

	/// This should return a value suitable to initialize the mean of the
	/// sampling distribution. It need only return any value with non-negligible probability.
	virtual double initMean() = 0;

protected:
	/// Computes the log-probability of x (as a value for this node) given
	/// the current values for the entire rest of the network (aka the
	/// complete conditional), which is equal to the log-probability of
	/// x given the Markov-Blanket of this node, which we can compute efficiently.
	double markovBlanket(double x);

	/// Sample the network in a manner that can be proven to converge to a
	/// true joint distribution for the network.
	void metropolis(WRand* pRand);
};



/// A node in a belief network that represents a Gaussian, or normal, distribution.
class WBNNormal : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_meanAndDev;
	bool m_devIsVariance;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setMeanAndDev) after constructing this node.
	WBNNormal(WBNNode* pDefaultVal);
	~WBNNormal() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setMeanAndDev) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the mean and deviation for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setMeanAndDev(size_t cat, WBNNode* pMean, WBNNode* pDeviation);

	/// Set the mean and variance for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setMeanAndVariance(size_t cat, WBNNode* pMean, WBNNode* pVariance);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a lognormal distribution.
class WBNLogNormal : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_meanAndDev;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setMeanAndDev) after constructing this node.
	WBNLogNormal(WBNNode* pDefaultVal);
	~WBNLogNormal() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setMeanAndDev) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the mean and deviation for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setMeanAndDev(size_t cat, WBNNode* pMean, WBNNode* pDeviation);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a Pareto distribution.
class WBNPareto : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_alphaAndM;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setAlphaAndM) after constructing this node.
	WBNPareto(WBNNode* pDefaultVal);
	~WBNPareto() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setAlphaAndM) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the alpha and M parameters for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setAlphaAndM(size_t cat, WBNNode* pAlpha, WBNNode* pM);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a uniform discrete distribution.
class WBNUniformDiscrete : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_minAndMax;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setMinAndMax) after constructing this node.
	WBNUniformDiscrete(WBNNode* pDefaultVal);
	~WBNUniformDiscrete() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setMinAndMax) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the min and max parameters for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setMinAndMax(size_t cat, WBNNode* pMin, WBNNode* pMax);

	/// Returns true.
	bool isDiscrete() override { return true; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a uniform continuous distribution.
class WBNUniformContinuous : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_minAndMax;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setMinAndMax) after constructing this node.
	WBNUniformContinuous(WBNNode* pDefaultVal);
	~WBNUniformContinuous() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setMinAndMax) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the min and max parameters for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setMinAndMax(size_t cat, WBNNode* pMin, WBNNode* pMax);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a Poisson distribution.
class WBNPoisson : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_lambda;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameter of this distribution. Typically, you will
	/// change this value (by calling setLambda) after constructing this node.
	WBNPoisson(WBNNode* pDefaultVal);
	~WBNPoisson() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setLambda) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the lambda parameter for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setLambda(size_t cat, WBNNode* pLambda);

	/// Returns true.
	bool isDiscrete() override { return true; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents an exponential distribution.
class WBNExponential : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_lambda;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameter of this distribution. Typically, you will
	/// change this value (by calling setLambda) after constructing this node.
	WBNExponential(WBNNode* pDefaultVal);
	~WBNExponential() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setLambda) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the lambda parameter for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setLambda(size_t cat, WBNNode* pLambda);

	/// Returns true.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a beta distribution.
class WBNBeta : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_alphaAndBeta;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setAlphaAndBeta) after constructing this node.
	WBNBeta(WBNNode* pDefaultVal);
	~WBNBeta() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setAlphaAndBeta) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the alpha and beta values for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents a gamma distribution.
class WBNGamma : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_alphaAndBeta;
	bool m_betaIsScaleInsteadOfRate;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setAlphaAndBeta or setShapeAndScale) after constructing this node.
	/// If the "beta" parameter is scale (typically denoted with theta) instead of rate,
	/// then betaIsScaleInsteadOfRate should be set to true.
	WBNGamma(WBNNode* pDefaultVal);
	~WBNGamma() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setAlphaAndBeta or setShapeAndScale) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the shape and rate (alpha and beta) values for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta);

	/// Set the shape and scale (k and theta) values for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setShapeAndScale(size_t cat, WBNNode* pK, WBNNode* pTheta);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// A node in a belief network that represents an inverse-gamma distribution.
class WBNInverseGamma : public WBNMetropolisNode
{
protected:
	std::vector<WBNNode*> m_alphaAndBeta;

public:
	/// General-purpose constructor. The prior mean and deviation parameters are
	/// for the Metropolis algorithm. pDefaultVal specifies a bogus value to be
	/// used for the parameters of this distribution. Typically, you will
	/// change these values (by calling setAlphaAndBeta) after constructing this node.
	/// If the "beta" parameter is scale (typically denoted with theta) instead of rate,
	/// then betaIsScaleInsteadOfRate should be set to true.
	WBNInverseGamma(WBNNode* pDefaultVal);
	~WBNInverseGamma() override {}

	/// Adds a categorical node as a parent of this node. Calling this method will cause
	/// This node to resize its table of distribution parameters, so a default value is
	/// required to fill in new elements. Typically, you will set these new elements to
	/// something more meaningful (by calling setAlphaAndBeta) after you call this method.
	void addCatParent(WBNCategorical* pNode, WBNNode* pDefaultVal) override;

	/// Set the shape and scale (alpha and beta) values for one of the distributions of this node.
	/// "cat" specifies the index of the combination of values (in little-endian order) in the
	/// categorical parent distributions for which this distribution is being specified.
	void setAlphaAndBeta(size_t cat, WBNNode* pAlpha, WBNNode* pBeta);

	/// Returns false.
	bool isDiscrete() override { return false; }

	/// Returns the likelihood that the value x would be drawn from this distribution given
	/// the current values of all the parent nodes.
	double likelihood(double x) override;

	/// See the comment for WBNMetropolisNode::initMean()
	double initMean() override;
};




/// This class provides a platform for Bayesian belief networks.
/// It allocates nodes in its own heap using placement new, so you don't have to worry about deleting the nodes.
/// You can allocate your nodes manually and use them separately from this class if you want, but it is a lot
/// easier if you use this class to manage it all.
class WBayesNet
{
protected:
	WHeap m_heap;
	std::vector<WBNNode*> m_nodes;
	std::vector<WBNVariable*> m_sampleNodes;
	WRand m_rand;
	WBNConstant* m_pConstOne;

public:
	/// General-purpose constructor
	WBayesNet(size_t seed = 0);
	~WBayesNet();

	/// Performs unit tests for this class. Throws an exception if any tests fail.
	static void test();

	/// Returns a reference to the pseudo-random number generator used by this network.
	/// You might use this, for example, to change the random seed.
	WRand& rand() { return m_rand; }

	/// Return a constant value that can be used as a default parameter for various nodes.
	WBNConstant* def() { return m_pConstOne; }

	/// Return a pointer to a node that represents a constant value.
	WBNConstant* newConst(double val);

	/// Return a pointer to a node that sums all of its parent values.
	WBNSum* newSum();

	/// Return a pointer to a node that sums all of its parent values.
	WBNProduct* newProduct();

	/// Return a pointer to a node that sums all of its parent values.
	WBNMath* newMath(WBNNode* pParent, WBNMath::math_op operation);

	/// Return a pointer to a node that represents a categorical distribution.
	WBNCategorical* newCat(size_t categories);

	/// Return a pointer to a node that represents a normal distribution.
	WBNNormal* newNormal();

	/// Return a pointer to a node that represents a lognormal distribution.
	WBNLogNormal* newLogNormal();

	/// Return a pointer to a node that represents a Pareto distribution.
	WBNPareto* newPareto();

	/// Return a pointer to a node that represents a uniform discrete distribution.
	WBNUniformDiscrete* newUniformDiscrete();

	/// Return a pointer to a node that represents a uniform continuous distribution.
	WBNUniformContinuous* newUniformContinuous();

	/// Return a pointer to a node that represents a Poisson distribution.
	WBNPoisson* newPoisson();

	/// Return a pointer to a node that represents an Exponential distribution.
	WBNExponential* newExponential();

	/// Return a pointer to a node that represents a Beta distribution.
	WBNBeta* newBeta();

	/// Return a pointer to a node that represents a Gamma distribution.
	WBNGamma* newGamma();

	/// Return a pointer to a node that represents an Inverse-Gamma distribution.
	WBNInverseGamma* newInverseGamma();

	/// Draw a Gibbs sample for each node in the graph in random order.
	void sample();
};



} // namespace WClasses

#endif // __GBN_H__
