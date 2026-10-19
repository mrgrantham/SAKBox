#include <DataDepRetriever/DataDependencies.hpp>
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static auto logger = spdlog::stdout_color_mt("test");

class ControlGeneratorPadd : public ScratchPadd::Base {
public:
  ControlGeneratorPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }
  std::string name() override { return __CLASS_NAME__; }
  bool shouldUseMainThread() override { return true; }

  ScratchPadd::ControlValueMap generateControls() override {
    logger().info("Generating controls for {}", name());
    return {
        {"aDouble",
         ScratchPadd::ControlType::Double(20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aString", ScratchPadd::ControlType::String(
                        "DefaultString", std::make_optional(std::vector<std::string>(
                                             {"DefaultString", "AnotherString", "YetAnother"})))},
        {"aBool", ScratchPadd::ControlType::Boolean(true)},
        {"anInt", ScratchPadd::ControlType::Integer(5, std::make_optional(std::make_pair(2, 9)))},
    };
  }
};

class ControlExposurePadd : public ScratchPadd::Base {

private:
  std::unordered_map<std::string, ScratchPadd::ControlValueMap> paddControls_;

public:
  std::unordered_map<std::string, ScratchPadd::ControlValueMap> &getAllPaddControlMaps() {
    return paddControls_;
  }

  std::string name() override { return __CLASS_NAME__; }
  ControlExposurePadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
    receive<ScratchPadd::MessageType::ControlSnapshot>(
        [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot) {
          setupControlView(snapshot);
        });
  }
  bool shouldUseMainThread() override { return true; }

  void starting() override { send(ScratchPadd::MessageType::ControlRequest{}); }

  void setupControlView(const ScratchPadd::MessageType::ControlSnapshot &controlSnapshot) {
    logger().info("Setting up control view from type {}", TypeName(controlSnapshot));
    if (paddControls_.contains(controlSnapshot.paddName)) {
      logger().info("Control map already added. Updating...");
    }
    logger().info("Adding control for {} with size {}", controlSnapshot.paddName,
                  controlSnapshot.controlMap.size());
    paddControls_[controlSnapshot.paddName] = controlSnapshot.controlMap;
  }

  ScratchPadd::ControlValueMap generateControls() override {
    logger().info("No controls required for the ControlExposurePadd");
    return {};
  }
};

// On startup ControlExposurePadd sends a ControlRequest; ControlGeneratorPadd
// answers with a ControlSnapshot holding all four of its controls.
TEST(ControlsTest, TestControlRequest) {

  using namespace std::chrono_literals;

  auto *spsystem =
      dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd> *>(
          ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

  // ScratchPadd::Data::ConfigureDependencies(argv[0]);
  SCOPED_METHOD_TIMER();
  // signal(SIGINT, signal_handler);
  logger->info("Welcome to SCRATCHPADD!");
  spsystem->instantiate();

  auto systemThread = std::thread([&] { spsystem->start(); });
  spsystem->waitForStart();

  logger->info("Getting workers");
  auto &workers = spsystem->getWorkers();

  auto &controlGenPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);
  auto &controlExpPadd = std::get<std::unique_ptr<ControlExposurePadd>>(workers);

  // Startup Expectation:
  // ControlExposurePadd sends a ControlRequest to ControlGeneratorPadd
  // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd

  spsystem->waitForWorkCompletion();

  auto &allPaddControlMaps = controlExpPadd->getAllPaddControlMaps();
  EXPECT_EQ(allPaddControlMaps.size(), 1);
  for (auto &[key, value] : allPaddControlMaps) {
    logger->info("Padd controls key: {}", key);
  }
  EXPECT_TRUE(allPaddControlMaps.contains(controlGenPadd->name()));
  auto &genPaddControls = allPaddControlMaps[controlGenPadd->name()];
  EXPECT_EQ(genPaddControls.size(), 4);

  spsystem->stop();
  systemThread.join();
}

