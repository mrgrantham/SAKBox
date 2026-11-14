// Cyclone's ragdoll on a Nintendo DS or 3DS, in padds: one app, built for
// each (../ds, ../3ds), the few differences between them in #if blocks.
//
//   ScreenPadd  (main thread) draws the ragdoll on the top screen with
//               ScratchPadd's scene layer (its DS backend, on the DS's 3D
//               hardware; or its 3DS backend, with citro3d, in stereo while
//               the 3D slider is up), from RagdollPadd's snapshots, and the
//               physics' figures on the bottom screen; sends the buttons
//   RagdollPadd (own thread)  steps the ragdoll (ragdoll_physics.h, as in
//               Cyclone's demo) in fixed steps and sub-steps (FixedSteps),
//               timing each step, and sends AudioPadd a knock for each hit
//               (ImpactKnocks, as the demos choose them)
//   AudioPadd   (own thread)  plays the knocks: on the DS's hardware sound
//               channels, on the 3DS through its DSP (AudioPadd.hpp picks)
//
// Cyclone runs in Q16.16 on the DS (no floating-point hardware), and in
// float on the 3DS, as the scene layer's Scalar does on each.
//
// A: reset (a new shove)   X: 1, 2, 4 or 8 sub-steps   Y: 10, 15, 20, 30 or 60
// steps a second   B: blend drawing between steps   R: the sweep (every
// setting in turn, from the same shove, logged and ranked: physics_sweep.h)
// SELECT: every padd's controls, as tabs, in place of the figures
// (HandheldControls)   START: quit
#if defined(__NDS__)
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <nds.h>
#else
#include <3ds.h>
#include <ScratchPadd/Scene/3DS/Display.hpp>
#endif

#include "physics_sweep.h" // (beside this file)
#include <AudioPadd.hpp>
#include <ScratchPadd/AboutPadd.hpp>
#include <ScratchPadd/AppEnded/AppEnded.hpp>
#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/DebugLog/Session.hpp>
#include <ScratchPadd/HandheldControls/ConsolePanel.hpp>
#include <ScratchPadd/Input/GamePadd.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#if defined(__3DS__)
#include <ScratchPadd/Network/NetworkPadd.hpp>
#include <ScratchPadd/Network/RemoteControlsPadd.hpp>
#endif
#include <ScratchPadd/Storage/StoragePadd.hpp>
#include <cyclone/cyclone.h>
#include <fixed_steps.h>
#include <impact_knocks.h>
#include <ragdoll_physics.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Scene = ScratchPadd::Scene;
namespace Input = ScratchPadd::Input;

// Where a debug build's log goes (main sets it), shown under the figures
std::string logWhere;
using Clock = std::chrono::steady_clock;

// Built as the sweep's app (cyclone-ds-ragdoll-sweep, ...-3ds-...: its BUILD
// defines RAGDOLL_SWEEP_AT_START), the sweep starts by itself, so it can run
// unattended, in an emulator or on a console
#if defined(RAGDOLL_SWEEP_AT_START)
constexpr bool kSweepAtStart = true;
#else
constexpr bool kSweepAtStart = false;
#endif

// What differs between the two consoles
#if defined(__NDS__)
constexpr const char *kConsole = "DS";
constexpr int kConsoleColumns = 32, kConsoleRows = 24; // the bottom screen's
bool mainLoop() { return pmMainLoop(); }
void startBottomScreen() { consoleDemoInit(); } // its console, for printf
void waitForRefresh() { swiWaitForVBlank(); }
// Drawn every other refresh: the physics runs 30 steps a second, and drawing
// at 60 took the ARM9 time it needs (a step at the start, 57 ms against 47)
constexpr int kDefaultFramesPerSecond = 30;
#else
constexpr const char *kConsole = "3DS";
constexpr int kConsoleColumns = 40, kConsoleRows = 30;
bool mainLoop() { return aptMainLoop(); }
void startBottomScreen() { consoleInit(GFX_BOTTOM, nullptr); }
void waitForRefresh() { gspWaitForVBlank(); }
constexpr int kDefaultFramesPerSecond = 60;
#endif

struct ButtonPressed {
  Input::Button button; // by where it is: FaceRight is Nintendo's A
};

// Each bone's place after a tick, ready to draw: its body's transform,
// scaled to the bone's size (the bones are one box mesh, 2 m a side).
// Cyclone's number type is the scene layer's on each console (Q16.16 on the
// DS, float on the 3DS), so they go straight in.
struct BonesSnapshot {
  std::array<Scene::Transform, ragdoll::kBones> bones{};
  // The step length (µs), for drawing between this snapshot and the last;
  // and whether the ragdoll was reset since the last (so it isn't drawn
  // sliding over from before)
  std::int32_t stepMicroseconds = 0;
  bool restarted = false;
};

