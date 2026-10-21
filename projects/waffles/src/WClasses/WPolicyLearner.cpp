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

#include "WPolicyLearner.h"
#include "WKNN.h"
#include "WDecisionTree.h"
#include "WNeighborFinder.h"
#include "WOptimizer.h"
#include <stdlib.h>
#include "WVec.h"
#include "WRand.h"
//#include "WImage.h"
#include "WHeap.h"
#include "WHillClimber.h"
#include "WDom.h"
#include <deque>
#include <math.h>

namespace WClasses
{

using std::deque;

// virtual
void WDiscreteActionIterator::reset(const double* pState)
{
	m_action = 0;
}

// virtual
void WDiscreteActionIterator::randomAction(double* pOutAction, WRand* pRand)
{
	*pOutAction = (double)pRand->next(m_count);
}

// virtual
bool WDiscreteActionIterator::nextAction(double* pOutAction)
{
	if(m_action < m_count)
	{
		*pOutAction = m_action++;
		return true;
	}
	else
		return false;
}

// -----------------------------------------------------------------------------

WPolicyLearner::WPolicyLearner(const WRelation& relation, int actionDims)
: m_pRelation(relation.clone())
{
	m_senseDims = (int)m_pRelation->size() - actionDims;
	if(m_senseDims < 0)
		throw Ex("more action dims than relation dims");
	m_actionDims = actionDims;
	m_teleported = true;
	m_explore = true;
}

WPolicyLearner::WPolicyLearner(WDomNode* pAgent)
{
	m_pRelation = WRelation::deserialize(pAgent->get("relation"));
	m_actionDims = (int)pAgent->getInt("actionDims");
	m_senseDims = (int)m_pRelation->size() - m_actionDims;
	m_teleported = true;
}

// virtual
WPolicyLearner::~WPolicyLearner()
{
	delete(m_pRelation);
}


WDomNode* WPolicyLearner::baseDomNode(WDom* pDoc)
{
	WDomNode* pNode = pDoc->newObj();
	pNode->add(pDoc, "actionDims", (long long)m_actionDims);
	pNode->add(pDoc, "relation", m_pRelation->serialize(pDoc));
	return pNode;
}


} // namespace WClasses

