// Cyclone's ragdoll on a Nintendo DS, in padds.
//
//   ScreenPadd  (main thread) draws the ragdoll with the DS's 3D hardware on
//               the top screen, from RagdollPadd's snapshots, and the
//               physics' figures on the bottom screen; sends the buttons
//   RagdollPadd (own thread)  steps the ragdoll (ragdoll_physics.h, as in
//               Cyclone's demo) in fixed steps and sub-steps (FixedSteps),
//               timing each step
//
// A: reset (a new shove)   X: 1, 2 or 4 sub-steps   START: quit
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

namespace {

using Clock = std::chrono::steady_clock;

struct ButtonPressed {
  int key = 0; // libnds' KEY_*
};

// Each bone's eight corners, in metres, after a tick
struct BonesSnapshot {
  std::array<std::array<std::array<float, 3>, 8>, ragdoll::kBones> corners{};
};

// How long a step took with each of a few settings, measured when the app
// starts (each from a fresh ragdoll, over its first steps)
struct Benchmark {
  struct Row {
    int subSteps = 0;
    int velocitySweeps = 0, settleSweeps = 0, positionSweeps = 0;
    double stepMilliseconds = 0;
  };
  std::array<Row, 4> rows{};
  // Where the time goes in a sub-step at the settings it runs with
  double integrateMilliseconds = 0, contactsMilliseconds = 0, resolveMilliseconds = 0;
};

// What the physics measured over the last second
struct PhysicsFigures {
  int subSteps = 0;
  double stepMilliseconds = 0; // a whole step, all its sub-steps
  double busy = 0;             // share of the second spent stepping
  double speed = 0;            // simulated seconds a second (under 1: falling behind)
  double worstStretch = 0;     // the most a joint was beyond its slack since reset (m)
};

class RagdollPadd : public ScratchPadd::Base {
public:
  static constexpr unsigned kMaxContacts = 256;

  RagdollPadd(ScratchPadd::System *system)
      : Base(system), contacts_(std::make_unique<cyclone::Contact[]>(kMaxContacts)),
        resolver_(kMaxContacts * 8), ragdoll_(std::make_unique<ragdoll::Ragdoll>()) {
    cData_.contactArray = contacts_.get();
    receive<ButtonPressed>([this](const ButtonPressed &pressed) {
      if (pressed.key == KEY_A) {
        reset();
      } else if (pressed.key == KEY_X) {
        clock_.setSubSteps(clock_.subSteps() == 4 ? 1 : clock_.subSteps() * 2);
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
      send(PhysicsFigures{.subSteps = static_cast<int>(clock_.subSteps()),
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
    unsigned subSteps, velocitySweeps, settleSweeps, positionSweeps;
  };
  // Cyclone's own sweeps (40, 10, 10) suit desktops; with sub-steps a few
  // sweeps each are enough (Box2D v3 takes one), and on the DS they're what
  // the time goes on
  static constexpr Settings kRunSettings{2, 8, 2, 2};
  static constexpr Settings kMeasured[] = {
      {1, 40, 10, 10}, {2, 40, 10, 10}, {2, 8, 2, 2}, {4, 4, 1, 1}};
  static constexpr int kStepsMeasured = 5;

  void useSettings(const Settings &settings) {
    clock_.setSubSteps(settings.subSteps);
    resolver_.setSweeps(settings.velocitySweeps, settings.settleSweeps, settings.positionSweeps);
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
          .subSteps = static_cast<int>(settings.subSteps),
          .velocitySweeps = static_cast<int>(settings.velocitySweeps),
          .settleSweeps = static_cast<int>(settings.settleSweeps),
          .positionSweeps = static_cast<int>(settings.positionSweeps),
          .stepMilliseconds =
              std::chrono::duration<double, std::milli>(Clock::now() - started).count() /
              kStepsMeasured};
    }
    // Each phase of a sub-step, at the settings it runs with
    useSettings(kRunSettings);
    reset();
    const auto duration = static_cast<cyclone::real>(clock_.subStepSeconds());
    Clock::duration integrating{}, findingContacts{}, resolving{};
    constexpr int kSubStepsMeasured = 10;
    for (int i = 0; i < kSubStepsMeasured; ++i) {
      auto t = Clock::now();
      ragdoll_->integrate(duration);
      integrating += Clock::now() - t;
      t = Clock::now();
      ragdoll_->generateContacts(cData_, kMaxContacts);
      findingContacts += Clock::now() - t;
      t = Clock::now();
      resolver_.resolveContacts(cData_.contactArray, cData_.contactCount, duration,
                                ragdoll_->jointList());
      resolving += Clock::now() - t;
    }
    const auto ms = [](Clock::duration d) {
      return std::chrono::duration<double, std::milli>(d).count() / kSubStepsMeasured;
    };
    result.integrateMilliseconds = ms(integrating);
    result.contactsMilliseconds = ms(findingContacts);
    result.resolveMilliseconds = ms(resolving);
    return result;
  }

  // One step, in its sub-steps, as Cyclone's demos step
  void step() {
    const auto duration = static_cast<cyclone::real>(clock_.subStepSeconds());
    for (unsigned i = 0; i < clock_.subSteps(); ++i) {
      ragdoll_->integrate(duration);
      ragdoll_->generateContacts(cData_, kMaxContacts);
      resolver_.resolveContacts(cData_.contactArray, cData_.contactCount, duration,
                                ragdoll_->jointList());
    }
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
        s.corners[b][c] = {static_cast<float>(world.x), static_cast<float>(world.y),
                           static_cast<float>(world.z)};
      }
    }
    return s;
  }

  void reset() {
    ragdoll_->reset(random_);
    resolver_.forgetWarmStart();
    cData_.contactCount = 0;
    worstStretch_ = 0;
  }

  std::unique_ptr<cyclone::Contact[]> contacts_;
  cyclone::CollisionData cData_;
  cyclone::ContactResolver resolver_;
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
    receive<PhysicsFigures>([this](const PhysicsFigures &figures) { figures_ = figures; });
    receive<Benchmark>([this](const Benchmark &benchmark) { benchmark_ = benchmark; });
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
  // The DS's 3D vertices are 4.12 fixed point (-8 to 8), so the world is
  // drawn at a quarter of its size
  static constexpr float kScale = 0.25f;

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
    for (int key : {KEY_A, KEY_X}) {
      if (down & key) {
        send(ButtonPressed{key});
      }
    }
    drawRagdoll();
    drawFigures();
    glFlush(0);
    swiWaitForVBlank();
    push([this] { frame(); });
  }

