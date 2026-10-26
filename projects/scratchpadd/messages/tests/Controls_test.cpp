#include <DataDepRetriever/DataDependencies.hpp>
#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <stdexcept>
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
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }

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
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }

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
  // Send a control change back to the generator, as the exposure padd's
  // panel would: straight to it
  using Access = ScratchPadd::PaddTestAccess;
  EXPECT_TRUE(
      Access::sendTo(*controlExpPadd, Access::addressOf<ControlGeneratorPadd>(*controlExpPadd),
                     ScratchPadd::MessageType::ControlChange{
                         .controlName = controlName, .controlTypeValue = updatedControlTypeValue}));

  controlGenPadd->waitForWorkCompletion();
  controlExpPadd->waitForWorkCompletion();

  // Check that the ControlsGeneratorPadd controls map has been updated with the new value
  // (read on its own thread, the only one that may touch its controls)

  auto genPaddControlsAfterChange =
      ScratchPadd::onPaddThread(*controlGenPadd, [](ControlGeneratorPadd &padd) {
        return Access::snapshot(padd);
      }).controlMap;
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

// Stands in for DisplayPadd: records every snapshot it receives, and who
// it came from. Its methods are called from the test's thread, and only
// send messages, which is safe from any thread.
template <int Id> class PanelPaddOf : public ScratchPadd::Base {
public:
  PanelPaddOf(ScratchPadd::System *system) : Base(system) {
    receive<ControlSnapshot>(
        [this](const ControlSnapshot &snapshot, ScratchPadd::PaddAddress from) {
          std::lock_guard<std::mutex> lk(mutex_);
          snapshots_.push_back(snapshot);
          senders_.push_back(from.name());
        });
  }
  std::string name() override { return Id == 1 ? "Panel" : "Panel" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  std::vector<ControlSnapshot> snapshots() {
    std::lock_guard<std::mutex> lk(mutex_);
    return snapshots_;
  }
  std::vector<std::string> senders() {
    std::lock_guard<std::mutex> lk(mutex_);
    return senders_;
  }
  // An edit on this panel: sent straight to the padd named `padd`, as
  // DisplayPadd sends it to the padd its snapshot came from
  bool change(const std::string &padd, const std::string &control,
              const ScratchPadd::ControlTypeVariant &value) {
    return sendTo(addressOf(padd),
                  ControlChange{.controlName = control, .controlTypeValue = value});
  }
  // Asks every padd with controls for them
  void requestAll() { send(ControlRequest{}); }
  // Asks just the padd named `padd`
  bool request(const std::string &padd) { return sendTo(addressOf(padd), ControlRequest{}); }

private:
  std::mutex mutex_;
  std::vector<ControlSnapshot> snapshots_;
  std::vector<std::string> senders_;
};
using PanelPadd = PanelPaddOf<1>;
using OtherPanelPadd = PanelPaddOf<2>;

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

// A running padd's control `name`, read on its own thread
template <typename C, typename Padd> C controlOf(Padd &padd, const std::string &name) {
  return ScratchPadd::onPaddThread(
      padd, [&](Padd &p) { return ScratchPadd::PaddTestAccess::control<C>(p, name); });
}

} // namespace

// A ControlRequest gets one snapshot from each padd that has controls; padds
// without controls stay silent.
TEST(ControlsTest, RequestGetsOneSnapshotPerPaddWithControls) {
  RunningSystem<PanelPadd, OwnerA, OwnerB, NoControlsPadd> running;
  running.padd<PanelPadd>().requestAll();
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(snapshotNames(running.padd<PanelPadd>().snapshots()),
            (std::vector<std::string>{"Owner1", "Owner2"}));
  EXPECT_EQ(running.padd<PanelPadd>().senders().size(), 2u); // each from its owner
}

