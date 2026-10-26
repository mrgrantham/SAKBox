// The simulation and event-log padds, driven by a test padd that stands in
// for FlightDisplayPadd: it chains FrameInputs the same way and records the
// snapshots, events and control snapshots that come back. No window needed.
#include "EventLogPadd.hpp"
#include "FlightSimPadd.hpp"

#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace PrecisionFlight;
using namespace std::chrono_literals;
namespace Controls = ScratchPadd::ControlType;
using ScratchPadd::MessageType::ControlChange;
using ScratchPadd::MessageType::ControlSnapshot;

namespace {

// Chains frames like the display: each WorldSnapshot answers one FrameInput
// and triggers the next, until `frames` have run.
class DriverPadd : public ScratchPadd::Base {
public:
  DriverPadd(ScratchPadd::System *system) : Base(system) {
    receive<WorldSnapshot>([this](const WorldSnapshot &world) {
      {
        std::lock_guard<std::mutex> lk(mutex_);
        last_ = world;
      }
      if (++framesDone_ < framesWanted_) {
        send(FrameInput{.seconds = kStepSeconds});
      }
    });
    receive<ControlSnapshot>([this](const ControlSnapshot &snapshot) {
      std::lock_guard<std::mutex> lk(mutex_);
      controlSnapshots_.push_back(snapshot);
    });
    recordEvents<GatePassed, Crashed, PowerupCollected, WallPhased, GameOver>();
  }
  std::string name() override { return "Driver"; }
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Runs `frames` frames of one simulation step each; returns once they're done.
  bool run(int frames) {
    framesWanted_ += frames;
    send(FrameInput{.seconds = kStepSeconds});
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (framesDone_ < framesWanted_) {
      if (std::chrono::steady_clock::now() > deadline)
        return false;
      std::this_thread::sleep_for(1ms);
    }
    return true;
  }

  // A key command, as the display sends it
  void command(GameCommand::Type type) { send(GameCommand{type}); }

  // An edit on the precision lab panel: sent straight to the simulation, as
  // the display's panel sends it
  bool changeControl(const std::string &control, const ScratchPadd::ControlTypeVariant &value) {
    return sendTo<FlightSimPadd>(ControlChange{.controlName = control, .controlTypeValue = value});
  }

  WorldSnapshot last() {
    std::lock_guard<std::mutex> lk(mutex_);
    return last_;
  }
  std::vector<std::string> eventLines() {
    std::lock_guard<std::mutex> lk(mutex_);
    return eventLines_;
  }
  std::vector<ControlSnapshot> controlSnapshots() {
    std::lock_guard<std::mutex> lk(mutex_);
    return controlSnapshots_;
  }

private:
  template <typename... Events> void recordEvents() {
    (receive<Events>([this](const Events &event) {
       std::lock_guard<std::mutex> lk(mutex_);
       eventLines_.push_back(logLine(event));
     }),
     ...);
  }

  std::atomic<int> framesDone_{0}, framesWanted_{0};
  std::mutex mutex_;
  WorldSnapshot last_;
  std::vector<std::string> eventLines_;
  std::vector<ControlSnapshot> controlSnapshots_;
};

// Captures EventLogPadd's output instead of printing it.
struct CapturedLog {
  std::mutex mutex;
  std::vector<std::string> lines;
  CapturedLog() {
    EventLogPadd::output = [this](const std::string &line) {
      std::lock_guard<std::mutex> lk(mutex);
      lines.push_back(line);
    };
  }
  ~CapturedLog() {
    EventLogPadd::output = [](const std::string &line) { std::printf("%s\n", line.c_str()); };
  }
  size_t size() {
    std::lock_guard<std::mutex> lk(mutex);
    return lines.size();
  }
};

template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread mainThread;

  explicit RunningSystem(bool autopilot = false) {
    FlightOptions::autopilot = autopilot; // read when FlightSimPadd is constructed
    system.instantiate();
    FlightOptions::autopilot = false;
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
};

std::vector<std::string> goldenEventLines() {
  std::ifstream file("projects/examples/precision_flight/tests/data/autopilot_events.txt");
  EXPECT_TRUE(file.good());
  std::vector<std::string> lines;
  for (std::string line; std::getline(file, line);) {
    if (line.rfind("END ", 0) != 0) {
      lines.push_back(line);
    }
  }
  return lines;
}

} // namespace

// Flown through the padds frame by frame, the autopilot run produces exactly
// the original game's events, at the driver and in the event log.
TEST(FlightPaddsTest, AutopilotRunMatchesOriginalGame) {
  CapturedLog log;
  RunningSystem<FlightSimPadd, DriverPadd, EventLogPadd> running(/*autopilot=*/true);
  auto &driver = running.padd<DriverPadd>();
  ASSERT_TRUE(driver.run(120 * 60));

  const auto golden = goldenEventLines();
  EXPECT_EQ(driver.eventLines(), golden);

  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (log.size() < golden.size() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(1ms);
  }
  std::lock_guard<std::mutex> lk(log.mutex);
  EXPECT_EQ(log.lines, golden);
}

// The precision lab is FlightSimPadd's controls: changing one changes the
// simulation from the next frame.
TEST(FlightPaddsTest, ControlChangeAppliesToSimulation) {
  RunningSystem<FlightSimPadd, DriverPadd> running;
  auto &driver = running.padd<DriverPadd>();
  EXPECT_TRUE(driver.changeControl("Flight bits", Controls::Integer(6)));
  EXPECT_TRUE(driver.changeControl("Rounding", Controls::String("stochastic")));
  ASSERT_TRUE(driver.run(2));

  EXPECT_EQ(driver.last().groups[0].fracBits, 6);
  // Read on the simulation's own thread
  EXPECT_EQ(
      ScratchPadd::onPaddThread(running.padd<FlightSimPadd>(),
                                [](FlightSimPadd &sim) { return sim.sim().settings.rounding; }),
      Flex::Math::Rounding::Stochastic);
}

// The T key's command flips the Autopilot control, and FlightSimPadd sends a
// fresh ControlSnapshot so panels show the change.
TEST(FlightPaddsTest, ToggleAutopilotUpdatesControl) {
  RunningSystem<FlightSimPadd, DriverPadd> running;
  auto &driver = running.padd<DriverPadd>();
  driver.command(GameCommand::Type::ToggleAutopilot);
  ASSERT_TRUE(driver.run(1));

  EXPECT_TRUE(driver.last().autopilot);
  const auto snapshots = driver.controlSnapshots();
  ASSERT_EQ(snapshots.size(), 1u);
  EXPECT_EQ(snapshots[0].paddName, "FlightSim");
  EXPECT_TRUE(std::get<Controls::Boolean>(snapshots[0].controlMap.at("Autopilot")).value);
}

// Pause stops the rail; restart begins the run again.
TEST(FlightPaddsTest, PauseAndRestartCommands) {
  RunningSystem<FlightSimPadd, DriverPadd> running;
  auto &driver = running.padd<DriverPadd>();
  ASSERT_TRUE(driver.run(30));
  const double before = driver.last().z;
  EXPECT_LT(before, 0.0);

  driver.command(GameCommand::Type::TogglePause);
  ASSERT_TRUE(driver.run(30));
  EXPECT_EQ(driver.last().z, before);
  EXPECT_TRUE(driver.last().paused);

  driver.command(GameCommand::Type::TogglePause);
  driver.command(GameCommand::Type::Restart);
  ASSERT_TRUE(driver.run(1));
  EXPECT_GT(driver.last().z, before); // back near the start
  EXPECT_EQ(driver.last().score, 0);
}
