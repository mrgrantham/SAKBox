#ifndef POLICY_LEARNER
#define POLICY_LEARNER

namespace GClasses
{
	class GRand;
	class GNeuralNet;
	class GVec;
}

void Train();
GClasses::GNeuralNet* LoadPolicy(const char* szFilename, GClasses::GRand* pRand);
GClasses::GNeuralNet* TrainPolicy();
void manualPolicy(const GClasses::GVec& pIn, GClasses::GVec& pOut);

#define FEATURE_DIMS 3
#define LABEL_DIMS 11

#endif // POLICY_LEARNER