// A ControlRequest sent to one padd is answered by that padd only.
TEST(ControlsTest, RequestCanBeSentToOnePadd) {
  RunningSystem<PanelPadd, OwnerA, OwnerB> running;
  EXPECT_TRUE(running.padd<PanelPadd>().request("Owner2"));
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(snapshotNames(running.padd<PanelPadd>().snapshots()),
            std::vector<std::string>{"Owner2"});
  EXPECT_EQ(running.padd<PanelPadd>().senders(), std::vector<std::string>{"Owner2"});
}

// The replies to a request go to the panel that asked, not to every panel.
TEST(ControlsTest, RepliesGoOnlyToThePanelThatAsked) {
  RunningSystem<PanelPadd, OtherPanelPadd, OwnerA, OwnerB> running;
  running.padd<PanelPadd>().requestAll();
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<PanelPadd>().snapshots().size(), 2u);
  EXPECT_TRUE(running.padd<OtherPanelPadd>().snapshots().empty());
}

// A change goes straight to its owner, whose new value every panel is told.
TEST(ControlsTest, EveryPanelSeesAChange) {
  RunningSystem<PanelPadd, OtherPanelPadd, OwnerA> running;
  EXPECT_TRUE(running.padd<PanelPadd>().change("Owner1", "speed", Controls::Double(3.0)));
  ASSERT_TRUE(running.settles());
  for (const auto &snapshots :
       {running.padd<PanelPadd>().snapshots(), running.padd<OtherPanelPadd>().snapshots()}) {
    ASSERT_EQ(snapshots.size(), 1u);
    EXPECT_DOUBLE_EQ(speedIn(snapshots[0]), 3.0);
  }
}

// A change to a padd that isn't there, or a request to one, isn't sent.
TEST(ControlsTest, ChangesToUnknownPaddsAreRefused) {
  RunningSystem<PanelPadd, OwnerA> running;
  EXPECT_FALSE(running.padd<PanelPadd>().change("Nobody", "speed", Controls::Double(3.0)));
  EXPECT_FALSE(running.padd<PanelPadd>().request("Nobody"));
  ASSERT_TRUE(running.settles());
  EXPECT_TRUE(running.padd<PanelPadd>().snapshots().empty());
}

// A change is applied only by the padd it names, which replies with exactly
// one snapshot holding the new value; other padds neither change nor reply.
TEST(ControlsTest, ChangeOnlyAffectsAndRepliesFromTarget) {
  RunningSystem<PanelPadd, OwnerA, OwnerB> running;
  running.padd<PanelPadd>().change("Owner1", "speed", Controls::Double(4.0));
  ASSERT_TRUE(running.settles());

  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(running.padd<OwnerA>(), "speed").value, 4.0);
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(running.padd<OwnerB>(), "speed").value, 1.0);

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
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(owner, "speed").value, 10.0);
  EXPECT_EQ(controlOf<Controls::Integer>(owner, "count").value, 2);
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(owner, "free").value, 1000.0);
  EXPECT_DOUBLE_EQ(speedIn(panel.snapshots().back()), 10.0);
}

// A change only carries a value: it can't widen the owner's range.
TEST(ControlsTest, ChangeCannotAlterRange) {
  RunningSystem<PanelPadd, OwnerA> running;
  running.padd<PanelPadd>().change("Owner1", "speed",
                                   Controls::Double(50.0, std::make_pair(0.0, 1000.0)));
  ASSERT_TRUE(running.settles());

  const auto speed = controlOf<Controls::Double>(running.padd<OwnerA>(), "speed");
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
  EXPECT_EQ(controlOf<Controls::String>(owner, "mode").value, "easy");
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(owner, "speed").value, 1.0);
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
  EXPECT_FALSE(controlOf<Controls::Boolean>(owner, "enabled").value);
  EXPECT_EQ(controlOf<Controls::String>(owner, "mode").value, "hard");
  EXPECT_EQ(controlOf<Controls::String>(owner, "label").value, std::string(500, 'x'));
  EXPECT_EQ(owner.changed(), (std::vector<std::string>{"enabled", "mode", "label"}));
}

