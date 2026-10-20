#ifndef POLICY_LEARNER
#define POLICY_LEARNER

#include <cstddef>
#include <memory>

namespace GClasses
{
class GNeuralNetLearner;
class GVec;
}

#define FEATURE_DIMS 3
#define LABEL_DIMS 11

/// A policy network, 3 -> 8 -> 11 with bent-identity units, ready to use: its
/// weights are allocated (randomly) and it knows its feature and label counts.
std::unique_ptr<GClasses::GNeuralNetLearner> MakePolicy();

/// Trains a policy to imitate manualPolicy, then improves it with an
/// evolutionary search for `generations` generations, saving policy<N>.json
/// in the current folder every `saveEvery` generations.
void Train(size_t generations = 25000, size_t saveEvery = 250);

/// Saves a policy as JSON, as Train does
void SavePolicy(GClasses::GNeuralNetLearner& policy, const char* szFilename);

/// Loads a policy saved by Train or SavePolicy
std::unique_ptr<GClasses::GNeuralNetLearner> LoadPolicy(const char* szFilename);

/// The highest the robot's lowest point gets in four seconds of simulation
/// (240 steps of 1/60 s) under this policy (or manualPolicy if null). This is
/// what training maximizes.
double JumpHeight(GClasses::GNeuralNetLearner* pPolicy);

/// A hand-made policy: squat, jump, then tuck
void manualPolicy(const GClasses::GVec& pIn, GClasses::GVec& pOut);

#endif // POLICY_LEARNER
