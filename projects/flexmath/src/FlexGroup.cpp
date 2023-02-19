#include "FlexGroup.hpp"
namespace Flex::Math {

    void FlexGroup::registerFlexPoint(Flex::Math::FlexPoint* flexPoint) {
      pointRegistry_.insert(flexPoint);
    }

    void FlexGroup::deregisterFlexPoint(Flex::Math::FlexPoint *flexPoint) {
      pointRegistry_.erase(flexPoint);
    }

    int16_t FlexGroup::getFractionalComponent() const {
      return fractionalComponent_;
    }

}