// control<C>() reads current values and reports misuse. (Before the padd
// starts, any thread may.)
TEST(ControlsTest, ControlAccessor) {
  using Access = ScratchPadd::PaddTestAccess;
  ScratchPadd::SystemImplementation<OwnerA> system;
  system.instantiate();
  auto &owner = *std::get<std::unique_ptr<OwnerA>>(system.getWorkers());
  EXPECT_EQ(Access::control<Controls::Integer>(owner, "count").value, 5);
  EXPECT_THROW(Access::control<Controls::Boolean>(owner, "count"), std::bad_variant_access);
  EXPECT_THROW(Access::control<Controls::Integer>(owner, "missing"), std::out_of_range);
}

// While a padd runs, its controls can only be read or changed on its own
// thread; from another thread the call throws, naming the padd and method.
TEST(ControlsTest, ControlsOnlyOnThePaddsThread) {
  using Access = ScratchPadd::PaddTestAccess;
  RunningSystem<PanelPadd, OwnerA> running;
  auto &owner = running.padd<OwnerA>();
  EXPECT_THROW(Access::setControl(owner, "speed", Controls::Double(2.0)), std::logic_error);
  EXPECT_THROW(Access::control<Controls::Double>(owner, "speed"), std::logic_error);
  EXPECT_THROW(Access::addControl(owner, "new", Controls::Boolean(true)), std::logic_error);
  try {
    Access::removeControl(owner, "speed");
    ADD_FAILURE() << "removeControl from the test's thread didn't throw";
  } catch (const std::logic_error &error) {
    EXPECT_NE(std::string(error.what()).find("Owner1: removeControl()"), std::string::npos)
        << error.what();
  }
  // On its own thread, the same calls work
  EXPECT_TRUE(ScratchPadd::onPaddThread(
      owner, [](OwnerA &o) { return Access::setControl(o, "speed", Controls::Double(2.0)); }));
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(owner, "speed").value, 2.0);
}

// An owner can change its own controls while it runs (setControl,
// addControl, removeControl, on its own thread), and each change sends a
// snapshot, so panels stay current.
namespace {
// Runs `work` on `padd`'s own thread, then waits for the system to settle
template <typename Padd, typename... Padds>
void onPaddThreadThenSettle(RunningSystem<Padds...> &running,
                            const std::function<void(Padd &)> &work) {
  ScratchPadd::onPaddThread(running.template padd<Padd>(), work);
  ASSERT_TRUE(running.settles());
}

bool hasControl(const ControlSnapshot &snapshot, const std::string &name) {
  return snapshot.controlMap.contains(name);
}
} // namespace

// setControl applies the value, tells panels, and doesn't call
// controlChanged() (which is for edits from a panel)
TEST(ControlUpdateTest, SetControlTellsPanels) {
  RunningSystem<PanelPadd, OwnerA> running;
  std::atomic<bool> applied = false;
  onPaddThreadThenSettle<OwnerA>(running, [&](OwnerA &owner) {
    applied = ScratchPadd::PaddTestAccess::setControl(owner, "speed", Controls::Double(4.0));
  });
  EXPECT_TRUE(applied);
  const auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_DOUBLE_EQ(speedIn(snapshots[0]), 4.0);
  EXPECT_TRUE(running.padd<OwnerA>().changed().empty());
}

// setControl follows the same rules as a panel's edit: clamped to the
// range, and refused (with nothing sent) for the wrong type or an unknown
// name
TEST(ControlUpdateTest, SetControlFollowsTheRules) {
  RunningSystem<PanelPadd, OwnerA> running;
  std::atomic<bool> clamped = false, wrongType = true, unknown = true;
  onPaddThreadThenSettle<OwnerA>(running, [&](OwnerA &owner) {
    clamped = ScratchPadd::PaddTestAccess::setControl(owner, "speed", Controls::Double(50.0));
    wrongType = ScratchPadd::PaddTestAccess::setControl(owner, "speed", Controls::Integer(3));
    unknown = ScratchPadd::PaddTestAccess::setControl(owner, "missing", Controls::Double(1.0));
  });
  EXPECT_TRUE(clamped);
  EXPECT_FALSE(wrongType);
  EXPECT_FALSE(unknown);
  const auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 1u); // only the valid change sent one
  EXPECT_DOUBLE_EQ(speedIn(snapshots[0]), 10.0);
}

