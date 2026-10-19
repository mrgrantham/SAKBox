// The code examples from README.md. If an example changes, update the README
// too. The padds are copied from the README with two test-only changes, marked
// below: a shorter interval, and a way to read what was received.
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

class Ticker : public ScratchPadd::Base {
public:
  Ticker(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Ticker"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // Call repeat() every second, on this padd's thread.
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return 10ms; // test only: the README uses 1s
  }
  void repeat() override {
    send(ScratchPadd::MessageType::Text{"tick " + std::to_string(++ticks_)});
  }

private:
  int ticks_ = 0;
};

class Printer : public ScratchPadd::Base {
public:
  Printer(ScratchPadd::System *system) : Base(system) {
    receive<ScratchPadd::MessageType::Text>([this](const ScratchPadd::MessageType::Text &text) {
      logger().info("received {}", text.contents);
      record(text.contents); // test only
    });
  }
  std::string name() override { return "Printer"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  // test only
  std::vector<std::string> received() {
    std::lock_guard<std::mutex> lk(mutex_);
    return received_;
  }

private:
  void record(const std::string &text) {
    std::lock_guard<std::mutex> lk(mutex_);
    received_.push_back(text);
  }
  std::mutex mutex_;
  std::vector<std::string> received_;
};

// The controls example from the README.
class Mover : public ScratchPadd::Base {
public:
  Mover(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Mover"; }

  ScratchPadd::ControlValueMap generateControls() override {
    return {
        {"speed", ScratchPadd::ControlType::Double(1.0, std::make_pair(0.0, 10.0))},
        {"enabled", ScratchPadd::ControlType::Boolean(true)},
    };
  }

  double speed() {
    return std::get<ScratchPadd::ControlType::Double>(
               generateControlsSnapshot().controlMap.at("speed"))
        .value;
  }
};

template <typename Condition> bool eventually(Condition condition) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (!condition()) {
    if (std::chrono::steady_clock::now() > deadline)
      return false;
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

} // namespace

TEST(ExamplesTest, QuickStartTickerReachesPrinter) {
  ScratchPadd::SystemImplementation<Ticker, Printer> system;
  system.instantiate();
  std::thread mainThread([&] { system.start(); }); // the README's main()
  system.waitForStart();

  auto &printer = *std::get<std::unique_ptr<Printer>>(system.getWorkers());
  EXPECT_TRUE(eventually([&] { return printer.received().size() >= 2; }));

  system.stop();
  mainThread.join();

  const auto received = printer.received();
  ASSERT_GE(received.size(), 2u);
  EXPECT_EQ(received[0], "tick 1");
  EXPECT_EQ(received[1], "tick 2");
}

TEST(ExamplesTest, ControlsHaveDefaultsAndAcceptChanges) {
  ScratchPadd::SystemImplementation<Mover> system;
  system.instantiate();
  auto &mover = *std::get<std::unique_ptr<Mover>>(system.getWorkers());

  EXPECT_DOUBLE_EQ(mover.speed(), 1.0);

  ScratchPadd::MessageType::ControlChange change{
      .paddName = "Mover",
      .controlName = "speed",
      .controlTypeValue = ScratchPadd::ControlType::Double(4.5, std::make_pair(0.0, 10.0))};
  mover.updateControl(change);
  EXPECT_DOUBLE_EQ(mover.speed(), 4.5);

  // A change with the wrong type is rejected.
  change.controlTypeValue = ScratchPadd::ControlType::Boolean(false);
  mover.updateControl(change);
  EXPECT_DOUBLE_EQ(mover.speed(), 4.5);
}
