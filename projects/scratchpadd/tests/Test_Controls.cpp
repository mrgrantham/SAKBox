#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/DataDependencies.hpp>

#include <gtest/gtest.h>

class ControlGeneratorPadd : public ScratchPadd::Base {
public:
      ControlGeneratorPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", __CLASS_NAME__);
  }
virtual bool runOnMainThread() override { return true; }

  virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [this](ScratchPadd::MessageType::ControlChange &message) {
                     std::cout << this->paddName_
                               << "Control controlName:" << message.controlName
                               << "\n";
                    if (message.paddName == paddName_) {
                     updateControl(message);
                    }
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
             )}};
  }

  void updateControl(ScratchPadd::MessageType::ControlChange &message) {

  }

};

class ControlExposurePadd : public ScratchPadd::Base {

    public:

          ControlExposurePadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", __CLASS_NAME__);
  }
    virtual bool runOnMainThread() override { return true; }

    virtual void starting() override {
        send(ScratchPadd::MessageType::ControlRequest{});
    }

    virtual void receive(ScratchPadd::Message message) override {
    ScratchPadd::MessageVariant &messageVariant = *message.get();
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::ControlRequest &messageContent) {
                     std::cout << paddName_
                               << "Control request padd name:" << messageContent.paddName.value_or("None")
                               << "\n";
                     setupControlView(messageContent);
                   },
                [&](ScratchPadd::MessageType::ControlChange &messageContent) {
                     std::cout << paddName_
                               << "Control change padd name:" << messageContent.paddName
                               << "\n";
                     setupControlView(messageContent);
                   },
                    [&](ScratchPadd::MessageType::ControlSnapshot &messageContent) {
                     std::cout << paddName_
                               << "Control change padd name:" << messageContent.paddName
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

    auto *spsystem =
    dynamic_cast<ScratchPadd::SystemImplementation<ControlGeneratorPadd, ControlExposurePadd>*>(ScratchPadd::SystemBuilder<ControlGeneratorPadd, ControlExposurePadd>());

    // ScratchPadd::Data::ConfigureDependencies(argv[0]);
    SCOPED_METHOD_TIMER();
    // signal(SIGINT, signal_handler);
    spdlog::info("Welcome to SCRATCHPADD!");
    spsystem->instantiate();
    spsystem->start();

    auto &workers = spsystem->getWorkers();


    spsystem->stop();

    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
}