// addControl adds a control, or replaces one (range and all), and panels
// see it
TEST(ControlUpdateTest, AddControlShowsUp) {
  RunningSystem<PanelPadd, OwnerA> running;
  onPaddThreadThenSettle<OwnerA>(running, [](OwnerA &owner) {
    ScratchPadd::PaddTestAccess::addControl(owner, "volume",
                                            Controls::Integer(3, std::make_pair(0, 11)));
    ScratchPadd::PaddTestAccess::addControl(owner, "speed",
                                            Controls::Double(20.0, std::make_pair(0.0, 100.0)));
  });
  auto &owner = running.padd<OwnerA>();
  EXPECT_EQ(controlOf<Controls::Integer>(owner, "volume").value, 3);
  const auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 2u);
  EXPECT_TRUE(hasControl(snapshots[0], "volume"));
  EXPECT_DOUBLE_EQ(speedIn(snapshots[1]), 20.0);

  // The replacement's wider range is the one that applies now
  running.padd<PanelPadd>().change("Owner1", "speed", Controls::Double(80.0));
  ASSERT_TRUE(running.settles());
  EXPECT_DOUBLE_EQ(controlOf<Controls::Double>(owner, "speed").value, 80.0);
}

// removeControl removes a control; removing the last one sends an empty
// snapshot, which takes the padd's panel away. Unknown names change nothing.
TEST(ControlUpdateTest, RemoveControlUpdatesPanels) {
  RunningSystem<PanelPadd, OwnerA> running;
  std::atomic<bool> removedSpeed = false, removedMissing = true;
  onPaddThreadThenSettle<OwnerA>(running, [&](OwnerA &owner) {
    removedSpeed = ScratchPadd::PaddTestAccess::removeControl(owner, "speed");
    removedMissing = ScratchPadd::PaddTestAccess::removeControl(owner, "missing");
  });
  EXPECT_TRUE(removedSpeed);
  EXPECT_FALSE(removedMissing);
  auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_FALSE(hasControl(snapshots[0], "speed"));
  EXPECT_TRUE(hasControl(snapshots[0], "count"));

  onPaddThreadThenSettle<OwnerA>(running, [](OwnerA &owner) {
    for (const char *name : {"free", "count", "enabled", "mode", "label"})
      ScratchPadd::PaddTestAccess::removeControl(owner, name);
  });
  snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 6u);
  EXPECT_TRUE(snapshots.back().controlMap.empty());

  // With no controls left, it no longer answers requests
  running.padd<PanelPadd>().requestAll();
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<PanelPadd>().snapshots().size(), 6u);
}

// A padd that started without controls gets a panel when it adds one
TEST(ControlUpdateTest, PaddWithoutControlsCanAddOne) {
  RunningSystem<PanelPadd, NoControlsPadd> running;
  running.padd<PanelPadd>().requestAll();
  ASSERT_TRUE(running.settles());
  EXPECT_TRUE(running.padd<PanelPadd>().snapshots().empty());

  onPaddThreadThenSettle<NoControlsPadd>(running, [](NoControlsPadd &padd) {
    ScratchPadd::PaddTestAccess::addControl(padd, "on", Controls::Boolean(true));
  });
  const auto snapshots = running.padd<PanelPadd>().snapshots();
  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_EQ(snapshots[0].paddName, "NoControls");
  EXPECT_TRUE(hasControl(snapshots[0], "on"));
}
