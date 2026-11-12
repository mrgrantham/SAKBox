// Cyclone's ragdoll on a Nintendo DS, in padds.
//
//   ScreenPadd  (main thread) draws the ragdoll on the top screen with
//               ScratchPadd's scene layer (its DS backend: the DS's 3D
//               hardware), from RagdollPadd's snapshots, and the physics'
//               figures on the bottom screen; sends the buttons
//   RagdollPadd (own thread)  steps the ragdoll (ragdoll_physics.h, as in
//               Cyclone's demo) in fixed steps and sub-steps (FixedSteps),
//               timing each step, and sends AudioPadd a knock for each hit
//               (ImpactKnocks, as the demos choose them)
//   AudioPadd   (own thread)  plays the knocks on the DS's hardware sound
//               channels (AudioPadd/HardwareChannelsAudioPadd.hpp)
//
// A: reset (a new shove)   X: 1, 2 or 4 sub-steps   Y: 30 or 60 steps a second
// B: blend drawing between steps   SELECT: every padd's controls, as tabs, in
// place of the figures (HandheldControls)   START: quit
#include <nds.h>

#include <AudioPadd.hpp>
#include <ScratchPadd/Audio/MadeSoundsPcm16.hpp>
#include <ScratchPadd/Audio/Messages.hpp>
#include <ScratchPadd/HandheldControls/ConsolePanel.hpp>
#include <ScratchPadd/Scene/DS/Display.hpp>
#include <ScratchPadd/Scene/Scene.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <cyclone/cyclone.h>
#include <fixed_steps.h>
#include <impact_knocks.h>
#include <ragdoll_physics.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

namespace Scene = ScratchPadd::Scene;
using Clock = std::chrono::steady_clock;

struct ButtonPressed {
  int key = 0; // libnds' KEY_*
};

