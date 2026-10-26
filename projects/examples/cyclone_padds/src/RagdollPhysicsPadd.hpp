// RagdollPhysicsPadd.hpp - Cyclone's ragdoll, stepped on this padd's own
// thread.
//
// Every tick (kPhysicsTickSeconds) it adds the real time since the last tick to a
// FixedSteps clock and takes the whole steps that covers, each in its
// sub-steps, then sends one RagdollSnapshot. Its controls (steps a second,
// sub-steps, the resolver, paused) are what the window's panel edits; the
// window's keys send RagdollCommands. It has no window, so its tests run it
// headless.
#pragma once

#include "RagdollMessages.hpp"

#include <ScratchPadd/Base.hpp>

#include <cyclone/cyclone.h>
#include <fixed_steps.h>
#include <ragdoll_physics.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CyclonePadds {

class RagdollPhysicsPadd : public ScratchPadd::Base {
public:
  static constexpr unsigned kMaxContacts = 256;

  RagdollPhysicsPadd(ScratchPadd::System *system)
      : Base(system), contacts_(std::make_unique<cyclone::Contact[]>(kMaxContacts)),
        resolver_(kMaxContacts * 8), ragdoll_(std::make_unique<ragdoll::Ragdoll>()) {
    cData_.contactArray = contacts_.get();
    receive<RagdollCommand>([this](const RagdollCommand &command) { runCommand(command); });
    reset();
  }

  std::string name() override { return "RagdollPhysics"; }

  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::duration<double>(kPhysicsTickSeconds);
  }

  ScratchPadd::ControlValueMap generateControls() override {
    namespace Controls = ScratchPadd::ControlType;
    return {
        {"Steps a second", Controls::Integer(60, std::make_pair(10, 240))},
        {"Sub-steps",
         Controls::Integer(static_cast<int>(FixedSteps::kDefaultSubSteps),
                           std::make_pair(1, static_cast<int>(FixedSteps::kMostSubSteps)))},
        {"Resolver", Controls::String(kTogether, std::vector<std::string>{kTogether, kOneAtATime})},
        {"Paused", Controls::Boolean(false)},
    };
  }

  void controlChanged(const std::string &) override { applyControls(); }

  void starting() override {
    applyControls();
    lastTick_ = std::chrono::steady_clock::now();
    busySince_ = lastTick_;
  }

  void repeat() override {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - lastTick_).count();
    lastTick_ = now;
    if (paused_) {
      return;
    }
    const unsigned steps = clock_.stepsFor(elapsed);
    for (unsigned i = 0; i < steps; ++i) {
      runStep();
    }
    const auto done = std::chrono::steady_clock::now();
    busySeconds_ += std::chrono::duration<double>(done - now).count();
    const double sinceBusy = std::chrono::duration<double>(done - busySince_).count();
    if (sinceBusy >= 1.0) {
      busy_ = busySeconds_ / sinceBusy;
      busySeconds_ = 0;
      busySince_ = done;
    }
    if (steps > 0) {
      send(snapshot());
    }
  }

  // For tests: the simulated seconds so far
  double time() const { return time_; }

