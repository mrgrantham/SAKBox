#include <gtest/gtest.h>

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

class TestSenderPadd : public ScratchPadd::Base {
public:
  bool shouldUseMainThread() override { return true; }

  TestSenderPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  std::string name() override { return __CLASS_NAME__; }

  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};

class TestReceiverPadd : public ScratchPadd::Base {
public:
  bool shouldUseMainThread() override { return true; }

  TestReceiverPadd(ScratchPadd::System *system) : Base(system) {
    spdlog::info("Constructing: {}", name());
  }

  std::string name() override { return __CLASS_NAME__; }

  std::unordered_map<std::string, ScratchPadd::ControlTypeVariant> generateControls() override {
    return {};
  }
};

class TestSystem : public ScratchPadd::System {
  void instantiate() override { spdlog::info("Test instantiate()"); }
  void start() override { spdlog::info("Test start()"); }
  void stop() override { spdlog::info("Test stop()"); }
  void send(ScratchPadd::Base *sender, ScratchPadd::Message message) override {
    spdlog::info("Test send()");
  }
  void sendIncludeSender(ScratchPadd::Base *sender, ScratchPadd::Message message) override {
    spdlog::info("Test sendIncludeSender()");
  }
  ScratchPadd::PaddAddress paddOfType(std::type_index) override { return {}; }
  ScratchPadd::PaddAddress paddNamed(std::string_view) override { return {}; }
  bool isRunning() override { return true; }
};

// A padd built on __CLASS_NAME__ reports its class name as its name(), with
// no system running (TestSystem is a do-nothing System).
TEST(BaseTest, PaddNamesComeFromClassName) {
  TestSystem testSystem;

  TestSenderPadd testSenderPadd(&testSystem);
  TestReceiverPadd testReceiverPadd(&testSystem);

  // SystemImplementation normally does this; Base's destructor needs the
  // logger that initialize() creates.
  ScratchPadd::PaddTestAccess::initialize(testSenderPadd);
  ScratchPadd::PaddTestAccess::initialize(testReceiverPadd);
  EXPECT_EQ(testSenderPadd.name(), "TestSenderPadd");
  EXPECT_EQ(testReceiverPadd.name(), "TestReceiverPadd");
}

// A system built with SystemBuilder constructs its padds, starts them, and
// stops cleanly.
TEST(BaseTest, SystemStartsAndStops) {
  auto *spsystem =
      dynamic_cast<ScratchPadd::SystemImplementation<TestSenderPadd, TestReceiverPadd> *>(
          ScratchPadd::SystemBuilder<TestSenderPadd, TestReceiverPadd>());
  spsystem->instantiate();

  auto systemThread = std::thread([&] { spsystem->start(); });
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
