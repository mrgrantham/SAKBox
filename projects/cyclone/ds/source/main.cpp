// Cyclone's ragdoll on a Nintendo DS, in padds.
//
//   ScreenPadd  (main thread) draws the ragdoll with the DS's 3D hardware on
//               the top screen, from RagdollPadd's snapshots, and the
//               physics' figures on the bottom screen; sends the buttons
//   RagdollPadd (own thread)  steps the ragdoll (ragdoll_physics.h, as in
//               Cyclone's demo) in fixed steps and sub-steps (FixedSteps),
//               timing each step
//
// A: reset (a new shove)   X: 1, 2 or 4 sub-steps   Y: 60 or 30 steps a second
// START: quit
#include <nds.h>

#include <ScratchPadd/ScratchPadd.hpp>
#include <cyclone/cyclone.h>
#include <fixed_steps.h>
#include <ragdoll_physics.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>

namespace {

using Clock = std::chrono::steady_clock;

struct ButtonPressed {
  int key = 0; // libnds' KEY_*
};

// The DS's 3D vertices are 4.12 fixed point (-8 to 8), so the world is
// drawn at a quarter of its size
constexpr float kDrawScale = 0.25f;

// A position (m) as a vertex coordinate. Cyclone's real is Q16.16 fixed point
// here (FlexPoint16, see the Makefile), so that's a shift: 16 fraction bits
// to 12, and a quarter (2 bits) smaller. (Through float, it was software
// floating point for each of the 288 coordinates a step.)
v16 vertexCoordinate(cyclone::real metres) {
  if constexpr (std::is_same_v<cyclone::real, Flex::Math::FlexPoint16>) {
    return static_cast<v16>(metres.valueFixed() >> 6);
  } else {
    return floattov16(static_cast<float>(metres) * kDrawScale);
  }
}

// Each bone's eight corners after a tick, ready to draw
struct BonesSnapshot {
  std::array<std::array<std::array<v16, 3>, 8>, ragdoll::kBones> corners{};
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
  std::array<Row, 4> rows{};
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
      } else if (pressed.key == KEY_Y) {
        // Half as many steps a second is half the work for each second
        // simulated, with coarser steps (joints stretch more on impact)
        clock_.setStepSeconds(clock_.stepSeconds() == kSlowStepSeconds
                                  ? FixedSteps::kDefaultStepSeconds
                                  : kSlowStepSeconds);
      }
    });
    reset();
  }
  std::string name() override { return "RagdollPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::duration<double>(FixedSteps::kDefaultStepSeconds);
  }

  // Times a few settings first (a few seconds), then runs with the cheap one
  void starting() override {
    send(benchmark());
    useSettings(kRunSettings);
    reset();
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
                          .worstStretch = worstStretch_});
      secondStart_ = Clock::now();
      stepping_ = {};
      stepsTaken_ = 0;
    }
  }

private:
  // Sub-steps, and the resolver's velocity, settle and position sweeps
  struct Settings {
    unsigned stepsPerSecond, subSteps, velocitySweeps, settleSweeps, positionSweeps;
  };
  // Cyclone's own sweeps (40, 10, 10) suit desktops; with sub-steps a few
  // sweeps each are enough (Box2D v3 takes one), and on the DS they're what
  // the time goes on
  static constexpr Settings kRunSettings{60, 2, 8, 2, 2};
  // Y's other step length: 30 steps a second
  static constexpr double kSlowStepSeconds = 1.0 / 30;
  static constexpr Settings kMeasured[] = {
      {60, 1, 40, 10, 10}, {60, 2, 8, 2, 2}, {60, 4, 4, 1, 1}, {30, 2, 8, 2, 2}};
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

  BonesSnapshot snapshot() const {
    BonesSnapshot s;
    for (unsigned b = 0; b < ragdoll::kBones; ++b) {
      const ragdoll::Bone &bone = ragdoll_->bones[b];
      for (int c = 0; c < 8; ++c) {
        const cyclone::Vector3 local((c & 1) ? bone.halfSize.x : -bone.halfSize.x,
                                     (c & 2) ? bone.halfSize.y : -bone.halfSize.y,
                                     (c & 4) ? bone.halfSize.z : -bone.halfSize.z);
        const cyclone::Vector3 world = bone.body->getPointInWorldSpace(local);
        s.corners[b][c] = {vertexCoordinate(world.x), vertexCoordinate(world.y),
                           vertexCoordinate(world.z)};
      }
    }
    return s;
  }

  void reset() {
    ragdoll_->reset(random_);
    worstStretch_ = 0;
  }

  std::unique_ptr<ragdoll::Ragdoll> ragdoll_;
  cyclone::Random random_;
  FixedSteps clock_;
  double worstStretch_ = 0;
  Clock::time_point lastTick_, secondStart_;
  Clock::duration stepping_{};
  int stepsTaken_ = 0;
};

