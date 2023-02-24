#include "PolicyLearner.h"
#include <WClasses/GError.h>
#include <WClasses/GOptimizer.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GActivation.h>
#include <WClasses/GRand.h>
#include <WClasses/GDom.h>
#include <WClasses/GFile.h>
#include <WClasses/GEvolutionary.h>
#include <WClasses/GTime.h>
#include <WClasses/GVec.h>
#include "Body.h"
#include "DemoApplication.h"
#include "btBulletDynamicsCommon.h"

using namespace GClasses;

class PolicyCritic : public GTargetFunction
{
protected:
	GNeuralNet* m_pNN;
	GRand* m_pRand;
	const double* m_pInitialVec;

public:
	PolicyCritic(GNeuralNet* pNN, GRand* pRand, const double* pInitialVec)
	: GTargetFunction(pNN->countWeights()), m_pNN(pNN), m_pRand(pRand), m_pInitialVec(pInitialVec)
	{
	}

	virtual ~PolicyCritic()
	{
	}

	virtual void initVector(double* pVector)
	{
		GVec::copy(pVector, m_pInitialVec, m_pRelation->size());
	}

	virtual bool isStable() { return false; }
	virtual bool isConstrained() { return false; }

protected:
	virtual double computeError(const double* pVector)
	{
		RagdollDemo simulation;
		m_pNN->setWeights(pVector);
		simulation.SetPolicy(m_pNN);
		float height = 0;
		int i;
		for(i = 0; i < 240; i++)
		{
			simulation.advanceTime(1.0f / 60);
			height = std::max(height, simulation.getRagDoll(0)->getLowestPoint());
		}
		return -(double)height;
	}
};



// This is a "seed" jumping policy that I made by hand. It's not very good,
// but we'll use an evolutionary algorithm to refine it.
//                        spine    lhip    lknee   rhip    rknee   lshldr   lelbow   rshldr   relbow    lankle  rankle
const double g_squat[] = { 0.4,    -0.4,    0.4,   -0.4,    0.4,     0.4,     0.4,     0.4,     0.4,    -0.4,    -0.4 };
const double g_jump[] =  { -0.4,    0.4,   -0.4,    0.4,   -0.4,    -0.4,    -0.4,    -0.4,    -0.4,     0.4,     0.4 };
const double g_tuck[] =  { 0.4,    -0.4,    0.4,   -0.4,    0.4,    -0.4,    -0.4,    -0.4,    -0.4,     0.4,     0.4 };

void manualPolicy(const GVec& pIn, GVec& pOut)
{
	GAssert(sizeof(g_squat) == LABEL_DIMS * sizeof(double));
	double time = pIn[0];
	if(time < 0.2)
		pOut.set(g_squat, LABEL_DIMS);
	else if(time < 0.6)
		pOut.set(g_jump, LABEL_DIMS);
	else
		pOut.set(g_tuck, LABEL_DIMS);
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

void Train()
{
	printf("Learning the seed policy...\n");
	GMatrix features(0, FEATURE_DIMS);
	GMatrix labels(0, LABEL_DIMS);
	GNeuralNet nn;
	GRand& prng = nn.rand();
	GenerateSeedTrainingSet(&features, &labels, &prng);
	nn.addLayer(new GLayerClassic(FLEXIBLE_SIZE, 8, new GActivationBend()));
	nn.addLayer(new GLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE, new GActivationBend()));
	nn.setLearningRate(0.01);
	nn.train(features, labels);
	int weightCount = nn.countWeights();
	double hintVec[weightCount];
	//	GTEMPBUF(double, hintVec, weightCount);
	nn.weights(hintVec);

	printf("Initializing a population for the evolutionary search...\n");
	PolicyCritic critic(&nn, &prng, hintVec);
	//GAssert(critic.Critique(hintVec) < -0.5, "bad critique");
	//printf("HintVec Score: %lg\n", critic.Critique(hintVec));
	GEvolutionaryOptimizer search(&critic, 15/*population*/, &prng, .93/*moreFitSurvivalProbability*/);

	printf("Evolving better policies...\n");
	char szBuf[64];
	char szTime[64];
	double err;
	for(int i = 0; i <= 25000; i++)
	{
		err = search.iterate();
		if(i % 250 == 0)
		{
			GDom doc;
			nn.setWeights(search.currentVector());
			doc.setRoot(nn.serialize(&doc));
			sprintf(szBuf, "policy%d.json", i);
			printf("Saving %s (height=%lg) at %s\n", szBuf, -err, GTime::asciiTime(szTime, 64, false));
			doc.saveJson(szBuf);
		}
	}
}

GNeuralNet* LoadPolicy(const char* szFilename, GRand* pRand)
{
	GDom doc;
	doc.loadJson(szFilename);
	GLearnerLoader ll;
	return new GNeuralNet(doc.root());
}

GNeuralNet* TrainPolicy()
{
	GRand* pRand = new GRand(1);
	GMatrix features(0, FEATURE_DIMS);
	GMatrix labels(0, LABEL_DIMS);
	GenerateSeedTrainingSet(&features, &labels, pRand);
	GNeuralNet* pNN = new GNeuralNet();
	//pNN->SetMinImprovement(0.01);
	//pNN->SetIterationsPerValidationCheck(600);
	pNN->addLayer(new GLayerClassic(FLEXIBLE_SIZE, 8, new GActivationBend()));
	pNN->addLayer(new GLayerClassic(FLEXIBLE_SIZE, FLEXIBLE_SIZE, new GActivationBend()));
	pNN->train(features, labels);
	return pNN;
}