// How long a step took with each of a few settings, measured when the app
// starts (each from a fresh ragdoll, over its first steps)
struct Benchmark {
  struct Row {
    int stepsPerSecond = 0;
    int subSteps = 0;
    int velocitySweeps = 0, settleSweeps = 0, positionSweeps = 0;
    double stepMilliseconds = 0;
  };
  std::array<Row, 5> rows{};
  // Where the time goes in a step at the settings it runs with (all its
  // sub-steps; see cyclone::World::Profile)
  double integrateMilliseconds = 0, contactsMilliseconds = 0, resolveMilliseconds = 0;
};

// How many frames the screen has drawn, published by ScreenPadd each frame
// (shared state), for the sweep's frame rate
struct FramesDrawn {
  std::uint32_t count = 0;
};

// The sweep, as it goes (RagdollPadd to ScreenPadd): the setting running,
// and once it's done, the best few
struct SweepProgress {
  bool running = false;
  unsigned index = 0, count = 0;
  physics_sweep::Setting setting;
};
struct SweepResults {
  static constexpr std::size_t kShown = 5;
  std::array<physics_sweep::Result, kShown> best{};
  unsigned shown = 0, runs = 0;
};

// What the physics measured over the last second
struct PhysicsFigures {
  int stepsPerSecond = 0;
  int subSteps = 0;
  double stepMilliseconds = 0; // a whole step, all its sub-steps
  double busy = 0;             // share of the second spent stepping
  double speed = 0;            // simulated seconds a second (under 1: falling behind)
  double worstStretch = 0;     // the most a joint was beyond its slack since reset (m)
  unsigned knocks = 0;         // knocks sent to AudioPadd so far
  bool audio = false;          // whether there's an AudioPadd to send them to
};

class RagdollPadd : public ScratchPadd::Base {
public:
  RagdollPadd(ScratchPadd::System *system)
      : Base(system), ragdoll_(std::make_unique<ragdoll::Ragdoll>()) {
    receive<ButtonPressed>([this](const ButtonPressed &pressed) {
      if (pressed.button == Input::Button::FaceRight) { // A
        reset();
      } else if (pressed.button == Input::Button::FaceTop) { // X
        setSubSteps(clock_.subSteps() >= 8 ? 1 : clock_.subSteps() * 2);
      } else if (pressed.button == Input::Button::FaceLeft) { // Y
        // Fewer steps a second is less work for each second simulated, with
        // coarser steps (joints stretch more on impact, unless sub-steps
        // make up for it)
        setStepsPerSecond(nextStepsPerSecond(stepsPerSecond()));
      } else if (pressed.button == Input::Button::R) {
        sweep_.running() ? endSweep() : startSweep();
      }
    });
    reset();
  }
  std::string name() override { return "RagdollPadd"; }
  // The settings X and Y change, as controls too (shown with SELECT)
  ScratchPadd::ControlValueMap generateControls() override {
    return {{"Sub-steps", ScratchPadd::ControlType::Integer(
                              static_cast<std::int32_t>(kRunSettings.subSteps), {{1, 8}})},
            {"Steps a second", ScratchPadd::ControlType::String(
                                   std::to_string(kRunSettings.stepsPerSecond), stepsChoices())}};
  }
  void controlChanged(const std::string &name, const ScratchPadd::ControlTypeVariant &) override {
    if (name == "Sub-steps") {
      clock_.setSubSteps(
          static_cast<unsigned>(control<ScratchPadd::ControlType::Integer>(name).value));
    } else if (name == "Steps a second") {
      clock_.setStepSeconds(1.0 / std::stoi(control<ScratchPadd::ControlType::String>(name).value));
    }
  }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::duration<double>(FixedSteps::kDefaultStepSeconds);
  }

  // Finds AudioPadd and gives it the knock: made in code, rendered when the
  // app is built (16-bit samples: no float work here)
  void prepare() override {
    audio_ = addressOf("AudioPadd");
    if (audio_) {
      namespace Pcm16 = ScratchPadd::Audio::MadeSoundsPcm16;
      sendTo(audio_,
             ScratchPadd::Audio::LoadSoundPcm16{
                 .name = "knock",
                 .samples = std::make_shared<const std::vector<std::int16_t>>(Pcm16::knock()),
                 .rate = Pcm16::kRate,
                 .channels = 1});
    }
  }

  // Times a few settings first (a few seconds), then runs with the cheap one,
  // hearing its hits (not while timing, so the figures are the physics')
  void starting() override {
    const Benchmark measured = benchmark();
    logBenchmark(measured);
    send(measured);
    useSettings(kRunSettings);
    reset();
    // The camera looks from (3, 2.2) across the ground: 36 degrees around
    // from +x (see ImpactKnocks::setCameraAngle)
    knocks_.setCameraAngle(36);
    ragdoll_->world.contactsFound = [this](std::span<const cyclone::Contact> contacts,
                                           cyclone::real_duration seconds) {
      knocks_.hear(contacts, static_cast<float>(seconds));
    };
    lastTick_ = secondStart_ = Clock::now();
    if (kSweepAtStart) {
      startSweep();
    }
  }

  // Works out the steps due, and queues the first. Each step is a job of
  // its own (stepDue), queued behind whatever came meanwhile, so a button
  // waits for at most one step, not all those due (three, when it's behind,
  // and a step lying on the ground in 8 sub-steps took 140 ms on a DS Lite)
  void repeat() override {
    const auto now = Clock::now();
    const unsigned steps = clock_.stepsFor(std::chrono::duration<double>(now - lastTick_).count());
    lastTick_ = now;
    // No more due than one call can give (FixedSteps::kLongestFrame), so a
    // ragdoll that can't keep up slows down instead of falling ever further
    // behind
    const auto mostDue =
        static_cast<unsigned>(std::ceil(FixedSteps::kLongestFrame / clock_.stepSeconds()));
    stepsDue_ = std::min(stepsDue_ + steps, std::max(mostDue, 1u));
    queueStepIfDue();
    reportEachSecond();
  }

