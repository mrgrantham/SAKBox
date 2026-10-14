#include <gtest/gtest.h>

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Messages/Message.hpp>

class TestSenderPadd : public ScratchPadd::Base {
    public:

    bool shouldUseMainThread() {
        return true;
    }

    TestSenderPadd(ScratchPadd::System *system) : Base(system) {
        spdlog::info("Constructing: {}", name());
    }

    std::string name() override {
        return __CLASS_NAME__;
    }

    void receive(ScratchPadd::Message message) override {

    }

 ScratchPadd::ControlValueMap
  generateControls() override {
    return {};
  }

};

class TestReceiverPadd : public ScratchPadd::Base {
    public:

    bool shouldUseMainThread() {
        return true;
    }

    TestReceiverPadd(ScratchPadd::System *system) : Base(system) {
        spdlog::info("Constructing: {}", name());
    }

    std::string name() override {
        return __CLASS_NAME__;
    }

    void receive(ScratchPadd::Message message) override {

    }

 std::unordered_map<std::string, ScratchPadd::ControlTypeVariant>
  generateControls() override {
    return {};
  }

};

class TestSystem : public ScratchPadd::System {
    void instantiate() override {
        spdlog::info("Test instantiate()");
    }
    void start() override {
        spdlog::info("Test start()");
    }
    void stop() override {
        spdlog::info("Test stop()");
    }
    void send(ScratchPadd::Base *sender, ScratchPadd::Message message) override {
        spdlog::info("Test send()");
    }
    void sendIncludeSender(ScratchPadd::Message message) override {
        spdlog::info("Test sendIncludeSender()");
    }
    bool isRunning() override {
        return true;
    }
};



// Demonstrate some basic assertions.
TEST(BaseTest, TestSending) {
    TestSystem testSystem;

    TestSenderPadd testSenderPadd(&testSystem);
    TestReceiverPadd testReceiverPadd(&testSystem);

    // SystemImplementation normally does this; Base's destructor needs the
    // logger that initialize() creates.
    testSenderPadd.initialize();
    testReceiverPadd.initialize();
    EXPECT_EQ(testSenderPadd.name(), "TestSenderPadd");
    EXPECT_EQ(testReceiverPadd.name(), "TestReceiverPadd");

}

TEST(BaseTest, TestRecieveWrapper) {
    auto *spsystem =
    dynamic_cast<ScratchPadd::SystemImplementation<TestSenderPadd, TestReceiverPadd>*>(ScratchPadd::SystemBuilder<TestSenderPadd, TestReceiverPadd>());
    spsystem->instantiate();

    auto systemThread = std::thread(
      [&]{
        spsystem->start();
      }
    );
    spsystem->waitForStart();

    spdlog::info("Getting workers");
    auto &workers = spsystem->getWorkers();

    auto &senderPadd = std::get<std::unique_ptr<TestSenderPadd>>(workers);
    auto &receiverPadd = std::get<std::unique_ptr<TestReceiverPadd>>(workers);
    EXPECT_EQ(senderPadd->name(), "TestSenderPadd");
    EXPECT_EQ(receiverPadd->name(), "TestReceiverPadd");

    spsystem->stop();
    systemThread.join();
}