// A ControlChange sent to ControlGeneratorPadd updates its value, and the
// snapshot it sends back brings ControlExposurePadd's copy into sync.
TEST(ControlsTest, TestControlChange) {
  using namespace std::chrono_literals;

  auto *spsystem =
      dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd> *>(
          ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

  // ScratchPadd::Data::ConfigureDependencies(argv[0]);
  SCOPED_METHOD_TIMER();
  // signal(SIGINT, signal_handler);
  logger->info("Welcome to SCRATCHPADD!");
  spsystem->instantiate();

  auto systemThread = std::thread([&] { spsystem->start(); });
  spsystem->waitForStart(); // Waits untill all workers are about to enter there work loop

  logger->info("Getting workers");
  auto &workers = spsystem->getWorkers();

  auto &controlGenPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);
  auto &controlExpPadd = std::get<std::unique_ptr<ControlExposurePadd>>(workers);

  // Startup Expectation:
  // ControlExposurePadd sends a ControlRequest to ControlGeneratorPadd
  // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd

  controlGenPadd->waitForWorkCompletion();
  controlExpPadd->waitForWorkCompletion();

  // Modification Expectation:
  // ControlExposurePadd send a ControlChange to ControlGeneratorPadd
  // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd

  // Check value before change
  auto &allPaddControlMapsBeforeChange = controlExpPadd->getAllPaddControlMaps();
  EXPECT_EQ(allPaddControlMapsBeforeChange.size(), 1);
  EXPECT_TRUE(allPaddControlMapsBeforeChange.contains(controlGenPadd->name()));
  auto &genPaddControls = allPaddControlMapsBeforeChange[controlGenPadd->name()];
  EXPECT_EQ(genPaddControls.size(), 4);
  std::string controlName = "aDouble";
  EXPECT_TRUE(genPaddControls.contains(controlName));
  ScratchPadd::ControlTypeVariant &controlTypeValue = genPaddControls[controlName];
  EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(controlTypeValue));

  // confirm original value set in ControlsGeneratorPadd generateControls() method
  EXPECT_DOUBLE_EQ(std::get<ScratchPadd::ControlType::Double>(controlTypeValue).value, 20.0);

  // update the value
  const double updatedValue = 40.1;
  auto updatedControlTypeValue = std::get<ScratchPadd::ControlType::Double>(
      controlTypeValue); // make a copy. value held in ControlExposurePadd will be unchanged until
                         // the update is round tripped back from ControlGeneratorPadd
  updatedControlTypeValue.value = updatedValue;
  // Send a control change back to the generator
  controlExpPadd->send(
      ScratchPadd::MessageType::ControlChange{.paddName = controlGenPadd->name(),
                                              .controlName = controlName,
                                              .controlTypeValue = updatedControlTypeValue});

  controlGenPadd->waitForWorkCompletion();
  controlExpPadd->waitForWorkCompletion();

  // Check that the ControlsGeneratorPadd controls map has been updated with the new value

  auto genPaddControlsAfterChange = controlGenPadd->generateControlsSnapshot().controlMap;
  EXPECT_EQ(genPaddControlsAfterChange.size(), 4);
  EXPECT_TRUE(genPaddControlsAfterChange.contains(controlName));
  EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(
      genPaddControlsAfterChange[controlName]));

  EXPECT_DOUBLE_EQ(
      std::get<ScratchPadd::ControlType::Double>(genPaddControlsAfterChange[controlName]).value,
      updatedValue);

  // Check that that update has been published back to the ControlExposurePadd
  // This is to ensure that the two controls are in sync.

  auto &allPaddControlMapsAfterChange = controlExpPadd->getAllPaddControlMaps();

  EXPECT_TRUE(allPaddControlMapsAfterChange.contains(controlGenPadd->name()));
  auto &controlMap = allPaddControlMapsAfterChange[controlGenPadd->name()];

  EXPECT_TRUE(controlMap.contains(controlName));
  EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(controlMap[controlName]));

  EXPECT_DOUBLE_EQ(std::get<ScratchPadd::ControlType::Double>(controlMap[controlName]).value,
                   updatedValue);

  spsystem->stop();
  systemThread.join();
}

