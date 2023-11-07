#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/DataDependencies.hpp>

#include <gtest/gtest.h>

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
                     std::cout << this->name()
                               << "Control controlName:" << message.controlName
                               << "\n";
                    if (message.paddName == name()) {
                     updateControl(message);
                    }
                   },

                                [&](ScratchPadd::MessageType::ControlRequest &messageContent) {
                     std::cout << name()
                               << "Control request padd name:" << messageContent.paddName.value_or("None")
                               << "\n";
                    send(generateControlsSnapshot());
                   },
                   [&](auto &message) {
                    std::cout << "Other messages" << std::endl;
                   }},
                   
               messageVariant);
  }
std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>  generateControls() override {
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

    public:
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
                     std::cout << name()
                               << "Control request padd name:" << messageContent.paddName.value_or("None")
                               << "\n";
                     setupControlView(messageContent);
                   },
                [&](ScratchPadd::MessageType::ControlChange &messageContent) {
                     std::cout << name()
                               << "Control change padd name:" << messageContent.paddName
                               << "\n";
                     setupControlView(messageContent);
                   },
                    [&](ScratchPadd::MessageType::ControlSnapshot &messageContent) {
                     std::cout << name()
                               << "Control snapshot padd name:" << messageContent.paddName
                               << "\n";
                     setupControlView(messageContent);
                   },
                [&](auto &message) {
                    std::cout << "Other messages" << std::endl;
                   }},
               messageVariant);
  }

  void setupControlView(auto &&controlInfo) {
    std::cout << "Setting up control view from type" << TypeName(controlInfo) << std::endl;
  }

  virtual std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {
    return {};
  }
};

// Demonstrate some basic assertions.
TEST(ControlsTest, BasicAssertions) {
    using namespace std::chrono_literals;

    auto *spsystem =
    dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd>*>(ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

    // ScratchPadd::Data::ConfigureDependencies(argv[0]);
    SCOPED_METHOD_TIMER();
    // signal(SIGINT, signal_handler);
    spdlog::info("Welcome to SCRATCHPADD!");
    spsystem->instantiate();

    auto systemThread = std::thread(
      [&]{
        spsystem->start();
      }
    );
    spsystem->waitForStart();

    spdlog::info("Getting workers");
    auto &workers = spsystem->getWorkers();
    
    auto &controlGenPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);
    auto &controlExpPadd = std::get<std::unique_ptr<ControlGeneratorPadd>>(workers);

    // controlExpPadd->send(ScratchPadd::MessageType::ControlRequest{});

    controlGenPadd->waitForWorkCompletion();
    controlExpPadd->waitForWorkCompletion();

    std::this_thread::sleep_for(1000ms);
    spdlog::info("Ending to SCRATCHPADD!");
    spsystem->stop();

    systemThread.join();
    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
}