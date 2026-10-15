#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

// Message types defined by the test, not the library.
struct Shape {
  std::string name;
  std::array<double, 3> sides;
};
struct Ping {
  int id;
};
struct Unhandled {
  int id;
};

// A background padd that records the Shapes and Pings it receives, and the
// address of each Shape (to check that receivers share one object).
template <int Id> class RecordingPadd : public ScratchPadd::Base {
public:
  RecordingPadd(ScratchPadd::System *system) : Base(system) {
    work_thread_sleep_interval_ = 1ms;
    receive<Shape>([this](const Shape &shape) {
      std::lock_guard<std::mutex> lk(mutex_);
      shapes_.push_back(shape);
      shapeAddresses_.push_back(&shape);
    });
    receive<Ping>([this](const Ping &ping) {
      std::lock_guard<std::mutex> lk(mutex_);
      pings_.push_back(ping.id);
    });
  }
  std::string name() override { return "RecordingPadd" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  std::vector<Shape> shapes() {
    std::lock_guard<std::mutex> lk(mutex_);
    return shapes_;
  }
  std::vector<const Shape *> shapeAddresses() {
    std::lock_guard<std::mutex> lk(mutex_);
    return shapeAddresses_;
  }
  std::vector<int> pings() {
    std::lock_guard<std::mutex> lk(mutex_);
    return pings_;
  }

private:
  std::mutex mutex_;
  std::vector<Shape> shapes_;
  std::vector<const Shape *> shapeAddresses_;
  std::vector<int> pings_;
};

using Sender = RecordingPadd<1>;
using Receiver = RecordingPadd<2>;
using OtherReceiver = RecordingPadd<3>;

// A padd that only handles Ping.
class PingOnlyPadd : public ScratchPadd::Base {
public:
  PingOnlyPadd(ScratchPadd::System *system) : Base(system) {
    work_thread_sleep_interval_ = 1ms;
    receive<Ping>([this](const Ping &) { ++pings_; });
  }
  std::string name() override { return "PingOnlyPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::atomic<int> pings_{0};
};

template <typename... Padds> class RunningSystem {
public:
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread mainThread;

  RunningSystem() {
    system.instantiate();
    mainThread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~RunningSystem() {
    system.stop();
    mainThread.join();
  }

  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }

  // True once every padd has worked through its queue, within two seconds.
  bool settles() {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while ((padd<Padds>().pendingWorkCount() + ...) != 0) {
      if (std::chrono::steady_clock::now() > deadline)
        return false;
      std::this_thread::sleep_for(1ms);
    }
    return true;
  }
};

} // namespace

// An app-defined message arrives with its contents intact, and is not
// delivered back to the sender.
TEST(MessageTest, TestSendingSimpleMessage) {
  RunningSystem<Sender, Receiver> running;

  running.padd<Sender>().send(Shape{.name = "triangle", .sides = {3, 4, 5}});
  ASSERT_TRUE(running.settles());

  const auto shapes = running.padd<Receiver>().shapes();
  ASSERT_EQ(shapes.size(), 1u);
  EXPECT_EQ(shapes[0].name, "triangle");
  EXPECT_EQ(shapes[0].sides[2], 5);
  EXPECT_TRUE(running.padd<Sender>().shapes().empty());
}

// Each message type goes to the handler registered for it.
TEST(MessageTest, TestMessagesReachTheirOwnHandlers) {
  RunningSystem<Sender, Receiver> running;
  auto &sender = running.padd<Sender>();

  sender.send(Ping{1});
  sender.send(Shape{.name = "square"});
  sender.send(Ping{2});
  ASSERT_TRUE(running.settles());

  EXPECT_EQ(running.padd<Receiver>().pings(), (std::vector<int>{1, 2}));
  ASSERT_EQ(running.padd<Receiver>().shapes().size(), 1u);
  EXPECT_EQ(running.padd<Receiver>().shapes()[0].name, "square");
}

// A padd with no handler for a type is never sent it: nothing is queued, and
// the messages it does handle still arrive.
TEST(MessageTest, TestSendingIgnoredMessage) {
  RunningSystem<Sender, PingOnlyPadd> running;
  auto &sender = running.padd<Sender>();
  auto &pingOnly = running.padd<PingOnlyPadd>();

  EXPECT_FALSE(pingOnly.handles(typeid(Shape)));
  sender.send(Shape{.name = "ignored"});
  sender.send(Unhandled{7});
  EXPECT_EQ(pingOnly.pendingWorkCount(), 0); // not even queued

  sender.send(Ping{3});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(pingOnly.pings_, 1);
}

// sendIncludeSender() reaches the sender as well as the other padds.
TEST(MessageTest, TestSendIncludeSender) {
  RunningSystem<Sender, Receiver> running;

  running.padd<Sender>().sendIncludeSender(Ping{9});
  ASSERT_TRUE(running.settles());

  EXPECT_EQ(running.padd<Sender>().pings(), std::vector<int>{9});
  EXPECT_EQ(running.padd<Receiver>().pings(), std::vector<int>{9});
}

// Every receiver gets the same immutable object rather than a copy.
TEST(MessageTest, TestReceiversShareOneMessage) {
  RunningSystem<Sender, Receiver, OtherReceiver> running;

  running.padd<Sender>().send(Shape{.name = "shared"});
  ASSERT_TRUE(running.settles());

  const auto first = running.padd<Receiver>().shapeAddresses();
  const auto second = running.padd<OtherReceiver>().shapeAddresses();
  ASSERT_EQ(first.size(), 1u);
  ASSERT_EQ(second.size(), 1u);
  EXPECT_EQ(first[0], second[0]);
}

// makeMessage() and messageAs() wrap and unwrap contents by type.
TEST(MessageTest, TestMakeMessageAndMessageAs) {
  ScratchPadd::Message message = ScratchPadd::makeMessage(Ping{4});
  EXPECT_EQ(message->type(), std::type_index(typeid(Ping)));
  ASSERT_NE(ScratchPadd::messageAs<Ping>(message), nullptr);
  EXPECT_EQ(ScratchPadd::messageAs<Ping>(message)->id, 4);
  EXPECT_EQ(ScratchPadd::messageAs<Shape>(message), nullptr);
}
