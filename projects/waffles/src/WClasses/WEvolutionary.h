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

#ifndef __GEVOLUTIONARY_H__
#define __GEVOLUTIONARY_H__

#include "WOptimizer.h"
#include <vector>

namespace WClasses
{

class WRand;
class WEvolutionaryOptimizerNode;
class WDiscreteEvolutionaryOptimizerNode;

/// Uses an evolutionary process to optimize a vector.
class WEvolutionaryOptimizer : public WOptimizer
{
protected:
	double m_tournamentProbability;
	WRand* m_pRand;
	std::vector<WEvolutionaryOptimizerNode*> m_population;
	double m_bestErr;
	size_t m_bestIndex;

public:
	/// moreFitSurvivalRate is the probability that the more fit member (in a tournament selection) survives
	WEvolutionaryOptimizer(WTargetFunction* pCritic, size_t population, WRand* pRand, double moreFitSurvivalRate);
	~WEvolutionaryOptimizer() override;

	/// Returns the best vector found in recent iterations.
	const WVec& currentVector() override;

	/// Do a little bit more optimization. (This method is typically called in a loop
	/// until satisfactory results are obtained.)
	double iterate() override;

protected:
	/// Returns the index of the tournament loser (who should typically die and be replaced).
	size_t doTournament();

	void recomputeError(size_t index, WEvolutionaryOptimizerNode* pNode, const WVec& vec);

	WEvolutionaryOptimizerNode* node(size_t index);
};


} // namespace WClasses

#endif // __GEVOLUTIONARY_H__
