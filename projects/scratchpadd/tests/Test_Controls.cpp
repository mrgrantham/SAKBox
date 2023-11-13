#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/DataDependencies.hpp>

#include <gtest/gtest.h>

static auto logger = spdlog::stdout_color_mt("test");    

class ControlGeneratorPadd : public ScratchPadd::Base {
public:
      ControlGeneratorPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
}
std::string name() override {
  return __CLASS_NAME__;
}
bool shouldUseMainThread() override { return true; }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                  [this](ScratchPadd::MessageType::ControlChange &message) {
                    logger().info("{} Control controlName: {}", this->name(), message.controlName);
                    if (message.paddName == name()) {
                      updateControl(message);
                    }
                    send(generateControlsSnapshot());
                  },
                  [&](ScratchPadd::MessageType::ControlRequest &messageContent) {
                     logger().info("{} Control request padd name: {}", name(), messageContent.paddName.value_or("None"));
                    send(generateControlsSnapshot());
                   },
                  [&](auto &message) {
                    logger().error("Unhandled messages in {} receiver",name());
                   }},
               messageVariant);
  }

ScratchPadd::ControlValueMap  generateControls() override {
    logger().info("Generating controls for {}",name());
    return {
        {"aDouble",
         ScratchPadd::ControlType::Double(
             20.0, std::make_optional(std::make_pair(0.0, 200.0)))},
        {"aString", ScratchPadd::ControlType::String(
                        "DefaultString",
                        std::make_optional(std::vector<std::string>(
                            {"DefaultString", "AnotherString", "YetAnother"})))},
        {"aBool",
         ScratchPadd::ControlType::Boolean(
             true)},
        {"anInt",
         ScratchPadd::ControlType::Integer(
             5, std::make_optional(std::make_pair(2, 9))
             )},
             };
  }

};

class ControlExposurePadd : public ScratchPadd::Base {

  private:

  std::unordered_map<std::string, ScratchPadd::ControlValueMap> paddControls_;
    public:

  std::unordered_map<std::string, ScratchPadd::ControlValueMap>& getAllPaddControlMaps() {
    return paddControls_;
   }

std::string name() override {
  return __CLASS_NAME__;
}
          ControlExposurePadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }
    virtual bool shouldUseMainThread() override { return true; }

    virtual void starting() override {
        send(ScratchPadd::MessageType::ControlRequest{});
    }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                  [&](ScratchPadd::MessageType::ControlRequest &messageContent) {
                      spdlog::info("{} Control request padd name: {}",name(), messageContent.paddName.value_or("None"));
                   },
                  [&](ScratchPadd::MessageType::ControlChange &messageContent) {
                      spdlog::info("{} Control change padd name: {}", name(), messageContent.paddName);
                   },
                  [&](ScratchPadd::MessageType::ControlSnapshot &messageContent) {
                      spdlog::info("{} Control snapshot padd name: {}", name(), messageContent.paddName);
                      setupControlView(messageContent);
                   },
                [&](auto &message) {
                    spdlog::error("Unhandled messages in {} receiver",name());
                   }},
               messageVariant);
  }

  void setupControlView(ScratchPadd::MessageType::ControlSnapshot &controlSnapshot) {
    logger().info("Setting up control view from type {}", TypeName(controlSnapshot));
    if (paddControls_.contains(controlSnapshot.paddName)) {
      logger().info("Control map already added. Updating...");
    }
    logger().info("Adding control for {} with size {}",controlSnapshot.paddName,controlSnapshot.controlMap.size());
    paddControls_[controlSnapshot.paddName] = controlSnapshot.controlMap;
  }

  ScratchPadd::ControlValueMap generateControls() override {
    logger().info("No controls required for the ControlExposurePadd");
    return {};
  }
};


// Demonstrate some basic assertions.
TEST(ControlsTest, TestControlRequest) {

    using namespace std::chrono_literals;

    auto *spsystem =
    dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd>*>(ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

    // ScratchPadd::Data::ConfigureDependencies(argv[0]);
    SCOPED_METHOD_TIMER();
    // signal(SIGINT, signal_handler);
    logger->info("Welcome to SCRATCHPADD!");
    spsystem->instantiate();

    auto systemThread = std::thread(
      [&]{
        spsystem->start();
      }
    );
    spsystem->waitForStart();

    logger->info("Getting workers");
    auto &workers = spsystem->getWorkers();
    
    auto &controlGenPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);
    auto &controlExpPadd = std::get<std::unique_ptr<ControlExposurePadd>>(workers);

    // Startup Expectation: 
    // ControlExposurePadd sends a ControlRequest to ControlGeneratorPadd
    // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd

    spsystem->waitForWorkCompletion();
    
    auto &allPaddControlMaps = controlExpPadd->getAllPaddControlMaps();
    EXPECT_EQ(allPaddControlMaps.size(), 1);
    for (auto &[key, value]: allPaddControlMaps) {
      logger->info("Padd controls key: {}",key);
    }
    EXPECT_TRUE(allPaddControlMaps.contains(controlGenPadd->name()));
    auto &genPaddControls = allPaddControlMaps[controlGenPadd->name()];
    EXPECT_EQ(genPaddControls.size(),4);

    spsystem->stop();
    systemThread.join();
}