// Each bone's place after a tick, ready to draw: its body's transform,
// scaled to the bone's size (the bones are one box mesh, 2 m a side). Cyclone
// runs in Q16.16 here, as the scene layer does on the DS, so they go
// straight in.
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
      if (pressed.key == KEY_A) {
        reset();
      } else if (pressed.key == KEY_X) {
        clock_.setSubSteps(clock_.subSteps() == 4 ? 1 : clock_.subSteps() * 2);
        setControl("Sub-steps",
                   ScratchPadd::ControlType::Integer(static_cast<std::int32_t>(clock_.subSteps())));
      } else if (pressed.key == KEY_Y) {
        // Half as many steps a second is half the work for each second
        // simulated, with coarser steps (joints stretch more on impact)
        setStepsPerSecond(clock_.stepSeconds() == kSlowStepSeconds ? 60 : 30);
        setControl("Steps a second", ScratchPadd::ControlType::String(
                                         clock_.stepSeconds() == kSlowStepSeconds ? "30" : "60"));
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
                                   std::to_string(kRunSettings.stepsPerSecond), {{"30", "60"}})}};
  }
  void controlChanged(const std::string &name, const ScratchPadd::ControlTypeVariant &) override {
    if (name == "Sub-steps") {
      clock_.setSubSteps(
          static_cast<unsigned>(control<ScratchPadd::ControlType::Integer>(name).value));
    } else if (name == "Steps a second") {
      setStepsPerSecond(control<ScratchPadd::ControlType::String>(name).value == "30" ? 30 : 60);
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
    send(benchmark());
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
  }

  void repeat() override {
    const auto now = Clock::now();
    const unsigned steps = clock_.stepsFor(std::chrono::duration<double>(now - lastTick_).count());
    lastTick_ = now;
    for (unsigned i = 0; i < steps; ++i) {
      const auto started = Clock::now();
      step();
      stepping_ += Clock::now() - started;
      ++stepsTaken_;
    }
    if (steps > 0) {
      send(snapshot());
    }
    playKnocks();
    const auto second = Clock::now() - secondStart_;
    if (second >= std::chrono::seconds(1)) {
      const double seconds = std::chrono::duration<double>(second).count();
      send(PhysicsFigures{.stepsPerSecond = static_cast<int>(std::lround(1 / clock_.stepSeconds())),
                          .subSteps = static_cast<int>(clock_.subSteps()),
                          .stepMilliseconds =
                              stepsTaken_
                                  ? std::chrono::duration<double, std::milli>(stepping_).count() /
                                        stepsTaken_
                                  : 0,
                          .busy = std::chrono::duration<double>(stepping_).count() / seconds,
                          .speed = stepsTaken_ * clock_.stepSeconds() / seconds,
                          .worstStretch = worstStretch_,
                          .knocks = knocksSent_,
                          .audio = static_cast<bool>(audio_)});
      secondStart_ = Clock::now();
      stepping_ = {};
      stepsTaken_ = 0;
    }
  }

private:
  // Sub-steps, and the resolver's velocity, settle and position sweeps a
  // step (shared among its sub-steps; see ContactResolver::setSweeps)
  struct Settings {
    unsigned stepsPerSecond, subSteps, velocitySweeps, settleSweeps, positionSweeps;
  };
  // It runs 30 steps a second, drawn blended between them (ScreenPadd), in
  // 4 sub-steps: at 60 a falling step (15 ms in 2 sub-steps) and the
  // drawing don't fit in a frame, while at 30 a step in 4 takes 21 ms of
  // its 33, and joints hold as well as at 60 in 2 (they stretch 0.6 cm at
  // most over 20 pushes; 5 cm in 2 sub-steps). Cyclone's own sweeps (40,
  // 10, 10 a step) suit desktops; on the DS they're what the time goes on,
  // so it takes 16, 4, 4 (4, 1, 1 in each sub-step; Box2D v3 takes one)
  static constexpr Settings kRunSettings{30, 4, 16, 4, 4};
  // 30 steps a second; Y switches between it and FixedSteps' 60
  static constexpr double kSlowStepSeconds = 1.0 / 30;
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

  void setStepsPerSecond(int steps) {
    clock_.setStepSeconds(steps == 30 ? kSlowStepSeconds : FixedSteps::kDefaultStepSeconds);
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
      : Base(system), controls_(
                          [this](ScratchPadd::PaddAddress owner,
                                 const ScratchPadd::MessageType::ControlChange &change) {
                            sendTo(owner, change);
                          },
                          ScratchPadd::HandheldControls::usableColumns(32), 24) {
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
  }
  std::string name() override { return "ScreenPadd"; }
  // The frame loop, on the main thread as in devkitPro's examples
  ScratchPadd::RunOn runOn() override { return ScratchPadd::RunOn::MainThread; }
  // Whether bones are drawn blended between steps (B, or the panel)
  ScratchPadd::ControlValueMap generateControls() override {
    return {{"Blend", ScratchPadd::ControlType::Boolean(true)}};
  }

  void starting() override {
    display_.emplace(); // the top screen
    consoleDemoInit();  // the bottom screen's console, for printf
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
  void frame() {
    if (!pmMainLoop()) {
      system_->stop();
      return;
    }
    scanKeys();
    const int down = keysDown();
    if (down & KEY_START) {
      system_->stop();
      return;
    }
    // While the controls are shown, the buttons are theirs
    const bool wasShown = controls_.visible();
    controls_.handle(ScratchPadd::HandheldControls::inputFrom(
        static_cast<std::uint32_t>(down), static_cast<std::uint32_t>(keysHeld())));
    if (controls_.visible() != wasShown) {
      std::printf("\x1b[2J"); // the screen cleared for the other
      if (!controls_.visible())
        drawFigures();
    }
    ScratchPadd::HandheldControls::draw(controls_);
    if (!wasShown && !controls_.visible()) {
      if (down & KEY_B) {
        setControl("Blend", ScratchPadd::ControlType::Boolean(!blend()));
        drawFigures();
      }
      for (int key : {KEY_A, KEY_X, KEY_Y}) {
        if (down & key) {
          send(ButtonPressed{key});
        }
      }
    }
    drawRagdoll();
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
      // In Q16.16 directly: integer arithmetic, no float work
      fraction = Scene::Scalar::fromFixed(static_cast<std::int32_t>(
          std::min<long long>(since.count() * 65536 / latest_.stepMicroseconds, 65536)));
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
    std::printf("\x1b[1;1HCyclone's ragdoll on a DS\x1b[K");
    if (renderer_) {
      const Scene::FrameCounts drawn = renderer_->lastFrame();
      std::printf("\x1b[2;1H3D: %u polygons, %u vertices\x1b[K", drawn.polygons, drawn.vertices);
    }
    std::printf("\x1b[3;1HPhysics: %d steps a second,\x1b[K", figures_.stepsPerSecond);
    std::printf("\x1b[4;1H  %d sub-step%s each, %s\x1b[K", figures_.subSteps,
                figures_.subSteps == 1 ? "" : "s", blend() ? "blended" : "unblended");
    // (While running, a step's time includes the main thread's, which takes
    // over each frame to draw)
    std::printf("\x1b[5;1HA step takes %.0f ms (running)\x1b[K", figures_.stepMilliseconds);
    std::printf("\x1b[6;1HPhysics thread busy: %.0f%%\x1b[K", 100 * figures_.busy);
    std::printf("\x1b[7;1HSimulation speed: %.2fx\x1b[K", figures_.speed);
    std::printf("\x1b[8;1HWorst joint stretch: %.2f cm\x1b[K", 100 * figures_.worstStretch);
    if (figures_.audio) {
      std::printf("\x1b[9;1HKnocks played: %u\x1b[K", figures_.knocks);
    } else {
      std::printf("\x1b[9;1HNo AudioPadd: no sound\x1b[K");
    }
    std::printf("\x1b[10;1HA: reset X: sub-steps B: blend\x1b[K");
    std::printf("\x1b[11;1HY: 30/60 steps SELECT: controls\x1b[K");
    std::printf("\x1b[12;1HSTART: quit\x1b[K");
    std::printf("\x1b[13;1HMeasured at start (ms a step):\x1b[K");
    std::printf("\x1b[14;1Hsteps/s sub sweeps        ms\x1b[K");
    int row = 15;
    for (const Benchmark::Row &r : benchmark_.rows) {
      if (r.subSteps > 0) {
        std::printf("\x1b[%d;1H%-7d %-3d %2d/%2d/%2d  %7.1f\x1b[K", row, r.stepsPerSecond,
                    r.subSteps, r.velocitySweeps, r.settleSweeps, r.positionSweeps,
                    r.stepMilliseconds);
      }
      ++row;
    }
    std::printf("\x1b[%d;1HA step as it runs, ms:\x1b[K", ++row);
    std::printf("\x1b[%d;1Hmove %.1f find %.1f solve %.1f\x1b[K", ++row,
                benchmark_.integrateMilliseconds, benchmark_.contactsMilliseconds,
                benchmark_.resolveMilliseconds);
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
  ScratchPadd::HandheldControls::ControlTabs controls_;
  PhysicsFigures figures_;
  Benchmark benchmark_;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  ScratchPadd::SystemImplementation<ScreenPadd, RagdollPadd, AudioPadd> system;
  system.instantiate();
  system.start(); // returns when ScreenPadd stops it (START)
  return 0;
}
