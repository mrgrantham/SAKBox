// PolicyLearner: jumper's policy network, its training, and the simulation it
// is scored in. (No window: the simulation runs without one.)
#include <gtest/gtest.h>

#include "PolicyLearner.h"

#include <WClasses/GDom.h>
#include <WClasses/GError.h>
#include <WClasses/GNeuralNet.h>
#include <WClasses/GRand.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

using namespace GClasses;

namespace {
std::string tempPath(const std::string &name) {
  const char *dir = getenv("TEST_TMPDIR");
  return std::string(dir ? dir : ".") + "/" + name;
}
} // namespace

// The hand-made policy's jump, as Bullet 3.26 simulates it. Each run is the
// same, so a policy is scored the same in training and when it's shown. If
// a Bullet upgrade changes the physics, this changes.
TEST(PolicyLearnerTest, SimulationIsRepeatable) {
  const double height = JumpHeight(nullptr);
  EXPECT_NEAR(height, 0.628827, 1e-6);
  EXPECT_EQ(JumpHeight(nullptr), height);
}

TEST(PolicyLearnerTest, PolicyMapsThreeFeaturesToElevenJoints) {
  std::unique_ptr<GNeuralNetLearner> policy = MakePolicy();
  EXPECT_EQ(policy->nn().inputs(), size_t(FEATURE_DIMS));
  EXPECT_EQ(policy->nn().outputs(), size_t(LABEL_DIMS));
  EXPECT_EQ(policy->relFeatures().size(), size_t(FEATURE_DIMS));
  EXPECT_EQ(policy->relLabels().size(), size_t(LABEL_DIMS));
}

// A saved policy loads back with the same weights, so it jumps exactly as high
TEST(PolicyLearnerTest, SavedPolicyJumpsTheSameWhenLoaded) {
  std::unique_ptr<GNeuralNetLearner> policy = MakePolicy();
  GRand rand(7);
  for (size_t i = 0; i < policy->nn().weights.size(); i++)
    policy->nn().weights[i] = rand.normal() * 0.5;
  const std::string path = tempPath("policy.json");
  SavePolicy(*policy, path.c_str());
  std::unique_ptr<GNeuralNetLearner> loaded = LoadPolicy(path.c_str());
  EXPECT_EQ(JumpHeight(loaded.get()), JumpHeight(policy.get()));
}

TEST(PolicyLearnerTest, LoadingAnotherNetworkIsAnError) {
  GNeuralNet other;
  other.add(new GBlockLinear(2, 4));
  GRand rand(0);
  other.init(rand);
  GDom doc;
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
    std::unique_ptr<GNeuralNetLearner> policy =
        LoadPolicy(("policy" + std::to_string(i * 2) + ".json").c_str());
    heights[i] = JumpHeight(policy.get());
    EXPECT_GT(heights[i], 0.0);
  }
  EXPECT_GE(heights[2], heights[0]);
  ASSERT_EQ(chdir(previous), 0);
}
