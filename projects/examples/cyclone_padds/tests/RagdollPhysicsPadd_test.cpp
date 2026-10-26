// The physics padd in a running system, with a test padd standing in for the
// window: it records the snapshots and control snapshots that come back, and
// sends what the window's keys and panel send. No window needed.
#include "RagdollPhysicsPadd.hpp"

#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using namespace CyclonePadds;
using namespace std::chrono_literals;
namespace Controls = ScratchPadd::ControlType;
using ScratchPadd::MessageType::ControlChange;
using ScratchPadd::MessageType::ControlSnapshot;

namespace {

class ViewerPadd : public ScratchPadd::Base {
public:
  ViewerPadd(ScratchPadd::System *system) : Base(system) {
    receive<RagdollSnapshot>([this](const RagdollSnapshot &snapshot) {
      std::lock_guard<std::mutex> lk(mutex_);
      snapshots_.push_back(snapshot);
    });
    receive<ControlSnapshot>([this](const ControlSnapshot &snapshot) {
      std::lock_guard<std::mutex> lk(mutex_);
      controlSnapshots_.push_back(snapshot);
    });
  }
  std::string name() override { return "Viewer"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void command(RagdollCommand::Type type) { send(RagdollCommand{type}); }
  bool changeControl(const std::string &control, const ScratchPadd::ControlTypeVariant &value) {
    return sendTo<RagdollPhysicsPadd>(
        ControlChange{.controlName = control, .controlTypeValue = value});
  }

  std::vector<RagdollSnapshot> snapshots() {
    std::lock_guard<std::mutex> lk(mutex_);
    return snapshots_;
  }
  std::vector<ControlSnapshot> controlSnapshots() {
    std::lock_guard<std::mutex> lk(mutex_);
    return controlSnapshots_;
  }
  // Waits up to 5 s for `done` to hold of the snapshots so far
  bool waitFor(const std::function<bool(const std::vector<RagdollSnapshot> &)> &done) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
      if (done(snapshots())) {
        return true;
      }
      std::this_thread::sleep_for(5ms);
    }
    return false;
  }

private:
  std::mutex mutex_;
  std::vector<RagdollSnapshot> snapshots_;
  std::vector<ControlSnapshot> controlSnapshots_;
};

struct RunningSystem {
  ScratchPadd::SystemImplementation<RagdollPhysicsPadd, ViewerPadd> system;
  std::thread mainThread;
  RunningSystem() {
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~RunningSystem() {
    system.stop();
    mainThread.join();
  }
  ViewerPadd &viewer() { return *std::get<std::unique_ptr<ViewerPadd>>(system.getWorkers()); }
  RagdollPhysicsPadd &physics() {
    return *std::get<std::unique_ptr<RagdollPhysicsPadd>>(system.getWorkers());
  }
};

// The newest snapshot's stats match `stepsPerSecond` and `subSteps`
auto statsAre(int stepsPerSecond, int subSteps) {
  return [=](const std::vector<RagdollSnapshot> &s) {
    return !s.empty() && s.back().stats.stepsPerSecond == stepsPerSecond &&
           s.back().stats.subSteps == subSteps;
  };
}

} // namespace

// Snapshots of the whole ragdoll arrive, its time keeping up with real time,
// at 60 steps a second in 2 sub-steps to start
TEST(RagdollPhysicsPaddTest, SendsSnapshotsInRealTime) {
  RunningSystem running;
  ViewerPadd &viewer = running.viewer();
  ASSERT_TRUE(viewer.waitFor([](const auto &s) { return !s.empty(); }));
  const auto started = std::chrono::steady_clock::now();
  const double startedAt = viewer.snapshots().back().time;
  std::this_thread::sleep_for(500ms);
  const auto snapshots = viewer.snapshots();
  const RagdollSnapshot &last = snapshots.back();
  const double real =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  EXPECT_NEAR(last.time - startedAt, real, 0.15);
  EXPECT_EQ(last.bones.size(), ragdoll::kBones);
  EXPECT_EQ(last.joints.size(), ragdoll::kJoints);
  EXPECT_EQ(last.stats.stepsPerSecond, 60);
  EXPECT_EQ(last.stats.subSteps, static_cast<int>(FixedSteps::kDefaultSubSteps));
  for (std::size_t i = 1; i < snapshots.size(); ++i) {
    EXPECT_GT(snapshots[i].time, snapshots[i - 1].time);
  }
}

// The panel's edits set the steps a second and sub-steps from the next step:
// at 240 steps a second, snapshots' times are whole numbers of 1/240 s apart
TEST(RagdollPhysicsPaddTest, ControlsSetStepsAndSubSteps) {
  RunningSystem running;
  ViewerPadd &viewer = running.viewer();
  EXPECT_TRUE(viewer.changeControl("Steps a second", Controls::Integer(240)));
  EXPECT_TRUE(viewer.changeControl("Sub-steps", Controls::Integer(4)));
  ASSERT_TRUE(viewer.waitFor(statsAre(240, 4)));
  const std::size_t from = viewer.snapshots().size();
  ASSERT_TRUE(viewer.waitFor([&](const auto &s) { return s.size() >= from + 10; }));
  const auto snapshots = viewer.snapshots();
  for (std::size_t i = from; i < snapshots.size(); ++i) {
    const double steps = (snapshots[i].time - snapshots[i - 1].time) * 240;
    EXPECT_NEAR(steps, std::round(steps), 1e-6);
    EXPECT_GE(std::round(steps), 1);
  }
}

// R resets the ragdoll: a new generation, sent at once, its stretch
// measured afresh
TEST(RagdollPhysicsPaddTest, ResetStartsANewGeneration) {
  RunningSystem running;
  ViewerPadd &viewer = running.viewer();
  ASSERT_TRUE(viewer.waitFor([](const auto &s) { return !s.empty(); }));
  const std::uint32_t before = viewer.snapshots().back().generation;
  viewer.command(RagdollCommand::Type::Reset);
  ASSERT_TRUE(viewer.waitFor([&](const auto &s) { return s.back().generation == before + 1; }));
  const auto snapshots = viewer.snapshots();
  const auto reset =
      std::find_if(snapshots.begin(), snapshots.end(),
                   [&](const RagdollSnapshot &s) { return s.generation == before + 1; });
  EXPECT_EQ(reset->stats.worstStretch, 0.0);
}

// P pauses: the Paused control turns on (panels are told), and the physics'
// time stops
TEST(RagdollPhysicsPaddTest, PauseStopsTime) {
  RunningSystem running;
  ViewerPadd &viewer = running.viewer();
  ASSERT_TRUE(viewer.waitFor([](const auto &s) { return !s.empty(); }));
  viewer.command(RagdollCommand::Type::TogglePause);
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  bool paused = false;
  while (!paused && std::chrono::steady_clock::now() < deadline) {
    for (const ControlSnapshot &snapshot : viewer.controlSnapshots()) {
      auto control = snapshot.controlMap.find("Paused");
      paused |= control != snapshot.controlMap.end() &&
                std::get<Controls::Boolean>(control->second).value;
    }
    std::this_thread::sleep_for(5ms);
  }
  ASSERT_TRUE(paused);
  const double time =
      ScratchPadd::onPaddThread(running.physics(), [](RagdollPhysicsPadd &p) { return p.time(); });
  std::this_thread::sleep_for(200ms);
  EXPECT_EQ(
      ScratchPadd::onPaddThread(running.physics(), [](RagdollPhysicsPadd &p) { return p.time(); }),
      time);
}
