#include "FlexController.hpp"

#include <stdexcept>

namespace Flex::Math {

FlexGroup &FlexController::addGroup(const std::string &name, Precision base, Policy policy) {
  auto [it, inserted] = groups_.try_emplace(name, nullptr);
  if (!inserted) {
    throw std::invalid_argument("FlexController: group '" + name + "' already exists");
  }
  it->second = std::make_unique<FlexGroup>(name, base, policy);
  return *it->second;
}

FlexGroup &FlexController::group(const std::string &name) {
  auto it = groups_.find(name);
  if (it == groups_.end()) {
    throw std::out_of_range("FlexController: no group named '" + name + "'");
  }
  return *it->second;
}

const FlexGroup &FlexController::group(const std::string &name) const {
  return const_cast<FlexController *>(this)->group(name);
}

} // namespace Flex::Math
