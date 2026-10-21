#include "PolicyLearner.h"
#include <WClasses/WError.h>
#include <WClasses/WOptimizer.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>
#include <WClasses/WDom.h>
#include <WClasses/WFile.h>
#include <WClasses/WEvolutionary.h>
#include <WClasses/WTime.h>
#include <WClasses/WVec.h>
#include "Body.h"
#include "DemoApplication.h"
#include "btBulletDynamicsCommon.h"
#include <algorithm>
#include <cstdio>

// SAKBox: ported from the 2015 network API (WNeuralNet as a learner,
// WLayerClassic, WActivationBend) to blocks: a policy is a WNeuralNetLearner
// of WBlockLinear and WBlockBentIdentity (the old "bend" function, the same
// formula), and the evolutionary search works on the network's weight vector.

using namespace WClasses;

std::unique_ptr<WNeuralNetLearner> MakePolicy()
{
	auto pPolicy = std::make_unique<WNeuralNetLearner>();
	pPolicy->nn().add(new WBlockLinear(FEATURE_DIMS, 8), new WBlockBentIdentity(8));
	pPolicy->nn().add(new WBlockLinear(8, LABEL_DIMS), new WBlockBentIdentity(LABEL_DIMS));
	WUniformRelation features(FEATURE_DIMS);
	WUniformRelation labels(LABEL_DIMS);
	pPolicy->beginIncrementalLearning(features, labels); // (allocates the weights)
	return pPolicy;
}

double JumpHeight(WNeuralNetLearner* pPolicy)
{
	RagdollDemo simulation;
	simulation.SetPolicy(pPolicy);
	float height = 0;
	for(int i = 0; i < 240; i++)
	{
		simulation.advanceTime(1.0f / 60);
		height = std::max(height, simulation.getRagDoll(0)->getLowestPoint());
	}
	return height;
}

// Scores a vector of weights for the policy by how high it makes the robot jump
class PolicyCritic : public WTargetFunction
{
protected:
	WNeuralNetLearner& m_policy;
	const WVec& m_initialWeights;

public:
	PolicyCritic(WNeuralNetLearner& policy, const WVec& initialWeights)
	: WTargetFunction(policy.nn().weightCount()), m_policy(policy), m_initialWeights(initialWeights)
	{
	}

	void initVector(WVec& vector) override
	{
		vector.copy(m_initialWeights);
	}

	bool isStable() override { return false; }

protected:
	double computeError(const WVec& vector) override
	{
		m_policy.nn().weights.copy(vector);
		return -JumpHeight(&m_policy);
	}
};



// This is a "seed" jumping policy that I made by hand. It's not very good,
// but we'll use an evolutionary algorithm to refine it.
//                        spine    lhip    lknee   rhip    rknee   lshldr   lelbow   rshldr   relbow    lankle  rankle
const double g_squat[] = {0.4, -0.4, 0.4, -0.4, 0.4, 0.4, 0.4, 0.4, 0.4, -0.4, -0.4};
const double g_jump[] = {-0.4, 0.4, -0.4, 0.4, -0.4, -0.4, -0.4, -0.4, -0.4, 0.4, 0.4};
const double g_tuck[] = {0.4, -0.4, 0.4, -0.4, 0.4, -0.4, -0.4, -0.4, -0.4, 0.4, 0.4};

void manualPolicy(const WVec& pIn, WVec& pOut)
{
	WAssert(sizeof(g_squat) == LABEL_DIMS * sizeof(double));
	double time = pIn[0];
	if(time < 0.2)
		pOut.copy(g_squat, LABEL_DIMS);
	else if(time < 0.6)
		pOut.copy(g_jump, LABEL_DIMS);
	else
		pOut.copy(g_tuck, LABEL_DIMS);
}

void GenerateSeedTrainingSet(WMatrix* pFeatures, WMatrix* pLabels, WRand* pRand)
{
	WAssert(FEATURE_DIMS == 3);
	double d;
	for(d = 0.0; d < 1.2; d += 0.0001)
	{
		WVec& pPat = pFeatures->newRow();
		pPat[0] = d;
		pPat[1] = pRand->normal();
		pPat[2] = pRand->normal();
		manualPolicy(pPat, pLabels->newRow());
	}
}

void Train(size_t generations, size_t saveEvery)
{
	printf("Learning the seed policy...\n");
	WRand prng(0);
	WMatrix features(0, FEATURE_DIMS);
	WMatrix labels(0, LABEL_DIMS);
	GenerateSeedTrainingSet(&features, &labels, &prng);
	std::unique_ptr<WNeuralNetLearner> pPolicy = MakePolicy();
	WNeuralNetOptimizer& optimizer = pPolicy->optimizer();
	optimizer.setLearningRate(0.01);
	optimizer.optimizeWithValidation(features, labels);
	WVec hintVec;
	hintVec.copy(pPolicy->nn().weights);

	printf("Initializing a population for the evolutionary search...\n");
	PolicyCritic critic(*pPolicy, hintVec);
	WEvolutionaryOptimizer search(&critic, 15 /*population*/, &prng, .93 /*moreFitSurvivalProbability*/);

	printf("Evolving better policies...\n");
	char szBuf[64];
	char szTime[64];
	for(size_t i = 0; i <= generations; i++)
	{
		double err = search.iterate();
		if(i % saveEvery == 0)
		{
			pPolicy->nn().weights.copy(search.currentVector());
			snprintf(szBuf, sizeof(szBuf), "policy%zu.json", i);
			printf("Saving %s (height=%lg) at %s\n", szBuf, -err, WTime::asciiTime(szTime, 64, false));
			fflush(stdout);
			SavePolicy(*pPolicy, szBuf);
		}
	}
}

void SavePolicy(WNeuralNetLearner& policy, const char* szFilename)
{
	WDom doc;
	doc.setRoot(policy.nn().serialize(&doc));
	doc.saveJson(szFilename);
}

std::unique_ptr<WNeuralNetLearner> LoadPolicy(const char* szFilename)
{
	WDom doc;
	doc.loadJson(szFilename);
	WRand rand(0);
	WNeuralNet saved(doc.root(), rand);
	std::unique_ptr<WNeuralNetLearner> pPolicy = MakePolicy();
	if(saved.weightCount() != pPolicy->nn().weightCount() || saved.inputs() != FEATURE_DIMS || saved.outputs() != LABEL_DIMS)
		throw Ex(szFilename, " isn't a jumper policy (a ", to_str(FEATURE_DIMS), "-input, ", to_str(LABEL_DIMS), "-output network)");
	pPolicy->nn().weights.copy(saved.weights);
	return pPolicy;
}
