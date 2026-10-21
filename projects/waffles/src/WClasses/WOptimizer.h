/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Luke B. Godfrey,
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

#ifndef __GSEARCH_H__
#define __GSEARCH_H__

#include "WError.h"
#include "WMatrix.h"
#include "WRand.h"
#include "WCudaMatrix.h"
#include <vector>

namespace WClasses
{

class WActionPath;
class WAction;
class WRand;
class WNeuralNet;
class WContextNeuralNet;


/// Optimizes the parameters of a differentiable function using an objective function.
class WNeuralNetOptimizer
{
protected:
	WNeuralNet& m_model;

	const WMatrix* m_pTrainingFeatures;
	const WMatrix* m_pTrainingLabels;
#ifdef WCUDA
	WCudaEngine m_cudaEngine;
	WCudaMatrix* m_pTrainingFeaturesCuda;
	WCudaMatrix* m_pTrainingLabelsCuda;
	bool m_useGPU;
#endif // WCUDA

	// variables for convenience training methods
	WRand& m_rand;
	size_t m_batchSize, m_batchesPerEpoch, m_epochs, m_windowSize;
	double m_minImprovement;
	double m_learningRate;
	WRandomIndexIterator* m_pII;

public:
	WNeuralNetOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures = nullptr, const WMatrix* pTrainingLabels = nullptr);
	virtual ~WNeuralNetOptimizer();

	/// Prepare for optimization (i.e. allocate delta vectors).
	virtual void init() = 0;

	/// Evaluate feat and lab, and update the model's gradient.
	virtual void computeGradient(const WVec& feat, const WVec& lab) = 0;

	/// Step the model's parameters in the direction of the calculated gradient scaled by learningRate.
	virtual void descendGradient(double learningRate) = 0;

#ifdef WCUDA
	virtual void computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab) = 0;
	virtual void descendGradientCuda(double learningRate) = 0;
	void optimizeIncrementalCuda(const WCudaVector& feat, const WCudaVector& lab);
#endif // WCUDA

	/// Flushes the memory in any recurrent units in the network.
	/// This method should be called when beginning a new training sequence with neural networks that contain any recurrent blocks.
	void resetState();

	/// Update and apply the gradient for a single training sample (on-line).
	virtual void optimizeIncremental(const WVec& feat, const WVec& lab);

	void optimizeEpoch();

	/// Update and apply the gradient for a single batch in order.
	virtual void optimizeBatch(const WMatrix& features, const WMatrix& labels, size_t start, size_t batchSize);
	void optimizeBatch(const WMatrix& features, const WMatrix& labels, size_t start);

	/// Update and apply the gradient for a single batch in randomized order.
	virtual void optimizeBatch(const WMatrix& features, const WMatrix& labels, WRandomIndexIterator& ii, size_t batchSize);
	void optimizeBatch(const WMatrix& features, const WMatrix& labels, WRandomIndexIterator& ii);

	// convenience training methods

	void optimize(const WMatrix& features, const WMatrix& labels);
	void optimizeWithValidation(const WMatrix& features, const WMatrix& labels, const WMatrix& validationFeat, const WMatrix& validationLab);
	void optimizeWithValidation(const WMatrix& features, const WMatrix& labels, double validationPortion = 0.35);

	// getters/setters
#ifdef WCUDA
	void useGPU(bool b) { m_useGPU = b; }
#endif

	WNeuralNet& model() { return m_model; }

	WRand& rand() { return m_rand; }

	void setBatchSize(size_t b) { m_batchSize = b; }
	size_t batchSize() const { return m_batchSize; }

	void setBatchesPerEpoch(size_t b) { m_batchesPerEpoch = b; }
	size_t batchesPerEpoch() const { return m_batchesPerEpoch; }

	void setEpochs(size_t e) { m_epochs = e; }
	size_t epochs() const { return m_epochs; }

	void setWindowSize(size_t w) { m_windowSize = w; }
	size_t windowSize() const { return m_windowSize; }

	void setImprovementThresh(double m) { m_minImprovement = m; }
	double improvementThresh() const { return m_minImprovement; }

	void setLearningRate(double l) { m_learningRate = l; }
	double learningRate() const { return m_learningRate; }
};


/// Trains a neural network by stochastic gradient descent.
class WSGDOptimizer : public WNeuralNetOptimizer
{
public:
	WSGDOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures = nullptr, const WMatrix* pTrainingLabels = nullptr);

	/// Prepare for optimization (i.e. allocate buffers).
	void init() override;

	/// Evaluate feat and lab, and update the model's gradient.
	void computeGradient(const WVec& feat, const WVec& lab) override;

	/// Step the model's parameters in the direction of the calculated gradient scaled by learningRate.
	void descendGradient(double learningRate) override;