namespace {

using namespace std::chrono_literals;
namespace Controls = ScratchPadd::ControlType;
using ScratchPadd::MessageType::ControlChange;
using ScratchPadd::MessageType::ControlRequest;
using ScratchPadd::MessageType::ControlSnapshot;

// An owner with one control of each kind. Records the controls it was told
// changed.
template <int Id> class OwnerPadd : public ScratchPadd::Base {
public:
  OwnerPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Owner" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override {
    return {
        {"speed", Controls::Double(1.0, std::make_pair(0.0, 10.0))},
        {"free", Controls::Double(0.5)},
        {"count", Controls::Integer(5, std::make_pair(2, 9))},
        {"enabled", Controls::Boolean(true)},
        {"mode", Controls::String("easy", std::vector<std::string>{"easy", "hard"})},
        {"label", Controls::String("hello")},
    };
  }
  void controlChanged(const std::string &controlName) override {
    std::lock_guard<std::mutex> lk(mutex_);
    changed_.push_back(controlName);
  }
  std::vector<std::string> changed() {
    std::lock_guard<std::mutex> lk(mutex_);
    return changed_;
  }

private:
  std::mutex mutex_;
  std::vector<std::string> changed_;
};
using OwnerA = OwnerPadd<1>;
using OwnerB = OwnerPadd<2>;

class NoControlsPadd : public ScratchPadd::Base {
public:
  NoControlsPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "NoControls"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};

// Stands in for DisplayPadd: records every snapshot it receives.
class PanelPadd : public ScratchPadd::Base {
public:
  PanelPadd(ScratchPadd::System *system) : Base(system) {
    receive<ControlSnapshot>([this](const ControlSnapshot &snapshot) {
      std::lock_guard<std::mutex> lk(mutex_);
      snapshots_.push_back(snapshot);
    });
  }
  std::string name() override { return "Panel"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  std::vector<ControlSnapshot> snapshots() {
    std::lock_guard<std::mutex> lk(mutex_);
    return snapshots_;
  }
  void change(const std::string &padd, const std::string &control,
              const ScratchPadd::ControlTypeVariant &value) {
    send(ControlChange{.paddName = padd, .controlName = control, .controlTypeValue = value});
  }

private:
  std::mutex mutex_;
  std::vector<ControlSnapshot> snapshots_;
};

template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
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
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
  // True once every padd has worked through its queue (replies included).
  bool settles() {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    int quietChecks = 0;
    while (quietChecks < 5) { // replies are queued by the work just finished
      quietChecks = (padd<Padds>().pendingWorkCount() + ...) == 0 ? quietChecks + 1 : 0;
      if (std::chrono::steady_clock::now() > deadline)
        return false;
      std::this_thread::sleep_for(1ms);
    }
    return true;
  }
};

std::vector<std::string> snapshotNames(const std::vector<ControlSnapshot> &snapshots) {
  std::vector<std::string> names;
  names.reserve(snapshots.size());
  for (const auto &snapshot : snapshots)
    names.push_back(snapshot.paddName);
  std::sort(names.begin(), names.end());
  return names;
}

double speedIn(const ControlSnapshot &snapshot) {
  return std::get<Controls::Double>(snapshot.controlMap.at("speed")).value;
}

} // namespace

// A ControlRequest gets one snapshot from each padd that has controls; padds
// without controls stay silent.
TEST(ControlsTest, RequestGetsOneSnapshotPerPaddWithControls) {
  RunningSystem<PanelPadd, OwnerA, OwnerB, NoControlsPadd> running;
  running.padd<PanelPadd>().send(ControlRequest{});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(snapshotNames(running.padd<PanelPadd>().snapshots()),
            (std::vector<std::string>{"Owner1", "Owner2"}));
}

// A ControlRequest naming a padd is answered by that padd only.
TEST(ControlsTest, RequestCanNameOnePadd) {
  RunningSystem<PanelPadd, OwnerA, OwnerB> running;
  running.padd<PanelPadd>().send(ControlRequest{.paddName = "Owner2"});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(snapshotNames(running.padd<PanelPadd>().snapshots()),
            std::vector<std::string>{"Owner2"});
}

// A change is applied only by the padd it names, which replies with exactly
// one snapshot holding the new value; other padds neither change nor reply.
TEST(ControlsTest, ChangeOnlyAffectsAndRepliesFromTarget) {
  RunningSystem<PanelPadd, OwnerA, OwnerB> running;
  running.padd<PanelPadd>().change("Owner1", "speed", Controls::Double(4.0));
  ASSERT_TRUE(running.settles());

  EXPECT_DOUBLE_EQ(running.padd<OwnerA>().control<Controls::Double>("speed").value, 4.0);
  EXPECT_DOUBLE_EQ(running.padd<OwnerB>().control<Controls::Double>("speed").value, 1.0);

  const auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_EQ(snapshots[0].paddName, "Owner1");
  EXPECT_DOUBLE_EQ(speedIn(snapshots[0]), 4.0);

  EXPECT_EQ(running.padd<OwnerA>().changed(), std::vector<std::string>{"speed"});
  EXPECT_TRUE(running.padd<OwnerB>().changed().empty());
}