  static v16 at(float metres) { return floattov16(metres * kScale); }

  void drawRagdoll() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(3.0f, 1.2f, 2.2f, 0.0f, 0.3f, 0.0f, 0.0f, 1.0f, 0.0f);
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE);
    glBegin(GL_QUADS);
    // The ground: a sand square, 16 m across
    glColor3b(200, 190, 160);
    for (auto [x, z] : {std::pair{-8.0f, -8.0f}, {8.0f, -8.0f}, {8.0f, 8.0f}, {-8.0f, 8.0f}}) {
      glVertex3v16(floattov16(x * kScale), 0, floattov16(z * kScale));
    }
    // Each bone's six faces, shaded by which way they face
    static constexpr int kFaces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                                         {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    static constexpr u8 kShade[6] = {150, 150, 120, 200, 170, 170};
    for (const auto &bone : bones_.corners) {
      for (int f = 0; f < 6; ++f) {
        glColor3b(kShade[f], kShade[f] * 2 / 3, kShade[f] * 2 / 3);
        for (int corner : kFaces[f]) {
          glVertex3v16(at(bone[corner][0]), at(bone[corner][1]), at(bone[corner][2]));
        }
      }
    }
    glEnd();
  }

  void drawFigures() {
    std::printf("\x1b[1;1HCyclone's ragdoll on a DS\x1b[K");
    std::printf("\x1b[3;1HPhysics: 60 steps a second,\x1b[K");
    std::printf("\x1b[4;1H  %d sub-step%s each\x1b[K", figures_.subSteps,
                figures_.subSteps == 1 ? "" : "s");
    // (While running, a step's time includes the main thread's, which takes
    // over each frame to draw)
    std::printf("\x1b[5;1HA step takes %.0f ms (running)\x1b[K", figures_.stepMilliseconds);
    std::printf("\x1b[6;1HPhysics thread busy: %.0f%%\x1b[K", 100 * figures_.busy);
    std::printf("\x1b[7;1HSimulation speed: %.2fx\x1b[K", figures_.speed);
    std::printf("\x1b[8;1HWorst joint stretch: %.2f cm\x1b[K", 100 * figures_.worstStretch);
    std::printf("\x1b[10;1HA: reset  X: sub-steps\x1b[K");
    std::printf("\x1b[11;1HSTART: quit\x1b[K");
    std::printf("\x1b[13;1HMeasured at start (ms a step):\x1b[K");
    std::printf("\x1b[14;1Hsub-steps sweeps        ms\x1b[K");
    int row = 15;
    for (const Benchmark::Row &r : benchmark_.rows) {
      if (r.subSteps > 0) {
        std::printf("\x1b[%d;1H%-9d %2d/%2d/%2d  %7.1f\x1b[K", row, r.subSteps, r.velocitySweeps,
                    r.settleSweeps, r.positionSweeps, r.stepMilliseconds);
      }
      ++row;
    }
    std::printf("\x1b[%d;1HA sub-step at 8/2/2, ms:\x1b[K", ++row);
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
