// FlexController will be a central control center that 
// can change the precision of all FlexPoint objects instantiated 
// with a reference to it.

#pragma once
#include <unordered_map>
#include <memory>
#include <FlexGroup.hpp>
// TODO: 
// Controller is statically allocated for all flexpoint operations. Every 
// flexpoint object registers with it on contruction and deregisters on deletion

// Through this interface all flexpoint objects can change their precision
namespace Flex::Math {

  typedef size_t FlexGroupID;

  class FlexController {    
    public:

    std::shared_ptr<Flex::Math::FlexGroup> getLatestFlexGroup();
    
    std::shared_ptr<Flex::Math::FlexGroup> getFlexGroup(FlexGroupID flexGroupID){
      return groupRegistry_[flexGroupID];
    }

    void setActiveFlexGroup(FlexGroupID flexGroupID);
    std::shared_ptr<Flex::Math::FlexGroup> getActiveFlexGroup();

    FlexGroupID registerFlexGroup(std::shared_ptr<Flex::Math::FlexGroup> flexGroup);

    static std::shared_ptr<FlexController> getInstance();

    FlexGroupID registerFlexPoint(FlexPoint* flexPoint);

    private:

    FlexController(){
      auto defaultFlexGroup = std::make_shared<FlexGroup>();
      auto defaultFlexGroupID = registerFlexGroup(defaultFlexGroup);
      setActiveFlexGroup(defaultFlexGroupID);
    };

    FlexGroupID latestFlexGroupID_;
    std::unordered_map<FlexGroupID, std::shared_ptr<Flex::Math::FlexGroup>> groupRegistry_;
    std::shared_ptr<FlexGroup> activeFlexGroup_;
  };
}
