// The simulation, event-log and sound padds, driven by a test padd that
// stands in for FlightDisplayPadd: it chains FrameInputs the same way and
// records the snapshots, events and control snapshots that come back. No
// window or sound device needed: a stand-in AudioPadd records what it's
// asked to play.
#include "EventLogPadd.hpp"
#include "FlightSimPadd.hpp"
#include "FlightSoundPadd.hpp"

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
  // The sound each event heard so far cues (cueFor)
  std::vector<std::string> cues() {
    std::lock_guard<std::mutex> lk(mutex_);
    return cues_;
  }

private:
  template <typename... Events> void recordEvents() {
    (receive<Events>([this](const Events &event) {
       std::lock_guard<std::mutex> lk(mutex_);
       eventLines_.push_back(logLine(event));
       cues_.push_back(cueFor(event).sound);
     }),
     ...);
  }

  std::atomic<int> framesDone_{0}, framesWanted_{0};
  std::mutex mutex_;
  WorldSnapshot last_;
  std::vector<std::string> eventLines_;
  std::vector<std::string> cues_;
  std::vector<ControlSnapshot> controlSnapshots_;
};

// Stands in for AudioPadd (named the same, so FlightSoundPadd finds it):
// records the sounds it's given and asked to play
class AudioStandIn : public ScratchPadd::Base {
public:
  AudioStandIn(ScratchPadd::System *system) : Base(system) {
    receive<ScratchPadd::Audio::LoadSoundSamples>(
        [this](const ScratchPadd::Audio::LoadSoundSamples &load) {
          std::lock_guard<std::mutex> lk(mutex_);
          loaded_.push_back(load.name);
        });
    receive<ScratchPadd::Audio::PlaySound>([this](const ScratchPadd::Audio::PlaySound &play) {
      std::lock_guard<std::mutex> lk(mutex_);
      played_.push_back(play.name);
    });
  }
  std::string name() override { return "AudioPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  std::vector<std::string> loaded() {
    std::lock_guard<std::mutex> lk(mutex_);
    return loaded_;
  }
  std::vector<std::string> played() {
    std::lock_guard<std::mutex> lk(mutex_);
    return played_;
  }

private:
  std::mutex mutex_;
  std::vector<std::string> loaded_, played_;
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

// Each event's cue: gates chime (gold higher), a wall zaps and a gate's
// frame clanks, FOCUS chimes up and PHASE down, phasing whooshes, and the
// game ends with a blast
TEST(FlightSoundTest, EachEventHasItsCue) {
  const SoundCue gate = cueFor(GatePassed{.gold = false});
  const SoundCue gold = cueFor(GatePassed{.gold = true});
  EXPECT_EQ(gate.sound, "chime");
  EXPECT_EQ(gold.sound, "chime");
  EXPECT_GT(gold.settings.pitch, gate.settings.pitch);
  EXPECT_EQ(cueFor(Crashed{.wall = true, .damage = 34}).sound, "zap");
  EXPECT_EQ(cueFor(Crashed{.wall = false, .damage = 20}).sound, "knock");
  EXPECT_GT(cueFor(Crashed{.wall = true, .damage = 34}).settings.volume,
            cueFor(Crashed{.wall = true, .damage = 5}).settings.volume);
  const SoundCue focus = cueFor(PowerupCollected{.kind = Kind::FocusPickup});
  const SoundCue phase = cueFor(PowerupCollected{.kind = Kind::PhasePickup});
  EXPECT_GT(focus.settings.pitch, 1.0f);
  EXPECT_LT(phase.settings.pitch, 1.0f);
  EXPECT_EQ(cueFor(WallPhased{}).sound, "whoosh");
  EXPECT_EQ(cueFor(GameOver{}).sound, "blast");
}

// Through the padds: FlightSoundPadd gives AudioPadd its sounds, then asks
// it to play each event's cue, in order, over the whole autopilot run
TEST(FlightSoundTest, AutopilotRunPlaysEachEventsCue) {
  RunningSystem<FlightSimPadd, DriverPadd, FlightSoundPadd, AudioStandIn> running(
      /*autopilot=*/true);
  auto &driver = running.padd<DriverPadd>();
  auto &audio = running.padd<AudioStandIn>();
  ASSERT_TRUE(driver.run(120 * 60));
  const std::vector<std::string> cues = driver.cues();
  ASSERT_FALSE(cues.empty());
  const auto deadline = std::chrono::steady_clock::now() + 5s;
  while (audio.played().size() < cues.size() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(1ms);
  }
  EXPECT_EQ(audio.played(), cues);
  EXPECT_EQ(audio.loaded(), (std::vector<std::string>{"chime", "zap", "knock", "whoosh", "blast"}));
}