#ifdef WCUDA
	virtual void computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab);
	virtual void descendGradientCuda(double learningRate);
#endif // WCUDA

	void setMomentum(double m) { m_momentum = m; }
	double momentum() const { return m_momentum; }

private:
	WVec m_gradient;
	double m_momentum;
#ifdef WCUDA
	WCudaVector m_gradientCuda;
#endif // WCUDA
};



/// Trains a neural network by ADAM.
/// See Diederik P. Kingma and Jimmy Lei Ba, "Adam: A Method for Stochastic Optimization", 2015.
class WAdamOptimizer : public WNeuralNetOptimizer
{
public:
	WAdamOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures = nullptr, const WMatrix* pTrainingLabels = nullptr);

	/// Prepare for optimization (i.e. allocate buffers).
	void init() override;

	/// Evaluate feat and lab, and update the model's gradient.
	void computeGradient(const WVec& feat, const WVec& lab) override;

	/// Step the model's parameters in the direction of the calculated gradient scaled by learningRate.
	void descendGradient(double learningRate) override;

#ifdef WCUDA
	virtual void computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab);
	virtual void descendGradientCuda(double learningRate);
#endif // WCUDA

	void setBeta1(double b) { m_beta1 = b; }
	double beta1() const { return m_beta1; }
	void setBeta2(double b) { m_beta2 = b; }
	double beta2() const { return m_beta2; }
	void setEpsilon(double e) { m_epsilon = e; }
	double epsilon() const { return m_epsilon; }

private:
	WVec m_gradient, m_deltas, m_sqdeltas;
	double m_correct1, m_correct2, m_beta1, m_beta2, m_epsilon;
};



/// Trains a neural network with RMS-prop.
class WRMSPropOptimizer : public WNeuralNetOptimizer
{
public:
	WRMSPropOptimizer(WNeuralNet& model, WRand& rand, const WMatrix* pTrainingFeatures = nullptr, const WMatrix* pTrainingLabels = nullptr);

	/// Prepare for optimization (i.e. allocate buffers).
	void init() override;

	/// Evaluate feat and lab, and update the model's gradient.
	void computeGradient(const WVec& feat, const WVec& lab) override;

	/// Step the model's parameters in the direction of the calculated gradient scaled by learningRate.
	void descendGradient(double learningRate) override;

#ifdef WCUDA
	virtual void computeGradientCuda(const WCudaVector& feat, const WCudaVector& lab);
	virtual void descendGradientCuda(double learningRate);
#endif // WCUDA

	void setMomentum(double m) { m_momentum = m; }
	double momentum() const { return m_momentum; }
	void setGamma(double g) { m_gamma = g; }
	double gamma() const { return m_gamma; }

private:
	WVec m_gradient, m_meanSquare;
	double m_momentum, m_gamma, m_epsilon;
};





/// The optimizer seeks to find values that minimize this target function.
class WTargetFunction
{
protected:
	WRelation* m_pRelation;

public:
	/// Takes ownership of pRelation
	WTargetFunction(WRelation* pRelation) : m_pRelation(pRelation) {}

	WTargetFunction(size_t dims);

	virtual ~WTargetFunction();

	/// Returns a (smart) pointer to the relation, which specifies the type
	/// (discrete or real) of each element in the vector that is being optimized.
	WRelation* relation() { return m_pRelation; }

	/// This method should return true if computeError is deterministic with respect to
	/// the vector being optimized. It should return false if the error depends on some
	/// state other than the vector being optimized. This mostly affects whether
	/// the optimization algorithms are permitted to remember old error values for
	/// efficiency purposes. Stable is assumed, so you should override this method if
	/// your target function is not stable.
	virtual bool isStable() { return true; }

	/// Sets pVector to an initial guess. The default behavior is to initialize the
	/// vector to all zeros. You should override this method if different behavior
	/// is desired.
	virtual void initVector(WVec& vector);

	/// Computes the error of the given vector using all patterns
	virtual double computeError(const WVec& vector) = 0;
};


class WOptimizerBasicTestTargetFunction : public WTargetFunction
{
public:
	WOptimizerBasicTestTargetFunction() : WTargetFunction(3) {}

	double computeError(const WVec& vector) override;
};


/// This is the base class of all search algorithms
/// that can jump to any vector in the search space
/// seek the vector that minimizes error.
class WOptimizer
{
protected:
	WTargetFunction* m_pCritic;

public:
	WOptimizer(WTargetFunction* pCritic);
	virtual ~WOptimizer();

	/// Makes another attempt to find a better vector. Returns
	/// the heuristic error. (Usually you will call this method
	/// in a loop until your stopping criteria has been met.)
	virtual double iterate() = 0;

