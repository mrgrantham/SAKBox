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

#ifndef __GGREEDYSEARCH_H__
#define __GGREEDYSEARCH_H__

#include "WOptimizer.h"
#include "WVec.h"
#include "WRand.h"

namespace WClasses
{


/// At each iteration this algorithm moves in only one
/// dimension. If the situation doesn't improve it tries
/// the opposite direction. If both directions are worse,
/// it decreases the step size for that dimension, otherwise
/// it increases the step size for that dimension.
class WMomentumGreedySearch : public WOptimizer
{
protected:
	size_t m_nDimensions;
	size_t m_nCurrentDim;
	WVec m_pStepSizes;
	WVec m_pVector;
	double m_dError;
	double m_dChangeFactor;

public:
	WMomentumGreedySearch(WTargetFunction* pCritic);
	~WMomentumGreedySearch() override;

	/// Performs unit testing. Throws an exception if any test fails.
	static void test();

	/// Returns a pointer to the state vector
	const WVec& currentVector() override { return m_pVector; }

	/// Set all the current step sizes to this value
	void setAllStepSizes(double dStepSize);

	/// Returns the vector of step sizes
	WVec& stepSizes();

	double iterate() override;

	/// d should be a value between 0 and 1
	void setChangeFactor(double d) { m_dChangeFactor = d; }

protected:
	void reset();
	double iterateOneDim();
};


/// In each dimension, tries 5 candidate adjustments:
/// a lot smaller, a little smaller, same spot, a little bigger, and a lot bigger.
/// If it picks a smaller adjustment, the step size in that dimension is made smaller.
/// If it picks a bigger adjustment, the step size in that dimension is made bigger.
class WHillClimber : public WOptimizer
{
protected:
	size_t m_nDims;
	size_t m_dim;
	WVec m_pStepSizes;
	WVec m_pVector;
	WVec m_pAnnealCand;
	double m_dError;
	double m_dChangeFactor;

public:
	WHillClimber(WTargetFunction* pCritic);
	~WHillClimber() override;

	/// Performs unit testing. Throws an exception if any test fails.
	static void test();

	/// Returns a pointer to the current vector
	const WVec& currentVector() override { return m_pVector; }

	/// Returns the error for the current vector
	double currentError() { return m_dError; }

	/// Set all the current step sizes to this value
	void setStepSizes(double size);

	/// Returns the vector of step sizes
	WVec& stepSizes();

	double iterate() override;

	/// You can call this method to simulate one annealing jump with the
	/// specified deviation in all dimensions.
	double anneal(double dev, WRand* pRand);

	/// d should be a value between 0 and 1
	void setChangeFactor(double d) { m_dChangeFactor = d; }

protected:
	void reset();
};



/// Perturbs the current vector in a random direction.
/// If it made the vector worse, restores the previous vector.
/// Decays the deviation of perturbation over time.
class WAnnealing : public WOptimizer
{
protected:
	double m_initialDeviation;
	double m_deviation;
	double m_decay;
	size_t m_dims;
	WVec m_pVector;
	WVec m_pCandidate;
	double m_dError;
	WRand* m_pRand;

public:
	WAnnealing(WTargetFunction* pTargetFunc, WRand* pRand);
	~WAnnealing() override;

	/// Performs unit testing. Throws an exception if any test fails.
	static void test();

	/// Performs a little more optimization. (Call this in a loop until
	/// acceptable results are found.)
	double iterate() override;

	/// Returns the best vector yet found.
	const WVec& currentVector() override { return m_pVector; }

	/// Specify the current deviation to use for annealing. (A random vector
	/// from a Normal distribution with the specified deviation will be added to each
	/// candidate vector in order to simulate annealing.)
	void setDeviation(double d) { m_deviation = d; }

protected:
	void reset();
};




/// This algorithm picks a random direction, then uses binary search
/// to determine how far to step, and repeats
class WRandomDirectionBinarySearch : public WOptimizer
{
protected:
	WVec m_direction;
	WVec m_current;
	size_t m_dims;
	double m_stepSize;
	double m_err;
	WRand* m_pRand;

public:
	WRandomDirectionBinarySearch(WTargetFunction* pTargetFunc, WRand* pRand);
	~WRandomDirectionBinarySearch() override;

	/// Performs unit testing. Throws an exception if any test fails.
	static void test();

	/// Performs a little more optimization. (Call this in a loop until
	/// acceptable results are found.)
	double iterate() override;

	/// Returns the best vector yet found.
	const WVec& currentVector() override { return m_current; }
};




/// This algorithm does a gradient descent by feeling a small distance
/// out in each dimension to measure the gradient. For efficiency reasons,
/// it only measures the gradient in one dimension (which it cycles
/// round-robin style) per iteration and uses the remembered gradient
/// in the other dimensions.
class WEmpiricalGradientDescent : public WOptimizer
{
protected:
	double m_dLearningRate;
	size_t m_nDimensions;
	WVec m_pVector;
	WVec m_pGradient;
	WVec m_pDelta;
	double m_dFeelDistance;
	double m_dMomentum;
	WRand* m_pRand;

public:
	WEmpiricalGradientDescent(WTargetFunction* pCritic, WRand* pRand);
	~WEmpiricalGradientDescent() override;

	/// Returns the best vector yet found.
	const WVec& currentVector() override { return m_pVector; }

	/// Performs a little more optimization. (Call this in a loop until
	/// acceptable results are found.)
	double iterate() override;

	/// Sets the learning rate
	void setLearningRate(double d) { m_dLearningRate = d; }

	/// Sets the momentum value
	void setMomentum(double d) { m_dMomentum = d; }

protected:
	void reset();
};



/// This is a variant of empirical gradient descent that tries to estimate
/// the gradient using a minimal number of samples. It is more efficient
/// than empirical gradient descent, but it only works well if the optimization
/// surface is quite locally linear.
class WSampleClimber : public WOptimizer
{
protected:
	WRand* m_pRand;
	double m_dStepSize;
	double m_alpha;
	double m_error;
	size_t m_dims;
	WVec m_pVector;
	WVec m_pDir;
	WVec m_pCand;
	WVec m_pGradient;

public:
	WSampleClimber(WTargetFunction* pCritic, WRand* pRand);
	~WSampleClimber() override;

	/// Returns the best vector yet found
	const WVec& currentVector() override { return m_pVector; }

	/// Performs a little more optimization. (Call this in a loop until
	/// acceptable results are found.)
	double iterate() override;

	/// Sets the current step size
	void setStepSize(double d) { m_dStepSize = d; }

	/// Sets the alpha value. It should be small (like 0.01)
	/// A very small value updates the gradient estimate
	/// slowly, but precisely. A bigger value updates the
	/// estimate quickly, but never converges very close to
	/// the precise gradient.
	void setAlpha(double d) { m_alpha = d; }

protected:
	void reset();
};


} // namespace WClasses

#endif // __GGREEDYSEARCH_H__
