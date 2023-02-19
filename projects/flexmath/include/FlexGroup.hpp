// FlexController will be a central control center that 
// can change the precision of all FlexPoint objects instantiated 
// with a reference to it.

#pragma once
#include <unordered_set>
// TODO: 
// Controller is statically allocated for all flexpoint operations. Every 
// flexpoint object registers with it on contruction and deregisters on deletion

// Through this interface all flexpoint objects can change their precision
namespace Flex::Math {
  class FlexPoint;
  class FlexGroup {
    public:
    void registerFlexPoint(Flex::Math::FlexPoint *flexPoint);
    void deregisterFlexPoint(Flex::Math::FlexPoint *flexPoint);
    int16_t getFractionalComponent() const;
    private:
    int16_t fractionalComponent_{8};
    std::unordered_set<Flex::Math::FlexPoint*> pointRegistry_;
  };
}
