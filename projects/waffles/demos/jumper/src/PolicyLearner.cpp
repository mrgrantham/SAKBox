#include "PolicyLearner.h"
#include <WClasses/GError.h>
#include <WClasses/GOptimizer.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GRand.h>
#include <WClasses/GDom.h>
#include <WClasses/GFile.h>
#include <WClasses/GEvolutionary.h>
#include <WClasses/GTime.h>
#include <WClasses/GVec.h>
#include "Body.h"
#include "DemoApplication.h"
#include "btBulletDynamicsCommon.h"
#include <algorithm>
#include <cstdio>

// SAKBox: ported from the 2015 network API (GNeuralNet as a learner,
// GLayerClassic, GActivationBend) to blocks: a policy is a GNeuralNetLearner
// of GBlockLinear and GBlockBentIdentity (the old "bend" function, the same
// formula), and the evolutionary search works on the network's weight vector.

using namespace GClasses;

std::unique_ptr<GNeuralNetLearner> MakePolicy()
{
	auto pPolicy = std::make_unique<GNeuralNetLearner>();
	pPolicy->nn().add(new GBlockLinear(FEATURE_DIMS, 8), new GBlockBentIdentity(8));
	pPolicy->nn().add(new GBlockLinear(8, LABEL_DIMS), new GBlockBentIdentity(LABEL_DIMS));
	GUniformRelation features(FEATURE_DIMS);
	GUniformRelation labels(LABEL_DIMS);
	pPolicy->beginIncrementalLearning(features, labels); // (allocates the weights)
	return pPolicy;
}

double JumpHeight(GNeuralNetLearner* pPolicy)
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
class PolicyCritic : public GTargetFunction
{
protected:
	GNeuralNetLearner& m_policy;
	const GVec& m_initialWeights;

public:
	PolicyCritic(GNeuralNetLearner& policy, const GVec& initialWeights)
	: GTargetFunction(policy.nn().weightCount()), m_policy(policy), m_initialWeights(initialWeights)
	{
	}

	void initVector(GVec& vector) override
	{
		vector.copy(m_initialWeights);
	}

	bool isStable() override { return false; }

protected:
	double computeError(const GVec& vector) override
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

void manualPolicy(const GVec& pIn, GVec& pOut)
{
	GAssert(sizeof(g_squat) == LABEL_DIMS * sizeof(double));
	double time = pIn[0];
	if(time < 0.2)
		pOut.copy(g_squat, LABEL_DIMS);
	else if(time < 0.6)
		pOut.copy(g_jump, LABEL_DIMS);
	else
		pOut.copy(g_tuck, LABEL_DIMS);
}

void GenerateSeedTrainingSet(GMatrix* pFeatures, GMatrix* pLabels, GRand* pRand)
{
	GAssert(FEATURE_DIMS == 3);
	double d;
	for(d = 0.0; d < 1.2; d += 0.0001)
	{
		GVec& pPat = pFeatures->newRow();
		pPat[0] = d;
		pPat[1] = pRand->normal();
		pPat[2] = pRand->normal();
		manualPolicy(pPat, pLabels->newRow());
	}
}

void Train(size_t generations, size_t saveEvery)
{
	printf("Learning the seed policy...\n");
	GRand prng(0);
	GMatrix features(0, FEATURE_DIMS);
	GMatrix labels(0, LABEL_DIMS);
	GenerateSeedTrainingSet(&features, &labels, &prng);
	std::unique_ptr<GNeuralNetLearner> pPolicy = MakePolicy();
	GNeuralNetOptimizer& optimizer = pPolicy->optimizer();
	optimizer.setLearningRate(0.01);
	optimizer.optimizeWithValidation(features, labels);
	GVec hintVec;
	hintVec.copy(pPolicy->nn().weights);

	printf("Initializing a population for the evolutionary search...\n");
	PolicyCritic critic(*pPolicy, hintVec);
	GEvolutionaryOptimizer search(&critic, 15 /*population*/, &prng, .93 /*moreFitSurvivalProbability*/);

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
			printf("Saving %s (height=%lg) at %s\n", szBuf, -err, GTime::asciiTime(szTime, 64, false));
			fflush(stdout);
			SavePolicy(*pPolicy, szBuf);
		}
	}
}

void SavePolicy(GNeuralNetLearner& policy, const char* szFilename)
{
	GDom doc;
	doc.setRoot(policy.nn().serialize(&doc));
	doc.saveJson(szFilename);
}

std::unique_ptr<GNeuralNetLearner> LoadPolicy(const char* szFilename)
{
	GDom doc;
	doc.loadJson(szFilename);
	GRand rand(0);
	GNeuralNet saved(doc.root(), rand);
	std::unique_ptr<GNeuralNetLearner> pPolicy = MakePolicy();
	if(saved.weightCount() != pPolicy->nn().weightCount() || saved.inputs() != FEATURE_DIMS || saved.outputs() != LABEL_DIMS)
		throw Ex(szFilename, " isn't a jumper policy (a ", to_str(FEATURE_DIMS), "-input, ", to_str(LABEL_DIMS), "-output network)");
	pPolicy->nn().weights.copy(saved.weights);
	return pPolicy;
}
