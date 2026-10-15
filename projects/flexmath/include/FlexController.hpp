// FlexController.hpp - the central control center for a game or
// simulation's precision.

#pragma once

#include <FlexGroup.hpp>
#include <map>
#include <memory>
#include <string>

namespace Flex::Math {

// Owns a game's FlexGroups and finds them by name, so any code - powerups,
// level scripts, debug tools - can reach any subsystem's precision.
//
// It is an ordinary object, not a singleton: the game owns one, and tests or
// separate simulations each create their own.
//
// Example:
//   FlexController precision;
//   FlexGroup &flight = precision.addGroup("Flight", Precision{.fracBits = 1});
//   precision.addGroup("Render", Precision{.fracBits = 9});
//
//   // Later, e.g. when a powerup is collected:
//   FlexOverride glitch = precision.group("Render").push(Precision{.fracBits = 3});
//
// References returned by addGroup() and group() stay valid for the lifetime
// of the controller. Like FlexGroup, the controller is not thread-safe.
class FlexController {
public:
  // Creates a group. Throws std::invalid_argument if the name is taken.
  FlexGroup &addGroup(const std::string &name, Precision base = kFullPrecision,
                      Policy policy = Policy::View);

  bool hasGroup(const std::string &name) const { return groups_.contains(name); }

  // The named group. Throws std::out_of_range if there is none.
  FlexGroup &group(const std::string &name);
  const FlexGroup &group(const std::string &name) const;

  size_t groupCount() const { return groups_.size(); }

  // Calls f(FlexGroup &) for every group, in name order - e.g. to draw a
  // debug panel or reset every base precision.
  template <typename F> void forEachGroup(F &&f) {
    for (auto &[name, group] : groups_) {
      f(*group);
    }
  }

private:
  // unique_ptr keeps each group at a stable address for its overrides
  std::map<std::string, std::unique_ptr<FlexGroup>> groups_;
};

} // namespace Flex::Math
