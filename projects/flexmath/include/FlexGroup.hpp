// FlexGroup.hpp - shared precision knobs, and overrides for changing them
// temporarily.

#pragma once

#include <FlexPoint.hpp>
#include <FlexPrecision.hpp>
#include <FlexReal.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Flex::Math {

// What a group does to the values that pass through it.
enum class Policy {
  // view() snaps, commit() returns values unchanged. Stored state keeps full
  // precision, so raising precision again restores everything exactly.
  // Use for rendering, and for checks that should *see* coarse values (e.g.
  // collision tests) while the simulation itself stays exact.
  View,

  // commit() snaps too, so detail lost while precision is low stays lost.
  // Use for state that should genuinely degrade: drift, sticky controls,
  // damping that stops working.
  Lossy,

  // Every arithmetic operation snaps, by running FlexReal code under
  // activate(). commit() snaps like Lossy. The most extreme mode; whole
  // algorithms degrade at once.
  PerOperation,
};

class FlexGroup;

// A temporary change to a group's precision, typically from a powerup or a
// level event. The override applies for as long as the handle is alive;
// release() or destroying the handle removes it.
//
// Overrides may overlap and may be released in any order: the group always
// uses its most recently pushed override that is still alive, else its base.
//
// Handles are move-only. Assigning a new handle releases the old one, which
// makes "replace the current override" a single statement:
//   focus = flight.push(Precision{.fracBits = 5});
//
// The group must outlive its handles.
class FlexOverride {
public:
  // An empty handle that holds no override.
  FlexOverride() = default;
  FlexOverride(FlexOverride &&other) noexcept;
  FlexOverride &operator=(FlexOverride &&other) noexcept;
  FlexOverride(const FlexOverride &) = delete;
  FlexOverride &operator=(const FlexOverride &) = delete;
  ~FlexOverride() { release(); }

  // Removes the override from its group. Safe to call more than once.
  void release();

  // True while this handle holds an override.
  bool active() const { return group_ != nullptr; }

private:
  friend class FlexGroup;
  FlexOverride(FlexGroup *group, uint64_t id) : group_(group), id_(id) {}

  FlexGroup *group_ = nullptr;
  uint64_t id_ = 0;
};

// A precision knob shared by everything that reads through it - usually one
// subsystem, such as flight physics, collision checks or rendering.
//
// Values never register with a group. They are stored at full precision, and
// the group's current precision is applied when they are read (view) or
// written back (commit). Changing a group's precision therefore costs nothing
// and reaches every value on its next read.
//
// Example:
//   FlexGroup flight("Flight", Precision{.fracBits = 1});    // 0.5 steps
//   double seen = flight.view(4.3);                           // 4.0
//   FlexOverride focus = flight.push(Precision{.fracBits = 5});
//   seen = flight.view(4.3);                                  // 4.28125
//   focus.release();                                          // back to 0.5 steps
//
// Groups are not thread-safe: change and read a group from one thread, or
// synchronise access yourself.
class FlexGroup {
public:
  explicit FlexGroup(std::string name, Precision base = kFullPrecision, Policy policy = Policy::View)
      : name_(std::move(name)), base_(base), policy_(policy) {}

  // Overrides point at their group, so a group stays where it was created.
  FlexGroup(const FlexGroup &) = delete;
  FlexGroup &operator=(const FlexGroup &) = delete;

  const std::string &name() const { return name_; }

  // The precision used when no override is active.
  const Precision &base() const { return base_; }
  void setBase(const Precision &base) { base_ = base; }

  Policy policy() const { return policy_; }
  void setPolicy(Policy policy) { policy_ = policy; }

  // The precision in effect: the most recent live override, else the base.
  const Precision &precision() const { return overrides_.empty() ? base_ : overrides_.back().second; }

  // Applies `precision` until the returned handle is released or destroyed.
  [[nodiscard]] FlexOverride push(const Precision &precision);

  // Number of overrides currently alive.
  size_t overrideCount() const { return overrides_.size(); }

  // `value` as seen through this group, at the current precision. Never
  // changes stored state; use for anything that only reads.
  double view(double value) const { return quantize(value, precision()); }
  FlexPoint view(const FlexPoint &value) const { return value.quantized(precision()); }

  // `value` as it should be stored back after an update. Snapped under the
  // Lossy and PerOperation policies; unchanged under View.
  //   position = flight.commit(position + velocity * dt);
  double commit(double value) const { return policy_ == Policy::View ? value : view(value); }
  FlexPoint commit(const FlexPoint &value) const { return policy_ == Policy::View ? value : view(value); }

  // Makes FlexReal arithmetic on this thread snap to this group's current
  // precision until the returned object goes out of scope:
  //   {
  //     auto active = physics.activate();
  //     world.runPhysics(dt);   // FlexReal-based code, snapped throughout
  //   }
  // The precision is captured when activate() is called; later changes to
  // the group do not affect an activation already in progress.
  [[nodiscard]] ScopedActivePrecision activate() const { return ScopedActivePrecision(precision()); }

private:
  friend class FlexOverride;
  void release(uint64_t id);

  std::string name_;
  Precision base_;
  Policy policy_;
  std::vector<std::pair<uint64_t, Precision>> overrides_; // oldest first
  uint64_t nextOverrideId_ = 1;
};

} // namespace Flex::Math