private:
  unsigned stepsDue_ = 0;
  bool stepQueued_ = false;
  void queueStepIfDue() {
    if (stepsDue_ > 0 && !stepQueued_) {
      stepQueued_ = true;
      push([this] { stepDue(); });
    }
  }
  void stepDue() {
    stepQueued_ = false;
    if (stepsDue_ == 0) {
      return;
    }
    --stepsDue_;
    const auto started = Clock::now();
    step();
    stepping_ += Clock::now() - started;
    ++stepsTaken_;
    send(snapshot());
    playKnocks();
    queueStepIfDue();
  }
  void reportEachSecond() {
    const auto second = Clock::now() - secondStart_;
    if (second >= std::chrono::seconds(1)) {
      const double seconds = std::chrono::duration<double>(second).count();
      const PhysicsFigures figures{
          .stepsPerSecond = static_cast<int>(std::lround(1 / clock_.stepSeconds())),
          .subSteps = static_cast<int>(clock_.subSteps()),
          .stepMilliseconds =
              stepsTaken_
                  ? std::chrono::duration<double, std::milli>(stepping_).count() / stepsTaken_
                  : 0,
          .busy = std::chrono::duration<double>(stepping_).count() / seconds,
          .speed = stepsTaken_ * clock_.stepSeconds() / seconds,
          .worstStretch = worstStretch_,
          .knocks = knocksSent_,
          .audio = static_cast<bool>(audio_)};
      logFigures(figures);
      send(figures);
      if (sweep_.running()) {
        addSweepSecond(seconds);
      }
      secondStart_ = Clock::now();
      stepping_ = {};
      stepsTaken_ = 0;
    }
  }

  // Sub-steps, and the resolver's velocity, settle and position sweeps a
  // step (shared among its sub-steps; see ContactResolver::setSweeps)
  struct Settings {
    unsigned stepsPerSecond, subSteps, velocitySweeps, settleSweeps, positionSweeps;
  };
  // On the DS it runs 15 steps a second in 4 sub-steps, drawn blended
  // between them (ScreenPadd) at 30 frames a second: of the sweep's 40
  // settings (R; physics_sweep.h), the only one a DS Lite kept up with while
  // the ragdoll lay awake with its joints held (a step 52 ms, 78% busy; 30
  // steps in 4 fell to 0.48x). It's drawn 100 ms behind (a step and a
  // frame). Cyclone's own sweeps (40, 10, 10 a step) suit desktops; on the
  // DS they're what the time goes on, so it takes 16, 4, 4 (4, 1, 1 in each
  // sub-step; Box2D v3 takes one). The 3DS has floating-point hardware: 60
  // steps a second.
#if defined(__NDS__)
  static constexpr Settings kRunSettings{15, 4, 16, 4, 4};
#else
  static constexpr Settings kRunSettings{60, 4, 16, 4, 4};
