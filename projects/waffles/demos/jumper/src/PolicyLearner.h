#ifndef POLICY_LEARNER
#define POLICY_LEARNER

#include "Robot.h"
#include <cstddef>
#include <memory>

namespace WClasses
{
class WNeuralNetLearner;
class WVec;
}

#define FEATURE_DIMS 3
#define LABEL_DIMS 11
static_assert(LABEL_DIMS == Robot::hingeCount, "the policy moves each hinge");

/// How the jump is simulated, in training and in the window alike: steps of
/// 1/60 s, each in two sub-steps (as the Cyclone demos' FixedSteps), with
/// room for this many contacts (as their RigidBodyApplication)
const double kStepSeconds = 1.0 / 60;
const unsigned kSubSteps = 2;
const unsigned kMaxContacts = 256;

/// The robot moved by a policy (or by manualPolicy, given none): before
/// each physics step, the policy reads the time and how the robot is lying,
/// and sets its motors
class Jumper
{
public:
	explicit Jumper(WClasses::WNeuralNetLearner* pPolicy);

	Robot& robot() { return m_robot; }

	/// Seconds since the jump began
	double time() const { return m_time; }

	/// Stands the robot up to jump again
	void reset();

	/// The start of a physics step: the policy acts, and the robot moves on
	/// by `duration` (its contacts are then found and resolved)
	void move(double duration);

protected:
	Robot m_robot;
	WClasses::WNeuralNetLearner* m_pPolicy;
	double m_time = 0;
};

/// A policy network, 3 -> 8 -> 11 with bent-identity units, ready to use: its
/// weights are allocated (randomly) and it knows its feature and label counts.
std::unique_ptr<WClasses::WNeuralNetLearner> MakePolicy();

/// Trains a policy to imitate manualPolicy, then improves it with an
/// evolutionary search for `generations` generations, saving policy<N>.json
/// in the current folder every `saveEvery` generations.
void Train(size_t generations = 25000, size_t saveEvery = 250);

/// Saves a policy as JSON, as Train does
void SavePolicy(WClasses::WNeuralNetLearner& policy, const char* szFilename);

/// Loads a policy saved by Train or SavePolicy
std::unique_ptr<WClasses::WNeuralNetLearner> LoadPolicy(const char* szFilename);

/// The highest the robot's lowest point gets in four seconds of simulation
/// (240 steps of 1/60 s) under this policy (or manualPolicy if null). This is
/// what training maximizes.
double JumpHeight(WClasses::WNeuralNetLearner* pPolicy);

/// A hand-made policy: squat, jump, then tuck
void manualPolicy(const WClasses::WVec& pIn, WClasses::WVec& pOut);

#endif // POLICY_LEARNER
