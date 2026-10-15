#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

// A background padd that keeps a copy of every message it receives.
template <int Id> class RecordingPadd : public ScratchPadd::Base {
public:
  RecordingPadd(ScratchPadd::System *system) : Base(system) {
    work_thread_sleep_interval_ = 1ms;
  }
  std::string name() override { return "RecordingPadd" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void receive(ScratchPadd::Message message) override {
    std::lock_guard<std::mutex> lk(mutex_);
    received_.push_back(*message);
  }

  std::vector<ScratchPadd::MessageVariant> received() {
    std::lock_guard<std::mutex> lk(mutex_);
    return received_;
  }

private:
  std::mutex mutex_;
  std::vector<ScratchPadd::MessageVariant> received_;
};

using Sender = RecordingPadd<1>;
using Receiver = RecordingPadd<2>;

// A padd that only acts on Text messages and ignores everything else.
class TextOnlyPadd : public ScratchPadd::Base {
public:
  TextOnlyPadd(ScratchPadd::System *system) : Base(system) {
    work_thread_sleep_interval_ = 1ms;
  }
  std::string name() override { return "TextOnlyPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void receive(ScratchPadd::Message message) override {
    std::visit(VariantHandler{
                   [&](ScratchPadd::MessageType::Text &text) {
                     std::lock_guard<std::mutex> lk(mutex_);
                     texts_.push_back(text.contents);
                   },
                   [](auto &) {},
               },
               *message);
  }

  std::vector<std::string> texts() {
    std::lock_guard<std::mutex> lk(mutex_);
    return texts_;
  }

private:
  std::mutex mutex_;
  std::vector<std::string> texts_;
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

// A message sent by one padd arrives at the other with its type and contents
// intact, and is not delivered back to the sender.
TEST(MessageTest, TestSendingSimpleMessage) {
  RunningSystem<Sender, Receiver> running;

  ScratchPadd::MessageType::Triangle triangle;
  triangle.position.x = 1.5;
  triangle.vertexes[2].y = -4.0;
  running.padd<Sender>().send(triangle);
  ASSERT_TRUE(running.settles());

  const auto received = running.padd<Receiver>().received();
  ASSERT_EQ(received.size(), 1u);
  ASSERT_TRUE(std::holds_alternative<ScratchPadd::MessageType::Triangle>(received[0]));
  const auto &got = std::get<ScratchPadd::MessageType::Triangle>(received[0]);
  EXPECT_DOUBLE_EQ(got.position.x, 1.5);
  EXPECT_DOUBLE_EQ(got.vertexes[2].y, -4.0);

  EXPECT_TRUE(running.padd<Sender>().received().empty());
}

// A padd that doesn't handle a message type still receives it, ignores it,
// and keeps processing the messages it does handle.
TEST(MessageTest, TestSendingIgnoredMessage) {
  RunningSystem<Sender, TextOnlyPadd> running;
  auto &sender = running.padd<Sender>();

  sender.send(ScratchPadd::MessageType::Point{.x = 1, .y = 2});
  sender.send(ScratchPadd::MessageType::Text{"after the point"});
  ASSERT_TRUE(running.settles());

  EXPECT_EQ(running.padd<TextOnlyPadd>().texts(),
            std::vector<std::string>{"after the point"});
}

// sendIncludeSender() delivers to every padd, including the one that sent it.
TEST(MessageTest, TestSendIncludeSender) {
  RunningSystem<Sender, Receiver> running;

  ScratchPadd::Message message = ScratchPadd::MakeMsg(ScratchPadd::MessageType::Text{"to all"});
  running.padd<Sender>().sendIncludeSender(message);
  ASSERT_TRUE(running.settles());

  EXPECT_EQ(running.padd<Sender>().received().size(), 1u);
  EXPECT_EQ(running.padd<Receiver>().received().size(), 1u);
}