// Demonstrate some basic assertions.
TEST(ControlsTest, TestControlChange) {
    using namespace std::chrono_literals;

    auto *spsystem =
    dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd>*>(ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

    // ScratchPadd::Data::ConfigureDependencies(argv[0]);
    SCOPED_METHOD_TIMER();
    // signal(SIGINT, signal_handler);
    logger->info("Welcome to SCRATCHPADD!");
    spsystem->instantiate();

    auto systemThread = std::thread(
      [&]{
        spsystem->start();
      }
    );
    spsystem->waitForStart(); // Waits untill all workers are about to enter there work loop

    logger->info("Getting workers");
    auto &workers = spsystem->getWorkers();
    
    auto &controlGenPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);
    auto &controlExpPadd = std::get<std::unique_ptr<ControlExposurePadd>>(workers);

    // Startup Expectation: 
    // ControlExposurePadd sends a ControlRequest to ControlGeneratorPadd
    // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd

    controlGenPadd->waitForWorkCompletion();
    controlExpPadd->waitForWorkCompletion();


    // Modification Expectation:
    // ControlExposurePadd send a ControlChange to ControlGeneratorPadd
    // ControlGeneratorPadd send a ControlSnapshot to ControlExposurePadd


    // Check value before change
    auto &allPaddControlMapsBeforeChange = controlExpPadd->getAllPaddControlMaps();
    EXPECT_EQ(allPaddControlMapsBeforeChange.size(), 1);
    EXPECT_TRUE(allPaddControlMapsBeforeChange.contains(controlGenPadd->name()));
    auto &genPaddControls = allPaddControlMapsBeforeChange[controlGenPadd->name()];
    EXPECT_EQ(genPaddControls.size(),4);
    std::string controlName = "aDouble";
    EXPECT_TRUE(genPaddControls.contains(controlName));
    ScratchPadd::ControlTypeVariant &controlTypeValue = genPaddControls[controlName];
    EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(controlTypeValue));


    // confirm original value set in ControlsGeneratorPadd generateControls() method
    EXPECT_DOUBLE_EQ(std::get<ScratchPadd::ControlType::Double>(controlTypeValue).value,20.0);

    // update the value
    const double updatedValue = 40.1;
    auto updatedControlTypeValue = std::get<ScratchPadd::ControlType::Double>(controlTypeValue); // make a copy. value held in ControlExposurePadd will be unchanged until the update is round tripped back from ControlGeneratorPadd
    updatedControlTypeValue.value = updatedValue;
    // Send a control change back to the generator
    controlExpPadd->send(ScratchPadd::MessageType::ControlChange{.paddName = controlGenPadd->name() ,.controlName = controlName,.controlTypeValue = updatedControlTypeValue});


    controlGenPadd->waitForWorkCompletion();
    controlExpPadd->waitForWorkCompletion();

    // Check that the ControlsGeneratorPadd controls map has been updated with the new value

    auto genPaddControlsAfterChange = controlGenPadd->generateControlsSnapshot().controlMap;
    EXPECT_EQ(genPaddControlsAfterChange.size(),4);
    EXPECT_TRUE(genPaddControlsAfterChange.contains(controlName));
    EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(genPaddControlsAfterChange[controlName]));

    EXPECT_DOUBLE_EQ(std::get<ScratchPadd::ControlType::Double>(genPaddControlsAfterChange[controlName]).value,updatedValue);

    // Check that that update has been published back to the ControlExposurePadd
    // This is to ensure that the two controls are in sync.

    auto &allPaddControlMapsAfterChange = controlExpPadd->getAllPaddControlMaps();

    EXPECT_TRUE(allPaddControlMapsAfterChange.contains(controlGenPadd->name()));
    auto &controlMap = allPaddControlMapsAfterChange[controlGenPadd->name()];

    EXPECT_TRUE(controlMap.contains(controlName));
    EXPECT_TRUE(std::holds_alternative<ScratchPadd::ControlType::Double>(controlMap[controlName]));

    EXPECT_DOUBLE_EQ(std::get<ScratchPadd::ControlType::Double>(controlMap[controlName]).value,updatedValue);

    spsystem->stop();
    systemThread.join();
}
