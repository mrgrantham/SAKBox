// PolicyLearner: jumper's policy network, its training, and the simulation it
// is scored in. (No window: the simulation runs without one.)
#include <gtest/gtest.h>

#include "PolicyLearner.h"

#include <WClasses/WDom.h>
#include <WClasses/WError.h>
#include <WClasses/WNeuralNet.h>
#include <WClasses/WRand.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <unistd.h>

using namespace WClasses;

namespace {
std::string tempPath(const std::string &name) {
  const char *dir = getenv("TEST_TMPDIR");
  return std::string(dir ? dir : ".") + "/" + name;
}
} // namespace

// The hand-made policy's jump, as Cyclone simulates it, to the last bit: the
// same every run, so a policy is scored the same in training and when it's
// shown, and the same on every platform and compiler, since the build turns
// fused multiply-adds off and Cyclone's maths functions are correctly
// rounded (see the top-level README's "Floating-point results across
// platforms"). Checked on macOS (Apple clang) and on Linux (GCC 14 and
// Clang, on ARM64 and x86-64). It's 0.5399 m.
TEST(PolicyLearnerTest, SimulationIsTheSameEverywhere) {
  const double height = JumpHeight(nullptr);
  EXPECT_EQ(height, 0x1.1467e00e32fbep-1) << std::hexfloat << height;
  EXPECT_EQ(JumpHeight(nullptr), height);
}

// With its motors off, the robot slumps like a ragdoll, and stays in one
// piece as it does: every hinge within its limits (past them by no more than
// the resolver's slop), its joints together, and nothing through the floor.
TEST(PolicyLearnerTest, LimpRobotSlumpsWithinItsLimits) {
  Robot robot;
  robot.world().setSubSteps(kSubSteps);
  double furthestPastLimit = 0, widestJoint = 0, lowest = 0;
  for (int i = 0; i < 120; i++) {
    robot.step(kStepSeconds);
    for (unsigned h = 0; h < Robot::hingeCount; h++) {
      const auto &hinge = static_cast<const cyclone::HingeJoint &>(*robot.joints()[h]);
      const double angle = robot.angle(Robot::Hinge(h));
      furthestPastLimit =
          std::max<double>({furthestPastLimit, hinge.lowerLimit - angle, angle - hinge.upperLimit});
    }
    for (const cyclone::Joint *joint : robot.joints()) {
      joint->body[0]->calculateDerivedData();
      joint->body[1]->calculateDerivedData();
      widestJoint =
          std::max<double>(widestJoint, (joint->body[1]->getPointInWorldSpace(joint->position[1]) -
                                         joint->body[0]->getPointInWorldSpace(joint->position[0]))
                                            .magnitude());
    }
    lowest = std::min(lowest, robot.lowestPoint());
  }
  EXPECT_LT(furthestPastLimit, 0.05);
  EXPECT_LT(widestJoint, 0.01);
  // It sinks briefly as it lands (the position sweeps take overlap out a
  // fifth a step), and rests within the resolver's tolerance (1 cm)
  EXPECT_GT(lowest, -0.04);
  EXPECT_GT(robot.lowestPoint(), -0.011);
  EXPECT_LT(robot.uprightness(), 0.99); // it did slump
}

TEST(PolicyLearnerTest, PolicyMapsThreeFeaturesToElevenJoints) {
  std::unique_ptr<WNeuralNetLearner> policy = MakePolicy();
  EXPECT_EQ(policy->nn().inputs(), size_t(FEATURE_DIMS));
  EXPECT_EQ(policy->nn().outputs(), size_t(LABEL_DIMS));
  EXPECT_EQ(policy->relFeatures().size(), size_t(FEATURE_DIMS));
  EXPECT_EQ(policy->relLabels().size(), size_t(LABEL_DIMS));
}

// A saved policy loads back with the same weights, so it jumps exactly as high
TEST(PolicyLearnerTest, SavedPolicyJumpsTheSameWhenLoaded) {
  std::unique_ptr<WNeuralNetLearner> policy = MakePolicy();
  WRand rand(7);
  for (size_t i = 0; i < policy->nn().weights.size(); i++)
    policy->nn().weights[i] = rand.normal() * 0.5;
  const std::string path = tempPath("policy.json");
  SavePolicy(*policy, path.c_str());
  std::unique_ptr<WNeuralNetLearner> loaded = LoadPolicy(path.c_str());
  EXPECT_EQ(JumpHeight(loaded.get()), JumpHeight(policy.get()));
}

TEST(PolicyLearnerTest, LoadingAnotherNetworkIsAnError) {
  WNeuralNet other;
  other.add(new WBlockLinear(2, 4));
  WRand rand(0);
  other.init(rand);
  WDom doc;
  doc.setRoot(other.serialize(&doc));
  const std::string path = tempPath("other.json");
  doc.saveJson(path.c_str());
  try {
    LoadPolicy(path.c_str());
    FAIL() << "expected an error";
  } catch (const std::exception &e) {
    EXPECT_NE(std::string(e.what()).find("isn't a jumper policy"), std::string::npos) << e.what();
  }
}

// A short training run saves policies that load and jump, and the search
// keeps the best it has found, so a later policy jumps at least as high.
TEST(PolicyLearnerTest, TrainingSavesPoliciesThatLoad) {
  const std::string dir = tempPath("training");
  std::filesystem::create_directories(dir);
  char previous[4096];
  ASSERT_NE(getcwd(previous, sizeof(previous)), nullptr);
  ASSERT_EQ(chdir(dir.c_str()), 0);
  testing::internal::CaptureStdout();
  Train(4, 2);
  testing::internal::GetCapturedStdout();
  double heights[3];
  for (int i = 0; i < 3; i++) {
    std::unique_ptr<WNeuralNetLearner> policy =
        LoadPolicy(("policy" + std::to_string(i * 2) + ".json").c_str());
    heights[i] = JumpHeight(policy.get());
    EXPECT_GT(heights[i], 0.0);
  }
  EXPECT_GE(heights[2], heights[0]);
  ASSERT_EQ(chdir(previous), 0);
}