#endif
  static constexpr Settings kMeasured[] = {{60, 1, 40, 10, 10},
                                           {60, 2, 16, 4, 4},
                                           {60, 4, 16, 4, 4},
                                           {30, 2, 16, 4, 4},
                                           {30, 4, 16, 4, 4}};
  static constexpr int kStepsMeasured = 5;

  void useSettings(const Settings &settings) {
    clock_.setStepSeconds(1.0 / settings.stepsPerSecond);
    clock_.setSubSteps(settings.subSteps);
    ragdoll_->world.getResolver().setSweeps(settings.velocitySweeps, settings.settleSweeps,
                                            settings.positionSweeps);
  }

  // Logged (a debug build's log: on the SD card on a DS, over Wi-Fi on a
  // 3DS), so a run's figures can be read afterwards, not just watched
  void logBenchmark(const Benchmark &measured) {
    for (const Benchmark::Row &row : measured.rows) {
      if (row.subSteps > 0) {
        logger().info(
            "Measured at start: {} steps/s, {} sub-steps, sweeps {}/{}/{}: {:.1f} ms a step",
            row.stepsPerSecond, row.subSteps, row.velocitySweeps, row.settleSweeps,
            row.positionSweeps, row.stepMilliseconds);
      }
    }
    logger().info("A step as it runs: move {:.1f} ms, find {:.1f} ms, solve {:.1f} ms",
                  measured.integrateMilliseconds, measured.contactsMilliseconds,
                  measured.resolveMilliseconds);
  }
  void logFigures(const PhysicsFigures &figures) {
    logger().info("{} steps/s, {} sub-steps: a step {:.1f} ms, {:.0f}% busy, speed {:.2f}x, "
                  "worst stretch {:.2f} cm, {} knocks",
                  figures.stepsPerSecond, figures.subSteps, figures.stepMilliseconds,
                  100 * figures.busy, figures.speed, 100 * figures.worstStretch, figures.knocks);
  }

  Benchmark benchmark() {
    Benchmark result;
    for (std::size_t i = 0; i < std::size(kMeasured); ++i) {
      const Settings &settings = kMeasured[i];
      useSettings(settings);
      reset();
      const auto started = Clock::now();
      for (int s = 0; s < kStepsMeasured; ++s) {
        step();
      }
      result.rows[i] = {
          .stepsPerSecond = static_cast<int>(settings.stepsPerSecond),
          .subSteps = static_cast<int>(settings.subSteps),
          .velocitySweeps = static_cast<int>(settings.velocitySweeps),
          .settleSweeps = static_cast<int>(settings.settleSweeps),
          .positionSweeps = static_cast<int>(settings.positionSweeps),
          .stepMilliseconds =
              std::chrono::duration<double, std::milli>(Clock::now() - started).count() /
              kStepsMeasured};
    }
    // Each phase of a step, at the settings it runs with
    useSettings(kRunSettings);
    reset();
    constexpr int kStepsProfiled = 10;
    for (int i = 0; i < kStepsProfiled; ++i) {
      step();
      const cyclone::World::Profile &profile = ragdoll_->world.profile();
      result.integrateMilliseconds += profile.integrate / kStepsProfiled;
      result.contactsMilliseconds += profile.findContacts / kStepsProfiled;
      result.resolveMilliseconds += profile.resolve / kStepsProfiled;
    }
    return result;
  }

  // One step, in its sub-steps, as Cyclone's demos step
  void step() {
    ragdoll_->world.setSubSteps(clock_.subSteps());
    ragdoll_->step(clock_.stepSeconds());
    worstStretch_ = std::max(worstStretch_, ragdoll_->worstStretch());
  }

  // Asks AudioPadd to play this tick's knocks
  void playKnocks() {
    knocks_.takeKnocks(knocksToPlay_);
    if (!audio_) {
      return;
    }
    for (const Knock &knock : knocksToPlay_) {
      ++knocksSent_;
      sendTo(audio_,
             ScratchPadd::Audio::PlaySound{
                 .name = "knock",
                 .settings = {.volume = knock.volume, .pan = knock.pan, .pitch = knock.pitch}});
    }
  }

  BonesSnapshot snapshot() {
    BonesSnapshot s;
    s.stepMicroseconds = static_cast<std::int32_t>(std::lround(clock_.stepSeconds() * 1e6));
    s.restarted = std::exchange(restarted_, false);
    for (unsigned b = 0; b < ragdoll::kBones; ++b) {
      const ragdoll::Bone &bone = ragdoll_->bones[b];
      const cyclone::Matrix4 body = bone.body->getTransform();
      Scene::Transform place;
      for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
          place.m[row][column] = body.data[row * 4 + column];
      s.bones[b] =
          place * Scene::Transform::scaling({bone.halfSize.x, bone.halfSize.y, bone.halfSize.z});
    }
    return s;
  }

  // The steps a second Y goes through (the sweep's), and the control's
  // choices
  static std::vector<std::string> stepsChoices() {
    std::vector<std::string> choices;
    for (const int steps : physics_sweep::kStepsPerSecond)
      choices.push_back(std::to_string(steps));
    return choices;
  }
  static int nextStepsPerSecond(int steps) {
    const auto &all = physics_sweep::kStepsPerSecond;
    const auto *next = std::upper_bound(std::begin(all), std::end(all), steps);
    return next == std::end(all) ? all[0] : *next;
  }
  int stepsPerSecond() const { return static_cast<int>(std::lround(1 / clock_.stepSeconds())); }
  // (each sets its control too, so the panel shows what's in use)
  void setStepsPerSecond(int steps) {
    clock_.setStepSeconds(1.0 / steps);
    setControl("Steps a second", ScratchPadd::ControlType::String(std::to_string(steps)));
  }
  void setSubSteps(unsigned subSteps) {
    clock_.setSubSteps(subSteps);
    setControl("Sub-steps", ScratchPadd::ControlType::Integer(static_cast<std::int32_t>(subSteps)));
  }
  void setFramesPerSecond(int frames) {
    if (const ScratchPadd::PaddAddress screen = addressOf("ScreenPadd")) {
      sendTo(screen,
             ScratchPadd::MessageType::ControlChange{
                 .controlName = "Frames a second",
                 .controlTypeValue = ScratchPadd::ControlType::String(std::to_string(frames))});
    }
  }

  // --- The sweep (R) ---
  // Each setting from the same shove, so they're compared on the same fall
  static constexpr unsigned kSweepSeed = 2026;
  physics_sweep::Sweep sweep_;
  std::uint32_t framesAtSecondStart_ = 0;

  void startSweep() {
    logger().info(
        "Sweep: {} settings, each from the same shove, for {:.0f} s simulated (or {:.0f} s "
        "if it can't keep up)",
        sweep_.count(), physics_sweep::kRunSimulatedSeconds, physics_sweep::kRunRealSeconds);
    sweep_.start();
    beginSweepRun();
  }
  // A run from the start: its settings, the same shove, the figures' second
  // begun again
  void beginSweepRun() {
    const physics_sweep::Setting &setting = sweep_.current();
    setStepsPerSecond(setting.stepsPerSecond);
    setSubSteps(static_cast<unsigned>(setting.subSteps));
    setFramesPerSecond(setting.framesPerSecond);
    cyclone::Random sameShove(kSweepSeed);
    ragdoll_->reset(sameShove);
    worstStretch_ = 0;
    restarted_ = true;
    stepsDue_ = 0;
    lastTick_ = secondStart_ = Clock::now();
    stepping_ = {};
    stepsTaken_ = 0;
    framesAtSecondStart_ = currentState<FramesDrawn>().value.count;
    send(SweepProgress{.running = true,
                       .index = static_cast<unsigned>(sweep_.index()),
                       .count = static_cast<unsigned>(sweep_.count()),
                       .setting = setting});
  }
  void addSweepSecond(double seconds) {
    const std::uint32_t frames = currentState<FramesDrawn>().value.count;
    const bool runEnded =
        sweep_.addSecond(seconds, stepsTaken_ * clock_.stepSeconds(),
                         std::chrono::duration<double>(stepping_).count(), stepsTaken_,
                         static_cast<int>(frames - framesAtSecondStart_), worstStretch_);
    framesAtSecondStart_ = frames;
    if (!runEnded) {
      return;
    }
    logSweepResult(sweep_.results().size(), sweep_.count(), sweep_.results().back());
    if (sweep_.running()) {
      beginSweepRun();
    } else {
      endSweep();
    }
  }
  // Stopped (R) or done: the results ranked, logged and shown, and the
  // usual settings back
  void endSweep() {
    sweep_.stop();
    const std::vector<physics_sweep::Result> ranked = physics_sweep::ranked(sweep_.results());
    logger().info("Sweep {}: {} of {} settings run, best first:",
                  sweep_.finished() ? "done" : "stopped", ranked.size(), sweep_.count());
    SweepResults shown{.runs = static_cast<unsigned>(ranked.size())};
    for (std::size_t i = 0; i < ranked.size(); ++i) {
      logSweepResult(i + 1, ranked.size(), ranked[i]);
      if (i < SweepResults::kShown) {
        shown.best[i] = ranked[i];
        shown.shown = static_cast<unsigned>(i + 1);
      }
    }
    send(SweepProgress{.running = false});
    send(shown);
    useSettings(kRunSettings);
    setStepsPerSecond(static_cast<int>(kRunSettings.stepsPerSecond));
    setSubSteps(kRunSettings.subSteps);
    setFramesPerSecond(kDefaultFramesPerSecond);
    reset();
  }
  void logSweepResult(std::size_t number, std::size_t of, const physics_sweep::Result &result) {
    const char *verdict = !physics_sweep::keepsUp(result)          ? "doesn't keep up"
                          : !physics_sweep::holdsItsJoints(result) ? "joints stretch"
                                                                   : "good";
    logger().info("Sweep {:2}/{}: {:2} steps/s, {} sub-steps, {} frames/s: a step {:.1f} ms, "
                  "{:.0f}% busy, speed {:.2f}x (slowest second {:.2f}x), {:.0f} frames/s, worst "
                  "stretch {:.2f} cm, {:.0f} ms behind: {}",
                  number, of, result.setting.stepsPerSecond, result.setting.subSteps,
                  result.setting.framesPerSecond, result.stepMilliseconds(), 100 * result.busy(),
                  result.speed(), result.slowestSecond, result.framesPerSecond(),
                  100 * result.worstStretch, result.delayMilliseconds(), verdict);
  }

  void reset() {
    ragdoll_->reset(random_);
    worstStretch_ = 0;
    restarted_ = true;
  }

  std::unique_ptr<ragdoll::Ragdoll> ragdoll_;
  ImpactKnocks knocks_;
  std::vector<Knock> knocksToPlay_; // reused each tick
  ScratchPadd::PaddAddress audio_;
  unsigned knocksSent_ = 0;
  cyclone::Random random_;
  FixedSteps clock_;
  double worstStretch_ = 0;
  bool restarted_ = false; // reset since the last snapshot
  Clock::time_point lastTick_, secondStart_;
  Clock::duration stepping_{};
  int stepsTaken_ = 0;
};

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system)
      : Base(system),
        controls_(
            [this](ScratchPadd::PaddAddress owner,
                   const ScratchPadd::MessageType::ControlChange &change) {
              sendTo(owner, change);
            },
            ScratchPadd::HandheldControls::usableColumns(kConsoleColumns), kConsoleRows) {
    // Every padd's controls (this one's too), for the panel SELECT shows
    receive<ScratchPadd::MessageType::ControlSnapshot>(
        [this](const ScratchPadd::MessageType::ControlSnapshot &snapshot,
               ScratchPadd::PaddAddress owner) { controls_.show(snapshot, owner); });
    // Each snapshot is drawn blended in from the last over a step, so the
    // bones move smoothly at any step rate (a step behind)
    receive<BonesSnapshot>([this](const BonesSnapshot &bones) {
      previous_ = bones.restarted ? bones : latest_;
      latest_ = bones;
      latestArrived_ = Clock::now();
    });
    // The figures are written when they change (about once a second), not
    // each frame: formatting numbers is float work, done in software on the
    // DS, and the main thread's time comes out of the physics'
    receive<PhysicsFigures>([this](const PhysicsFigures &figures) {
      figures_ = figures;
      drawFigures();
    });
    receive<Benchmark>([this](const Benchmark &benchmark) {
      benchmark_ = benchmark;
      drawFigures();
    });
    receive<SweepProgress>([this](const SweepProgress &progress) {
      sweepProgress_ = progress;
      drawFigures();
    });
    receive<SweepResults>([this](const SweepResults &results) {
      sweepResults_ = results;
      drawFigures();
    });
  }
  std::string name() override { return "ScreenPadd"; }
  // The frame loop, on the main thread as in devkitPro's examples
  ScratchPadd::Runs runs() override { return ScratchPadd::Runs::OnMainThread; }
  // Whether bones are drawn blended between steps (B, or the panel)
  ScratchPadd::ControlValueMap generateControls() override {
    return {{"Blend", ScratchPadd::ControlType::Boolean(true)},
            {"Frames a second", ScratchPadd::ControlType::String(
                                    std::to_string(kDefaultFramesPerSecond), {{"30", "60"}})}};
  }

  void starting() override {
    display_.emplace(); // the top screen
    startBottomScreen();
    renderer_.emplace(*display_);
    // A box 2 m a side, scaled to each bone, and the ground: a sand square,
    // 16 m across
    box_ = renderer_->load(Scene::MeshBuilder().box({}, {1, 1, 1}).data());
    ground_ = renderer_->load(
        Scene::MeshBuilder()
            .quad({-8, 0, 8}, {8, 0, 8}, {8, 0, -8}, {-8, 0, -8}, Scene::Colour{200, 190, 160})
            .data());
    controls_.setVisible(false); // the figures first; SELECT for the controls
    sendIncludeSender(ScratchPadd::MessageType::ControlRequest{});
    push([this] { frame(); });
  }

