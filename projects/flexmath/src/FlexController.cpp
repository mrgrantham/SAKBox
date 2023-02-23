#include "FlexController.hpp"
namespace Flex::Math {

std::shared_ptr<Flex::Math::FlexGroup> FlexController::getLatestFlexGroup() {
  return getFlexGroup(latestFlexGroupID_);
}

void FlexController::setActiveFlexGroup(FlexGroupID flexGroupID) {
  activeFlexGroup_ = groupRegistry_[latestFlexGroupID_];
}

std::shared_ptr<Flex::Math::FlexGroup> FlexController::getActiveFlexGroup() {
  return activeFlexGroup_;
}

FlexGroupID FlexController::registerFlexGroup(
    std::shared_ptr<Flex::Math::FlexGroup> flexGroup) {
  latestFlexGroupID_++;
  groupRegistry_[latestFlexGroupID_] = flexGroup;
  return latestFlexGroupID_;
}

std::shared_ptr<FlexController> FlexController::getInstance() {
  static std::shared_ptr<FlexController> flexController;
  return flexController;
}

FlexGroupID FlexController::registerFlexPoint(FlexPoint *flexPoint) {
  getLatestFlexGroup()->registerFlexPoint(flexPoint);
  return latestFlexGroupID_;
}
} // namespace Flex::Math