private:
  static inline const std::string kTogether = "all contacts together";
  static inline const std::string kOneAtATime = "one contact at a time (the book's)";

  // One step, in its sub-steps: the rigid-body demos' step
  // (RigidBodyApplication::runStep)
  void runStep() {
    const auto duration = static_cast<cyclone::real>(clock_.subStepSeconds());
    for (unsigned i = 0; i < clock_.subSteps(); ++i) {
      ragdoll_->integrate(duration);
      ragdoll_->generateContacts(cData_, kMaxContacts);
      resolver_.resolveContacts(cData_.contactArray, cData_.contactCount, duration,
                                ragdoll_->jointList());
    }
    time_ += clock_.stepSeconds();
    stretchNow_ = ragdoll_->worstStretch();
    worstStretch_ = std::max(worstStretch_, stretchNow_);
  }

  RagdollSnapshot snapshot() const {
    RagdollSnapshot s;
    s.time = time_;
    s.generation = generation_;
    s.bones.reserve(ragdoll::kBones);
    for (const ragdoll::Bone &bone : ragdoll_->bones) {
      const cyclone::Vector3 p = bone.body->getPosition();
      const cyclone::Quaternion q = bone.body->getOrientation();
      s.bones.push_back({.position = glm::vec3(p.x, p.y, p.z),
                         .orientation = glm::normalize(
                             glm::quat(static_cast<float>(q.r), static_cast<float>(q.i),
                                       static_cast<float>(q.j), static_cast<float>(q.k))),
                         .halfSize = glm::vec3(bone.halfSize.x, bone.halfSize.y, bone.halfSize.z),
                         .awake = bone.body->getAwake()});
    }
    const auto boneIndex = [this](const cyclone::RigidBody *body) {
      for (unsigned i = 0; i < ragdoll::kBones; ++i) {
        if (ragdoll_->bones[i].body == body) {
          return static_cast<int>(i);
        }
      }
      return 0;
    };
    s.joints.reserve(ragdoll::kJoints);
    for (const cyclone::Joint &joint : ragdoll_->joints) {
      JointSnapshot j;
      for (int end = 0; end < 2; ++end) {
        j.bones[end] = boneIndex(joint.body[end]);
        const cyclone::Vector3 &point = joint.position[end];
        j.points[end] = glm::vec3(point.x, point.y, point.z);
      }
      j.slack = static_cast<float>(joint.error);
      s.joints.push_back(j);
    }
    s.stats = {.stepsPerSecond = static_cast<int>(std::lround(1 / clock_.stepSeconds())),
               .subSteps = static_cast<int>(clock_.subSteps()),
               .busy = busy_,
               .stretchNow = stretchNow_,
               .worstStretch = worstStretch_};
    return s;
  }

  void reset() {
    ragdoll_->reset(random_);
    resolver_.forgetWarmStart();
    cData_.contactCount = 0;
    stretchNow_ = 0;
    worstStretch_ = 0;
    ++generation_;
  }

  void runCommand(const RagdollCommand &command) {
    switch (command.type) {
    case RagdollCommand::Type::Reset:
      reset();
      send(snapshot()); // shown even while paused
      break;
    case RagdollCommand::Type::TogglePause:
      // Changed by the owner itself; setControl tells the panels
      setControl("Paused", ScratchPadd::ControlType::Boolean(!paused_));
      applyControls();
      break;
    }
  }

  // Copies the controls into the clock and resolver; they take effect from
  // the next step
  void applyControls() {
    namespace Controls = ScratchPadd::ControlType;
    clock_.setStepSeconds(1.0 / control<Controls::Integer>("Steps a second").value);
    clock_.setSubSteps(static_cast<unsigned>(control<Controls::Integer>("Sub-steps").value));
    const auto method = control<Controls::String>("Resolver").value == kOneAtATime
                            ? cyclone::ContactResolver::Method::oneContactAtATime
                            : cyclone::ContactResolver::Method::sequentialImpulses;
    if (method != resolver_.getMethod()) {
      resolver_.setMethod(method);
      resolver_.forgetWarmStart();
    }
    paused_ = control<Controls::Boolean>("Paused").value;
  }

  std::unique_ptr<cyclone::Contact[]> contacts_;
  cyclone::CollisionData cData_;
  cyclone::ContactResolver resolver_;
  std::unique_ptr<ragdoll::Ragdoll> ragdoll_;
  cyclone::Random random_;
  FixedSteps clock_;
  bool paused_ = false;

  double time_ = 0;
  std::uint32_t generation_ = 0;
  double stretchNow_ = 0, worstStretch_ = 0;
  std::chrono::steady_clock::time_point lastTick_, busySince_;
  double busySeconds_ = 0, busy_ = 0;
};

} // namespace CyclonePadds
