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

std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>  generateControls() override {
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

  void updateControl(ScratchPadd::MessageType::ControlChange &message) {

  }
};

class ControlExposurePadd : public ScratchPadd::Base {

  private:
  std::unordered_map<std::string, ScratchPadd::ControlValueMap> paddControls_;
    public:

  std::unordered_map<std::string, ScratchPadd::ControlValueMap>& getAllPaddControls() {
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
      logger().error("Control map already added");
      return;
    }
    paddControls_[controlSnapshot.paddName] = controlSnapshot.controlMap;
  }

  virtual std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {
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

    controlGenPadd->waitForWorkCompletion();
    controlExpPadd->waitForWorkCompletion();

    auto &paddControlMap = controlExpPadd->getAllPaddControls();
    EXPECT_EQ(paddControlMap.size(), 1);
    EXPECT_TRUE(paddControlMap.contains(controlGenPadd->name()));

    std::this_thread::sleep_for(1000ms);
    spsystem->stop();

    systemThread.join();
    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
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
    spsystem->waitForStart();

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
    auto &paddControlMapBeforeChange = controlExpPadd->getAllPaddControls();

    controlExpPadd->send(ScratchPadd::MessageType::ControlChange{});


    controlGenPadd->waitForWorkCompletion();
    controlExpPadd->waitForWorkCompletion();

    // Check value after change
    auto &paddControlMapAfterChange = controlExpPadd->getAllPaddControls();

    EXPECT_TRUE(paddControlMapAfterChange.contains(controlGenPadd->name()));
    auto &controlMap = paddControlMapAfterChange[controlGenPadd->name()];

    EXPECT_EQ(paddControlMapAfterChange.size(), 1);

    // controlGenPadd->waitForWorkCompletion();
    // controlExpPadd->waitForWorkCompletion();

    std::this_thread::sleep_for(1000ms);
    spsystem->stop();

    systemThread.join();
}