class ScreenPadd : public ScratchPadd::Base {
public:
  ScreenPadd(ScratchPadd::System *system) : Base(system) {
    receive<BonesSnapshot>([this](const BonesSnapshot &bones) { bones_ = bones; });
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
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void starting() override {
    videoSetMode(MODE_0_3D);
    consoleDemoInit(); // the bottom screen
    glInit();
    glEnable(GL_ANTIALIAS);
    glClearColor(28, 29, 31, 31); // pale sky
    glClearPolyID(63);
    glClearDepth(0x7FFF);
    glViewport(0, 0, 255, 191);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50, 256.0f / 192.0f, 0.1f, 40);
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
    for (int key : {KEY_A, KEY_X, KEY_Y}) {
      if (down & key) {
        send(ButtonPressed{key});
      }
    }
    drawRagdoll();
    glFlush(0);
    swiWaitForVBlank();
    push([this] { frame(); });
  }

  void drawRagdoll() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    {
      // gluLookAt uses the divider and square root unit, which the physics
      // thread's fixed point uses too (FixedPointNds.hpp): no switching
      // threads partway through
      Flex::Math::HardwareMathsLock lock;
      gluLookAt(3.0f, 1.2f, 2.2f, 0.0f, 0.3f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBegin(GL_QUADS);
    // The ground: a sand square, 16 m across
    glColor3b(200, 190, 160);
    for (auto [x, z] : {std::pair{-8.0f, -8.0f}, {8.0f, -8.0f}, {8.0f, 8.0f}, {-8.0f, 8.0f}}) {
      glVertex3v16(floattov16(x * kDrawScale), 0, floattov16(z * kDrawScale));
    }
    // Each bone's six faces, shaded by which way they face
    static constexpr int kFaces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                                         {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    static constexpr u8 kShade[6] = {150, 150, 120, 200, 170, 170};
    for (const auto &bone : bones_.corners) {
      for (int f = 0; f < 6; ++f) {
        glColor3b(kShade[f], kShade[f] * 2 / 3, kShade[f] * 2 / 3);
        for (int corner : kFaces[f]) {
          glVertex3v16(bone[corner][0], bone[corner][1], bone[corner][2]);
        }
      }
    }
    glEnd();
  }

  void drawFigures() {
    std::printf("\x1b[1;1HCyclone's ragdoll on a DS\x1b[K");
    std::printf("\x1b[3;1HPhysics: %d steps a second,\x1b[K", figures_.stepsPerSecond);
    std::printf("\x1b[4;1H  %d sub-step%s each\x1b[K", figures_.subSteps,
                figures_.subSteps == 1 ? "" : "s");
    // (While running, a step's time includes the main thread's, which takes
    // over each frame to draw)
    std::printf("\x1b[5;1HA step takes %.0f ms (running)\x1b[K", figures_.stepMilliseconds);
    std::printf("\x1b[6;1HPhysics thread busy: %.0f%%\x1b[K", 100 * figures_.busy);
    std::printf("\x1b[7;1HSimulation speed: %.2fx\x1b[K", figures_.speed);
    std::printf("\x1b[8;1HWorst joint stretch: %.2f cm\x1b[K", 100 * figures_.worstStretch);
    std::printf("\x1b[10;1HA: reset  X: sub-steps\x1b[K");
    std::printf("\x1b[11;1HY: 60 or 30 steps a second\x1b[K");
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
    std::printf("\x1b[%d;1HA step at 60/2, 8/2/2, ms:\x1b[K", ++row);
    std::printf("\x1b[%d;1Hmove %.1f find %.1f solve %.1f\x1b[K", ++row,
                benchmark_.integrateMilliseconds, benchmark_.contactsMilliseconds,
                benchmark_.resolveMilliseconds);
  }

  BonesSnapshot bones_;
  PhysicsFigures figures_;
  Benchmark benchmark_;
};

} // namespace

int main() {
  spdlog::set_level(spdlog::level::off); // the screens are for the padds
  ScratchPadd::SystemImplementation<ScreenPadd, RagdollPadd> system;
  system.instantiate();
  system.start(); // returns when ScreenPadd stops it (START)
  return 0;
}