private:
  Input::Gamepad lastPad_; // last frame's, for what's pressed since
  void frame() {
    if (!mainLoop()) {
      system_->stop();
      return;
    }
    // The pad as GamePadd last read it, and what was pressed since the
    // last frame
    const Input::Gamepad pad = currentState<Input::Gamepad>().value;
    const Input::Edges pressed(lastPad_, pad);
    const Input::Gamepad before = std::exchange(lastPad_, pad);
    if (pressed.pressed(Input::Button::Start)) {
      system_->stop();
      return;
    }
    // While the controls are shown, the buttons are theirs
    const bool wasShown = controls_.visible();
    controls_.handle(ScratchPadd::HandheldControls::panelInputFrom(before, pad));
    if (controls_.visible() != wasShown) {
      std::printf("\x1b[2J"); // the screen cleared for the other
      if (!controls_.visible())
        drawFigures();
    }
    ScratchPadd::HandheldControls::draw(controls_);
    if (!wasShown && !controls_.visible()) {
      if (pressed.pressed(Input::Button::FaceBottom)) { // B
        setControl("Blend", ScratchPadd::ControlType::Boolean(!blend()));
        drawFigures();
      }
      for (const Input::Button button : {Input::Button::FaceRight, Input::Button::FaceTop,
                                         Input::Button::FaceLeft, Input::Button::R}) {
        if (pressed.pressed(button)) {
          send(ButtonPressed{button});
        }
      }
    }
    // At 30 frames a second, every other refresh is let go by, its time
    // left to the physics (the main thread outranks the physics' thread,
    // so each frame drawn comes out of the physics' time)
    if (framesPerSecond() == 30) {
      waitForRefresh();
    }
    drawRagdoll();
    publishState(FramesDrawn{++framesDrawn_});
    push([this] { frame(); });
  }

  // The camera looks from (12, 4.8, 8.8) across the ground, 36 degrees
  // around from +x (see ImpactKnocks::setCameraAngle), lit from above
  void drawRagdoll() {
    static const Scene::Camera kCamera{.eye = {12, 4.8f, 8.8f},
                                       .target = {0, 1.2f, 0},
                                       .up = {0, 1, 0},
                                       .fieldOfView = 50,
                                       .nearPlane = 0.4f,
                                       .farPlane = 160};
    static const Scene::Environment kEnvironment = [] {
      Scene::Environment environment;
      environment.background = {224, 232, 248}; // pale sky
      environment.ambient = {110, 105, 110};
      environment.addLight({.direction = {-0.5f, -1, -0.3f}, .colour = {190, 185, 175}});
      return environment;
    }();
    renderer_->beginFrame(kCamera, kEnvironment);
    renderer_->draw(ground_, Scene::Transform::identity(), {});
    // How far into the step after the latest snapshot the time is (0 to 1):
    // the bones are drawn that far from the last snapshot to the latest
    Scene::Scalar fraction = 1;
    if (blend() && latest_.stepMicroseconds > 0) {
      const auto since =
          std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - latestArrived_);
#if defined(__NDS__)
      // In Q16.16 directly: integer arithmetic, no float work on the DS
      fraction = Scene::Scalar::fromFixed(static_cast<std::int32_t>(
          std::min<long long>(since.count() * 65536 / latest_.stepMicroseconds, 65536)));
#else
      fraction = std::min(
          static_cast<float>(since.count()) / static_cast<float>(latest_.stepMicroseconds), 1.0f);
#endif
    }
    const Scene::Material bone{.colour = {235, 160, 150}};
    for (unsigned b = 0; b < ragdoll::kBones; ++b) {
      // Each number moved that far from the last place to the latest: a
      // blend of two turns isn't quite a turn, but a step apart it's close
      const Scene::Transform &from = previous_.bones[b], &to = latest_.bones[b];
      Scene::Transform place;
      for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
          place.m[row][column] =
              from.m[row][column] + (to.m[row][column] - from.m[row][column]) * fraction;
      renderer_->draw(box_, place, bone);
    }
    renderer_->endFrame(); // waits for the screen's refresh
  }

  void drawFigures() {
    if (controls_.visible()) {
      return; // the panel has the screen; they're drawn when it's hidden
    }
    // (rows from 0)
    constexpr int kColumns = ScratchPadd::HandheldControls::usableColumns(kConsoleColumns);
    using ScratchPadd::HandheldControls::printLine;
    printLine(1, kColumns, "Cyclone's ragdoll on a %s", kConsole);
    if (renderer_) {
      const Scene::FrameCounts drawn = renderer_->lastFrame();
      const Scene::FrameTimes times = renderer_->lastFrameTimes();
      if (times.gpu > 0) // (the 3DS measures its frames)
        printLine(2, kColumns, "3D: %u triangles, GPU %.1f ms", drawn.polygons, times.gpu);
      else
        printLine(2, kColumns, "3D: %u polygons, %u vertices", drawn.polygons, drawn.vertices);
    }
    printLine(3, kColumns, "Physics: %d steps a second,", figures_.stepsPerSecond);
    printLine(4, kColumns, "  %d sub-step%s each, %s", figures_.subSteps,
              figures_.subSteps == 1 ? "" : "s", blend() ? "blended" : "unblended");
    printLine(5, kColumns, "Drawn %d frames a second", framesPerSecond());
    // (While running, a step's time includes the main thread's, which takes
    // over each frame to draw)
    printLine(6, kColumns, "A step takes %.0f ms (running)", figures_.stepMilliseconds);
    printLine(7, kColumns, "Physics thread busy: %.0f%%", 100 * figures_.busy);
    printLine(8, kColumns, "Simulation speed: %.2fx", figures_.speed);
    printLine(9, kColumns, "Worst joint stretch: %.2f cm", 100 * figures_.worstStretch);
    if (figures_.audio) {
      printLine(10, kColumns, "Knocks played: %u", figures_.knocks);
    } else {
      printLine(10, kColumns, "No AudioPadd: no sound");
    }
    printLine(11, kColumns, "A: reset X: sub-steps B: blend");
    printLine(12, kColumns, "Y: steps R: sweep SELECT: panel");
    printLine(13, kColumns, "START: quit");
    // Rows 14 to 20: the sweep as it runs, its best once it's done, or the
    // timings at the start
    if (sweepProgress_.running) {
      printLine(14, kColumns, "Sweep %u of %u (R stops it):", sweepProgress_.index + 1,
                sweepProgress_.count);
      printLine(15, kColumns, "  %d steps/s, %d sub, %d fps", sweepProgress_.setting.stepsPerSecond,
                sweepProgress_.setting.subSteps, sweepProgress_.setting.framesPerSecond);
      for (int row = 16; row <= 20; ++row)
        printLine(row, kColumns, "");
    } else if (sweepResults_.runs > 0) {
      printLine(14, kColumns, "Sweep: best of %u (all: log)", sweepResults_.runs);
      printLine(15, kColumns, "st/s sub fps  ms   slow   cm");
      for (unsigned i = 0; i < SweepResults::kShown; ++i) {
        const physics_sweep::Result &r = sweepResults_.best[i];
        if (i < sweepResults_.shown)
          printLine(16 + static_cast<int>(i), kColumns, "%-4d %-3d %-3d %4.0f %5.2f %4.1f",
                    r.setting.stepsPerSecond, r.setting.subSteps, r.setting.framesPerSecond,
                    r.stepMilliseconds(), r.slowestSecond, 100 * r.worstStretch);
        else
          printLine(16 + static_cast<int>(i), kColumns, "");
      }
    } else {
      printLine(14, kColumns, "Measured at start (ms a step):");
      printLine(15, kColumns, "steps/s sub sweeps        ms");
      for (std::size_t i = 0; i < benchmark_.rows.size(); ++i) {
        const Benchmark::Row &r = benchmark_.rows[i];
        if (r.subSteps > 0) {
          printLine(16 + static_cast<int>(i), kColumns, "%-7d %-3d %2d/%2d/%2d  %7.1f",
                    r.stepsPerSecond, r.subSteps, r.velocitySweeps, r.settleSweeps,
                    r.positionSweeps, r.stepMilliseconds);
        }
      }
    }
    int row = 21;
    printLine(row, kColumns, "A step as it runs, ms:"); // (row 23 is the last)
    printLine(++row, kColumns, "move %.1f find %.1f solve %.1f", benchmark_.integrateMilliseconds,
              benchmark_.contactsMilliseconds, benchmark_.resolveMilliseconds);
    if (!logWhere.empty())
      printLine(++row, kColumns, "Log: %s", logWhere.c_str());
  }

  // The last two snapshots, drawn between (see drawRagdoll), and when the
  // latest came
  BonesSnapshot previous_, latest_;
  Clock::time_point latestArrived_;
  // The top screen and what draws on it (made in starting(), on the main
  // thread), and the meshes
  std::optional<Scene::Display> display_;
  std::optional<Scene::Renderer> renderer_;
  Scene::Mesh box_, ground_;
  bool blend() const { return control<ScratchPadd::ControlType::Boolean>("Blend").value; }
  int framesPerSecond() const {
    return control<ScratchPadd::ControlType::String>("Frames a second").value == "30" ? 30 : 60;
  }
  ScratchPadd::HandheldControls::ControlTabs controls_;
  PhysicsFigures figures_;
  Benchmark benchmark_;
  SweepProgress sweepProgress_;
  SweepResults sweepResults_;
  std::uint32_t framesDrawn_ = 0;
};

} // namespace