	/// Returns the current vector of the optimizer. For greedy search
	/// methods, this will be the best vector yet found.
	virtual const WVec& currentVector() = 0;

	/// This will first call iterate() nBurnInIterations times,
	/// then it will repeatedly call iterate() in blocks of
	/// nIterations times. If the error heuristic has not improved
	/// by the specified ratio after a block of iterations, it will
	/// stop. (For example, if the error before the block of iterations
	/// was 50, and the error after is 49, then training will stop
	/// if dImprovement is > 0.02.) If the error heuristic is not
	/// stable, then the value of nIterations should be large.
	double searchUntil(size_t nBurnInIterations, size_t nIterations, double dImprovement);

	/// This is a helper method used by the unit tests of several model learners
	void basicTest(double minAccuracy, double warnRange = 0.001);
};



/// This class simplifies simultaneously solving several optimization problems
class WParallelOptimizers
{
protected:
	WRelation* m_pRelation;
	std::vector<WTargetFunction*> m_targetFunctions;
	std::vector<WOptimizer*> m_optimizers;

public:
	/// If the problems all have the same number of dims, and they're all continuous, you can call
	/// relation() to get a relation for constructing the target functions. Otherwise, use dims=0
	/// and don't call relation().
	WParallelOptimizers(size_t dims);
	~WParallelOptimizers();

	/// Returns the relation associated with these optimizers
	WRelation* relation() { return m_pRelation; }

	/// Takes ownership of pTargetFunction and pOptimizer
	void add(WTargetFunction* pTargetFunction, WOptimizer* pOptimizer);

	/// Returns a vector of pointers to the optimizers
	std::vector<WOptimizer*>& optimizers() { return m_optimizers; }

	/// Returns a vector of pointers to the target functions
	std::vector<WTargetFunction*>& targetFunctions() { return m_targetFunctions; }

	/// Perform one iteration on all of the optimizers
	double iterateAll();

	/// Optimize until the specified conditions are met
	double searchUntil(size_t nBurnInIterations, size_t nIterations, double dImprovement);
};



class WActionPathState
{
	friend class WActionPath;
public:
	WActionPathState() {}
	virtual ~WActionPathState() {}

protected:
	/// Performs the specified action on the state. (so pState holds
	/// both input and output data.) This method is protected because
	/// you should call WActionPath::doAction, and it will call this method.
	virtual void performAction(size_t nAction) = 0;

	/// Creates a deep copy of this state object
	virtual WActionPathState* copy() = 0;

protected:
	/// Evaluate the error of the given path. Many search algorithms
	/// (like WAStarSearch) rely heavily on the heuristic to make the search effective.
	/// For example, if you don't penalize redundant paths to the same state, the search
	/// space becomes exponential and therefore impossible to search. So a good critic
	/// must keep track of which states have already been visited, severely penalize longer
	/// paths to a state that has already been visited by a shorter path, and will carefully
	/// balance between path length and distance from the goal in producing the error value.
	virtual double critiquePath(size_t nPathLen, WAction* pLastAction) = 0;
};




class WActionPath
{
protected:
	WActionPathState* m_pHeadState;
	WAction* m_pLastAction;
	size_t m_nPathLen;

public:
	/// Takes ownership of pState
	WActionPath(WActionPathState* pState);
	~WActionPath();

	/// Makes a copy of this path
	WActionPath* fork();

	/// Returns the number of actions in the path
	size_t length() { return m_nPathLen; }

	/// Gets the first nCount actions of the specified path
	void path(size_t nCount, size_t* pOutBuf);

	/// Returns the head-state of the path
	WActionPathState* state() { return m_pHeadState; }

	/// Adds the specified action to the path and modifies the head state accordingly
	void doAction(size_t nAction);

	/// Computes the error of this path
	double critique();
};




/// This is the base class of search algorithms that can
/// only perform a discreet set of actions (as opposed to jumping
/// to anywhere in the search space), and seeks to minimize the
/// error of a path of actions
class WActionPathSearch
{
protected:
	size_t m_nActionCount;

public:
	/// Takes ownership of pStartState
	WActionPathSearch(WActionPathState* pStartState, size_t nActionCount)
	{
		WAssert(nActionCount > 1); // not enough actions for a meaningful search")
		m_nActionCount = nActionCount;
	}

	virtual ~WActionPathSearch()
	{
	}

	/// Returns the number of possible actions
	inline size_t actionCount() { return m_nActionCount; }

	/// Call this in a loop to do the searching. If it returns
	/// true, then it's done so don't call it anymore.
	virtual bool iterate() = 0;

	/// Returns the best known path so far
	virtual WActionPath* bestPath() = 0;

	/// Returns the error of the best known path
	virtual double bestPathError() = 0;
};


} // namespace WClasses

#endif // __GSEARCH_H__
