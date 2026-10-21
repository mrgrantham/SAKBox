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

#ifndef __GPARTICLESWARM_H__
#define __GPARTICLESWARM_H__

#include "WOptimizer.h"
#include "WMatrix.h"

namespace WClasses
{

class WRand;


/// An optimization algorithm inspired by flocking birds
class WParticleSwarm : public WOptimizer
{
protected:
	double m_dMin, m_dRange;
	double m_dLearningRate;
	size_t m_nDimensions;
	size_t m_nPopulation;
	WMatrix m_pPositions;
	WMatrix m_pVelocities;
	WMatrix m_pBests;
	WVec m_pErrors;
	size_t m_nGlobalBest;
	WRand* m_pRand;

public:
	WParticleSwarm(WTargetFunction* pCritic, size_t nPopulation, double dMin, double dRange, WRand* pRand);
	~WParticleSwarm() override;

	/// Perform a little more optimization
	double iterate() override;

	/// Specify the learning rate
	void setLearningRate(double d) { m_dLearningRate = d; }

protected:
	void reset();
};


// An optimization algorithm inspired by boucing balls
class WBouncyBalls : public WOptimizer
{
public:
	WMatrix m_positions;
	WMatrix m_velocities;
	WVec m_errors;
	size_t m_bestIndex;
	size_t m_ball;
	WRand& m_rand;
	double m_probTeleport;
	double m_probSpurt;

	WBouncyBalls(WTargetFunction* pCritic, size_t population, WRand& rand, double probTeleport = 0.01);
	~WBouncyBalls() override;

	static void test();

	double iterate() override;
	const WVec& currentVector() override { return m_positions[m_bestIndex]; }
};


} // namespace WClasses

#endif // __GPARTICLESWARM_H__