// The 3DS's networking: its log over Wi-Fi (NetworkPadd) and its controls
// from the Mac (RemoteControlsPadd, for the remote controls app). A DS
// Lite can't join a WPA2 network: none on the DS.
#if defined(__3DS__)
#define NETWORK_PADDS , ScratchPadd::Network::NetworkPadd, ScratchPadd::Network::RemoteControlsPadd
#else
#define NETWORK_PADDS
#endif

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
#if defined(__3DS__)
  gfxInitDefault(); // (before ScreenPadd's Display)
#endif
#if defined(SAKBOX_DEBUG)
  // A debug build's log: over Wi-Fi on a 3DS, on the SD card on a DS
  ScratchPadd::DebugLog::Session log; // (its file named after the app: AppInfo)
  logWhere = log.where();
#endif
  {
    ScratchPadd::SystemImplementation<ScratchPadd::AboutPadd, ScratchPadd::Input::GamePadd,
                                      ScreenPadd, RagdollPadd, AudioPadd,
                                      ScratchPadd::Storage::StoragePadd NETWORK_PADDS>
        system; // (StoragePadd: a debug build's log, written to the card when idle)
    system.instantiate();
    system.start(); // returns when ScreenPadd stops it (START)
  }
  ScratchPadd::AppEnded::show(); // ("<its title> has ended.")
#if defined(__3DS__)
  gfxExit();
#endif
  return 0;
}
