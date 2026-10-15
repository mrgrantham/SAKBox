#include "FlexGroup.hpp"

#include <algorithm>

namespace Flex::Math {

FlexOverride::FlexOverride(FlexOverride &&other) noexcept
    : group_(std::exchange(other.group_, nullptr)), id_(other.id_) {}

FlexOverride &FlexOverride::operator=(FlexOverride &&other) noexcept {
  if (this != &other) {
    release();
    group_ = std::exchange(other.group_, nullptr);
    id_ = other.id_;
  }
  return *this;
}

void FlexOverride::release() {
  if (group_) {
    group_->release(id_);
    group_ = nullptr;
  }
}

FlexOverride FlexGroup::push(const Precision &precision) {
  const uint64_t id = nextOverrideId_++;
  overrides_.emplace_back(id, precision);
  return FlexOverride(this, id);
}

void FlexGroup::release(uint64_t id) {
  overrides_.erase(std::remove_if(overrides_.begin(), overrides_.end(),
                                  [id](const auto &entry) { return entry.first == id; }),
                   overrides_.end());
}

} // namespace Flex::Math