// Numbers are clamped to the owner's range; unranged numbers are not, and the
// reply shows the clamped value.
TEST(ControlsTest, NumbersAreClampedToTheirRange) {
  RunningSystem<PanelPadd, OwnerA> running;
  auto &panel = running.padd<PanelPadd>();
  panel.change("Owner1", "speed", Controls::Double(50.0));
  panel.change("Owner1", "count", Controls::Integer(-3));
  panel.change("Owner1", "free", Controls::Double(1000.0));
  ASSERT_TRUE(running.settles());

  auto &owner = running.padd<OwnerA>();
  EXPECT_DOUBLE_EQ(owner.control<Controls::Double>("speed").value, 10.0);
  EXPECT_EQ(owner.control<Controls::Integer>("count").value, 2);
  EXPECT_DOUBLE_EQ(owner.control<Controls::Double>("free").value, 1000.0);
  EXPECT_DOUBLE_EQ(speedIn(panel.snapshots().back()), 10.0);
}

// A change only carries a value: it can't widen the owner's range.
TEST(ControlsTest, ChangeCannotAlterRange) {
  RunningSystem<PanelPadd, OwnerA> running;
  running.padd<PanelPadd>().change("Owner1", "speed",
                                   Controls::Double(50.0, std::make_pair(0.0, 1000.0)));
  ASSERT_TRUE(running.settles());

  const auto &speed = running.padd<OwnerA>().control<Controls::Double>("speed");
  EXPECT_DOUBLE_EQ(speed.value, 10.0);
  EXPECT_EQ(speed.enforcableRange, std::make_optional(std::make_pair(0.0, 10.0)));
}

// Invalid changes are rejected and controlChanged() isn't called, but the
// owner still replies so a panel can show the real value again.
TEST(ControlsTest, InvalidChangesAreRejected) {
  RunningSystem<PanelPadd, OwnerA> running;
  auto &panel = running.padd<PanelPadd>();
  panel.change("Owner1", "mode", Controls::String("impossible"));  // not allowed
  panel.change("Owner1", "speed", Controls::Boolean(false));       // wrong type
  panel.change("Owner1", "missing", Controls::Boolean(false));     // unknown control
  panel.change("Owner1", "speed", Controls::Double(std::nan(""))); // not a number
  ASSERT_TRUE(running.settles());

  auto &owner = running.padd<OwnerA>();
  EXPECT_EQ(owner.control<Controls::String>("mode").value, "easy");
  EXPECT_DOUBLE_EQ(owner.control<Controls::Double>("speed").value, 1.0);
  EXPECT_TRUE(owner.changed().empty());
  EXPECT_EQ(panel.snapshots().size(), 4u);
}

// Valid changes of every kind apply and each calls controlChanged() once.
TEST(ControlsTest, ValidChangesOfEveryKindApply) {
  RunningSystem<PanelPadd, OwnerA> running;
  auto &panel = running.padd<PanelPadd>();
  panel.change("Owner1", "enabled", Controls::Boolean(false));
  panel.change("Owner1", "mode", Controls::String("hard"));
  panel.change("Owner1", "label", Controls::String(std::string(500, 'x'))); // long text
  ASSERT_TRUE(running.settles());

  auto &owner = running.padd<OwnerA>();
  EXPECT_FALSE(owner.control<Controls::Boolean>("enabled").value);
  EXPECT_EQ(owner.control<Controls::String>("mode").value, "hard");
  EXPECT_EQ(owner.control<Controls::String>("label").value, std::string(500, 'x'));
  EXPECT_EQ(owner.changed(), (std::vector<std::string>{"enabled", "mode", "label"}));
}

// control<C>() reads current values and reports misuse.
TEST(ControlsTest, ControlAccessor) {
  ScratchPadd::SystemImplementation<OwnerA> system;
  system.instantiate();
  auto &owner = *std::get<std::unique_ptr<OwnerA>>(system.getWorkers());
  EXPECT_EQ(owner.control<Controls::Integer>("count").value, 5);
  EXPECT_THROW(owner.control<Controls::Boolean>("count"), std::bad_variant_access);
  EXPECT_THROW(owner.control<Controls::Integer>("missing"), std::out_of_range);
}
