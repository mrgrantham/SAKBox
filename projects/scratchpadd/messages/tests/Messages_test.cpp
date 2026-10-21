#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

// Sends `contents` from the test's thread as if padd `from` sent them
template <typename T> void sendAs(ScratchPadd::Base &from, T contents) {
  ScratchPadd::PaddTestAccess::send(from, std::move(contents));
}

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
    receive<Ping>([this](const Ping &) { ++pings_; });
  }
  std::string name() override { return "PingOnlyPadd"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::atomic<int> pings_{0};
};

// A padd that counts how often its name() is called, and handles Ping.
template <int Id> class NameCountingPadd : public ScratchPadd::Base {
public:
  NameCountingPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &) { ++pings_; });
  }
  std::string name() override {
    ++nameCalls_;
    return "NameCountingPadd" + std::to_string(Id);
  }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::atomic<int> nameCalls_{0};
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

  sendAs(running.padd<Sender>(), Shape{.name = "triangle", .sides = {3, 4, 5}});
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

  sendAs(sender, Ping{1});
  sendAs(sender, Shape{.name = "square"});
  sendAs(sender, Ping{2});
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

  EXPECT_FALSE(ScratchPadd::PaddTestAccess::handles(pingOnly, typeid(Shape)));
  sendAs(sender, Shape{.name = "ignored"});
  sendAs(sender, Unhandled{7});
  EXPECT_EQ(pingOnly.pendingWorkCount(), 0); // not even queued

  sendAs(sender, Ping{3});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(pingOnly.pings_, 1);
}

// sendIncludeSender() reaches the sender as well as the other padds.
TEST(MessageTest, TestSendIncludeSender) {
  RunningSystem<Sender, Receiver> running;

  ScratchPadd::PaddTestAccess::sendIncludeSender(running.padd<Sender>(), Ping{9});
  ASSERT_TRUE(running.settles());

  EXPECT_EQ(running.padd<Sender>().pings(), std::vector<int>{9});
  EXPECT_EQ(running.padd<Receiver>().pings(), std::vector<int>{9});
}

// Every receiver gets the same immutable object rather than a copy.
TEST(MessageTest, TestReceiversShareOneMessage) {
  RunningSystem<Sender, Receiver, OtherReceiver> running;

  sendAs(running.padd<Sender>(), Shape{.name = "shared"});
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

// A padd's name() is read once, when the system starts it, and cached: the
// system used to call it again for every message sent and delivered (for
// debug logs, whose arguments are built even when debug logging is off),
// and a name() built on __CLASS_NAME__ demangles its class name each call.
TEST(MessageTest, NameIsReadOnceNotPerMessage) {
  using A = NameCountingPadd<1>;
  using B = NameCountingPadd<2>;
  RunningSystem<A, B> running;
  auto &a = running.padd<A>();
  auto &b = running.padd<B>();
  const int aCallsAtStart = a.nameCalls_, bCallsAtStart = b.nameCalls_;
  EXPECT_EQ(aCallsAtStart, 1);
  EXPECT_EQ(bCallsAtStart, 1);
  EXPECT_EQ(a.paddName(), "NameCountingPadd1");

  for (int i = 0; i < 50; i++) {
    sendAs(a, Ping{i});
    sendAs(b, Ping{i});
  }
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(a.pings_, 50);
  EXPECT_EQ(b.pings_, 50);
  EXPECT_EQ(a.nameCalls_, aCallsAtStart);
  EXPECT_EQ(b.nameCalls_, bCallsAtStart);
}

// A message's type name is worked out once per type: Base::send logs it for
// every message, and it used to be demangled each time.
TEST(MessageTest, TypeNameIsWorkedOutOncePerType) {
  const ScratchPadd::Message first = ScratchPadd::makeMessage(Ping{1});
  const ScratchPadd::Message second = ScratchPadd::makeMessage(Ping{2});
  EXPECT_EQ(first->typeName(), "(anonymous namespace)::Ping");
  EXPECT_EQ(&first->typeName(), &second->typeName()); // the same string
  EXPECT_EQ(ScratchPadd::makeMessage(Shape{})->typeName(), "(anonymous namespace)::Shape");
}

// A running padd can start and stop handling a type (receive() and
// stopReceiving()), from its own thread, including from inside a handler.
namespace {
// A padd that starts out handling only Ping, and records what it gets.
class ChangingPadd : public ScratchPadd::Base {
public:
  ChangingPadd(ScratchPadd::System *system) : Base(system) { receivePings(); }
  std::string name() override { return "ChangingPadd"; }

  // Test only: so the tests can change its handlers (on its own thread)
  using Base::receive;
  using Base::stopReceiving;
  ScratchPadd::ControlValueMap generateControls() override { return {}; }

  void receivePings() {
    receive<Ping>([this](const Ping &ping) { record("ping " + std::to_string(ping.id)); });
  }
  void receiveShapes() {
    receive<Shape>([this](const Shape &shape) { record("shape " + shape.name); });
  }
  void record(const std::string &entry) {
    std::lock_guard<std::mutex> lk(mutex_);
    received_.push_back(entry);
  }
  std::vector<std::string> received() {
    std::lock_guard<std::mutex> lk(mutex_);
    return received_;
  }

private:
  std::mutex mutex_;
  std::vector<std::string> received_;
};

// Runs `work` on `padd`'s own thread and waits for the system to settle
template <typename Padd, typename... Padds>
void onPaddThread(RunningSystem<Padds...> &running, const std::function<void(Padd &)> &work) {
  Padd &padd = running.template padd<Padd>();
  ScratchPadd::PaddTestAccess::push(padd, [&padd, work] { work(padd); });
  ASSERT_TRUE(running.settles());
}
} // namespace

TEST(ChangingHandlersTest, StartReceivingWhileRunning) {
  RunningSystem<Sender, ChangingPadd> running;
  auto &sender = running.padd<Sender>();
  auto &changing = running.padd<ChangingPadd>();

  sendAs(sender, Shape{.name = "early"}); // not handled yet, so never queued
  onPaddThread<ChangingPadd>(running, [](ChangingPadd &padd) { padd.receiveShapes(); });
  EXPECT_TRUE(ScratchPadd::PaddTestAccess::handles(changing, typeid(Shape)));
  sendAs(sender, Shape{.name = "late"});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(changing.received(), std::vector<std::string>{"shape late"});
}

TEST(ChangingHandlersTest, StopReceivingWhileRunning) {
  RunningSystem<Sender, ChangingPadd> running;
  auto &sender = running.padd<Sender>();
  auto &changing = running.padd<ChangingPadd>();

  sendAs(sender, Ping{1});
  ASSERT_TRUE(running.settles());
  bool stopped = false, stoppedAgain = true;
  onPaddThread<ChangingPadd>(running, [&](ChangingPadd &padd) {
    stopped = padd.stopReceiving<Ping>();
    stoppedAgain = padd.stopReceiving<Ping>();
  });
  EXPECT_TRUE(stopped);
  EXPECT_FALSE(stoppedAgain); // there was nothing left to remove
  EXPECT_FALSE(ScratchPadd::PaddTestAccess::handles(changing, typeid(Ping)));

  sendAs(sender, Ping{2});
  EXPECT_EQ(changing.pendingWorkCount(), 0); // not even queued
  onPaddThread<ChangingPadd>(running, [](ChangingPadd &padd) { padd.receivePings(); });
  sendAs(sender, Ping{3});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(changing.received(), (std::vector<std::string>{"ping 1", "ping 3"}));
}

// A handler can remove itself while it runs. Pings queued behind it are
// dropped when their turn comes, rather than run by a handler that's gone.
TEST(ChangingHandlersTest, HandlerCanStopItsOwnType) {
  RunningSystem<Sender, ChangingPadd> running;
  auto &changing = running.padd<ChangingPadd>();
  onPaddThread<ChangingPadd>(running, [](ChangingPadd &padd) {
    padd.receive<Ping>([&padd](const Ping &ping) {
      padd.record("last ping " + std::to_string(ping.id));
      padd.stopReceiving<Ping>(); // destroys this handler's stored copy
    });
  });
  // Queue three behind a job that holds the padd's thread, so all three are
  // queued before the first one runs
  std::promise<void> release;
  ScratchPadd::PaddTestAccess::push(changing,
                                    [gate = release.get_future().share()] { gate.wait(); });
  for (int id = 1; id <= 3; id++)
    sendAs(running.padd<Sender>(), Ping{id});
  EXPECT_EQ(changing.pendingWorkCount(), 4);
  release.set_value();
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(changing.received(), std::vector<std::string>{"last ping 1"});
}

// A handler can replace itself; the next message goes to the replacement.
TEST(ChangingHandlersTest, HandlerCanReplaceItself) {
  RunningSystem<Sender, ChangingPadd> running;
  onPaddThread<ChangingPadd>(running, [](ChangingPadd &padd) {
    padd.receive<Ping>([&padd](const Ping &ping) {
      padd.record("first " + std::to_string(ping.id));
      padd.receivePings();
    });
  });
  for (int id = 1; id <= 3; id++)
    sendAs(running.padd<Sender>(), Ping{id});
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<ChangingPadd>().received(),
            (std::vector<std::string>{"first 1", "ping 2", "ping 3"}));
}

// Senders check handles() on their own threads while the padd changes its
// handlers on its own; under ThreadSanitizer this checks that's safe. Each
// Ping is received at most once, in the order sent.
TEST(ChangingHandlersTest, SendingWhileHandlersChange) {
  RunningSystem<Sender, ChangingPadd> running;
  auto &changing = running.padd<ChangingPadd>();
  std::thread toggler([&] {
    for (int i = 0; i < 200; i++) {
      ScratchPadd::PaddTestAccess::push(changing, [&changing, i] {
        if (i % 2 == 0)
          changing.stopReceiving<Ping>();
        else
          changing.receivePings();
      });
    }
  });
  for (int id = 0; id < 2000; id++)
    sendAs(running.padd<Sender>(), Ping{id});
  toggler.join();
  ASSERT_TRUE(running.settles());

  int last = -1;
  for (const std::string &entry : changing.received()) {
    const int id = std::stoi(entry.substr(5)); // after "ping "
    EXPECT_GT(id, last);
    last = id;
  }
}

// ---------------------------------------------------------------------------
// Sending to one padd (sendTo), and knowing who sent a message

namespace {
using Access = ScratchPadd::PaddTestAccess;

struct Pong {
  int id;
};

// Answers each Ping with a Pong, straight to whoever sent it
class ReplierPadd : public ScratchPadd::Base {
public:
  ReplierPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>(
        [this](const Ping &ping, ScratchPadd::PaddAddress from) { sendTo(from, Pong{ping.id}); });
  }
  std::string name() override { return "Replier"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};

// Records the Pongs it gets, with who sent each, and who sent each Ping
class AskerPadd : public ScratchPadd::Base {
public:
  AskerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Pong>([this](const Pong &pong, ScratchPadd::PaddAddress from) {
      std::lock_guard<std::mutex> lk(mutex_);
      pongs_.push_back(pong.id);
      pongSenders_.push_back(from.name());
    });
    receive<Ping>([this](const Ping &, ScratchPadd::PaddAddress from) {
      std::lock_guard<std::mutex> lk(mutex_);
      pingFromSomeone_.push_back(static_cast<bool>(from));
    });
  }
  std::string name() override { return "Asker"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::vector<int> pongs() {
    std::lock_guard<std::mutex> lk(mutex_);
    return pongs_;
  }
  std::vector<std::string> pongSenders() {
    std::lock_guard<std::mutex> lk(mutex_);
    return pongSenders_;
  }
  std::vector<bool> pingFromSomeone() {
    std::lock_guard<std::mutex> lk(mutex_);
    return pingFromSomeone_;
  }

private:
  std::mutex mutex_;
  std::vector<int> pongs_;
  std::vector<std::string> pongSenders_;
  std::vector<bool> pingFromSomeone_;
};
} // namespace

// sendTo by class reaches that padd only: not the others that handle the
// type, and not the sender.
TEST(SendToTest, ReachesOnlyThatPadd) {
  RunningSystem<Sender, Receiver, OtherReceiver> running;
  auto &sender = running.padd<Sender>();
  EXPECT_TRUE(Access::sendTo(sender, Access::addressOf<Receiver>(sender), Ping{1}));
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<Receiver>().pings(), std::vector<int>{1});
  EXPECT_TRUE(running.padd<OtherReceiver>().pings().empty());
  EXPECT_TRUE(sender.pings().empty());
}

// By name, as by class; and a padd can send to itself
TEST(SendToTest, ByNameAndToItself) {
  RunningSystem<Sender, Receiver> running;
  auto &sender = running.padd<Sender>();
  const ScratchPadd::PaddAddress receiver = Access::addressOf(sender, "RecordingPadd2");
  EXPECT_EQ(receiver, Access::addressOf<Receiver>(sender));
  EXPECT_EQ(receiver.name(), "RecordingPadd2");
  EXPECT_TRUE(Access::sendTo(sender, receiver, Ping{2}));
  EXPECT_TRUE(Access::sendTo(sender, Access::address(sender), Ping{3}));
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<Receiver>().pings(), std::vector<int>{2});
  EXPECT_EQ(sender.pings(), std::vector<int>{3});
}

// Messages from one padd to another arrive in the order they were sent.
TEST(SendToTest, KeepsOrder) {
  RunningSystem<Sender, Receiver> running;
  auto &sender = running.padd<Sender>();
  const auto receiver = Access::addressOf<Receiver>(sender);
  std::vector<int> sent;
  for (int i = 0; i < 200; ++i) {
    Access::sendTo(sender, receiver, Ping{i});
    sent.push_back(i);
  }
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<Receiver>().pings(), sent);
}

// Sending to a padd that isn't there, to an empty address, or a type the
// receiver doesn't handle, is refused (and logged); nothing is queued.
TEST(SendToTest, RefusesWhatCantBeDelivered) {
  RunningSystem<Sender, PingOnlyPadd> running;
  auto &sender = running.padd<Sender>();
  EXPECT_FALSE(Access::addressOf<Receiver>(sender)); // not in this system
  EXPECT_FALSE(Access::addressOf(sender, "Nobody"));
  EXPECT_EQ(ScratchPadd::PaddAddress().name(), "(no padd)");
  EXPECT_FALSE(Access::sendTo(sender, Access::addressOf<Receiver>(sender), Ping{1}));
  EXPECT_FALSE(Access::sendTo(sender, ScratchPadd::PaddAddress(), Ping{1}));
  auto &pingOnly = running.padd<PingOnlyPadd>();
  EXPECT_FALSE(Access::sendTo(sender, Access::addressOf<PingOnlyPadd>(sender), Shape{}));
  EXPECT_EQ(pingOnly.pendingWorkCount(), 0);
}

// A handler that takes the sender can reply to it; the reply's sender is
// the replier.
TEST(SenderTest, HandlersCanReplyToTheSender) {
  RunningSystem<AskerPadd, ReplierPadd> running;
  auto &asker = running.padd<AskerPadd>();
  sendAs(asker, Ping{7}); // broadcast; only the replier handles Ping besides the asker
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(asker.pongs(), std::vector<int>{7});
  EXPECT_EQ(asker.pongSenders(), std::vector<std::string>{"Replier"});
}

namespace {
// Answers each Ping with reply(), keeping whether each answer was sent
class PoliteReplierPadd : public ScratchPadd::Base {
public:
  PoliteReplierPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &ping, ScratchPadd::PaddAddress from) {
      std::lock_guard<std::mutex> lk(mutex_);
      sent_.push_back(reply(from, Pong{ping.id}));
    });
  }
  std::string name() override { return "PoliteReplier"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  std::vector<bool> sent() {
    std::lock_guard<std::mutex> lk(mutex_);
    return sent_;
  }

private:
  std::mutex mutex_;
  std::vector<bool> sent_;
};
} // namespace

// reply() answers a sender that listens for the answer, and quietly sends
// nothing to one that doesn't (an asker that doesn't handle Pong) or to no
// padd at all
TEST(SenderTest, ReplyOnlyReachesSendersThatListen) {
  RunningSystem<AskerPadd, PoliteReplierPadd, Sender> running;
  sendAs(running.padd<AskerPadd>(), Ping{1});
  ASSERT_TRUE(running.settles());
  Access::sendTo(running.padd<Sender>(),
                 Access::addressOf<PoliteReplierPadd>(running.padd<Sender>()), Ping{2});
  ASSERT_TRUE(running.settles());
  running.system.sendIncludeSender(nullptr, ScratchPadd::makeMessage(Ping{3}));
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<AskerPadd>().pongs(), std::vector<int>{1});
  EXPECT_EQ(running.padd<PoliteReplierPadd>().sent(), (std::vector<bool>{true, false, false}));
}

// A message from outside any padd (the system's own sendIncludeSender with
// no sender) arrives with an empty sender.
TEST(SenderTest, MessagesFromOutsideHaveNoSender) {
  RunningSystem<AskerPadd> running;
  running.system.sendIncludeSender(nullptr, ScratchPadd::makeMessage(Ping{1}));
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.padd<AskerPadd>().pingFromSomeone(), std::vector<bool>{false});
}

// ---------------------------------------------------------------------------
// Padds only talk through messages: code outside a padd (another padd
// included) can't call into one. These are checked when the test compiles:
// each `requires` is false because the member isn't accessible here.

namespace {
template <typename P> constexpr bool canSend = requires(P &p) { p.send(Ping{}); };
template <typename P>
constexpr bool canSendTo = requires(P &p) { p.sendTo(ScratchPadd::PaddAddress(), Ping{}); };
template <typename P>
constexpr bool canReply = requires(P &p) { p.reply(ScratchPadd::PaddAddress(), Ping{}); };
template <typename P> constexpr bool canPush = requires(P &p) { p.push([] {}); };
template <typename P>
constexpr bool canDeliver =
    requires(P &p) { p.deliver(ScratchPadd::makeMessage(Ping{}), ScratchPadd::PaddAddress()); };
template <typename P>
constexpr bool canSetControl =
    requires(P &p) { p.setControl("x", ScratchPadd::ControlType::Boolean(true)); };
template <typename P>
constexpr bool canReadControl =
    requires(P &p) { p.template control<ScratchPadd::ControlType::Boolean>("x"); };
template <typename P>
constexpr bool canReceive = requires(P &p) { p.template receive<Ping>([](const Ping &) {}); };
template <typename P> constexpr bool canStop = requires(P &p) { p.stop(); };
template <typename P> constexpr bool canReadName = requires(P &p) { p.paddName(); };
template <typename A> constexpr bool canOpenAddress = requires(A &a) { a.padd_; };
} // namespace

TEST(EnforcementTest, OutsideCodeCantReachIntoAPadd) {
  using Padd = RecordingPadd<1>;
  static_assert(canReadName<Padd>, "public state can be read (so the checks below can pass)");
  static_assert(!canSend<Padd> && !canSendTo<Padd> && !canReply<Padd>,
                "only a padd sends its own messages");
  static_assert(!canPush<Padd>, "only a padd queues its own work");
  static_assert(!canDeliver<Padd> && !canStop<Padd>, "only the system runs a padd");
  static_assert(!canSetControl<Padd> && !canReadControl<Padd>, "a padd's controls are its own");
  static_assert(!canReceive<Padd>, "only a padd chooses what it receives");
  static_assert(!canOpenAddress<ScratchPadd::PaddAddress>, "an address can't be opened");
  SUCCEED();
}

// Many senders using sendTo on one padd at once, from their own threads:
// every message arrives exactly once, in the order its sender sent it, and
// with the right sender. (Run under ThreadSanitizer too.)
namespace {
// Records each Ping with the name of the padd that sent it
class CollectorPadd : public ScratchPadd::Base {
public:
  CollectorPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &ping, ScratchPadd::PaddAddress from) {
      received_[from.name()].push_back(ping.id); // only on this padd's thread
    });
  }
  std::string name() override { return "Collector"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  // What arrived from each sender, read on this padd's thread
  std::map<std::string, std::vector<int>> received() {
    return ScratchPadd::onPaddThread(*this, [](CollectorPadd &self) { return self.received_; });
  }

private:
  std::map<std::string, std::vector<int>> received_;
};
} // namespace

TEST(SendToTest, ConcurrentSendersLoseNothing) {
  using SenderA = RecordingPadd<1>;
  using SenderB = RecordingPadd<2>;
  using SenderC = RecordingPadd<3>;
  using SenderD = RecordingPadd<4>;
  RunningSystem<SenderA, SenderB, SenderC, SenderD, CollectorPadd> running;
  std::vector<ScratchPadd::Base *> senders = {&running.padd<SenderA>(), &running.padd<SenderB>(),
                                              &running.padd<SenderC>(), &running.padd<SenderD>()};
  constexpr int kMessages = 1000;
  std::atomic<int> refused = 0;
  std::vector<std::thread> threads;
  threads.reserve(senders.size());
  for (size_t s = 0; s < senders.size(); ++s) {
    threads.emplace_back([&, s] {
      ScratchPadd::Base &sender = *senders[s];
      const ScratchPadd::PaddAddress collector = Access::addressOf<CollectorPadd>(sender);
      for (int i = 0; i < kMessages; ++i) {
        // Half by the kept address, half by class
        const bool sent =
            s % 2 == 0 ? Access::sendTo(sender, collector, Ping{i})
                       : Access::sendTo(sender, Access::addressOf<CollectorPadd>(sender), Ping{i});
        if (!sent)
          ++refused;
      }
    });
  }
  for (std::thread &thread : threads)
    thread.join();
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(refused, 0);

  const auto received = running.padd<CollectorPadd>().received();
  ASSERT_EQ(received.size(), senders.size());
  std::vector<int> inOrder(kMessages);
  for (int i = 0; i < kMessages; ++i)
    inOrder[i] = i;
  for (const ScratchPadd::Base *sender : senders) {
    ASSERT_TRUE(received.contains(sender->paddName())) << sender->paddName();
    EXPECT_EQ(received.at(sender->paddName()), inOrder) << sender->paddName();
  }
}

// A broadcast no padd handles is noted (and warned about) once per type,
// rather than dropped silently. A message only its sender handles counts:
// send() goes to every other padd.
TEST(UnhandledTest, BroadcastsNobodyHandlesAreNotedOncePerType) {
  RunningSystem<Sender, PingOnlyPadd> running;
  auto &sender = running.padd<Sender>();
  sendAs(sender, Unhandled{1});
  sendAs(sender, Unhandled{2});                                    // same type: no second note
  sendAs(sender, Ping{3});                                         // PingOnlyPadd handles it
  sendAs(sender, Shape{});                                         // only the sender handles Shape
  ScratchPadd::PaddTestAccess::sendIncludeSender(sender, Shape{}); // the sender gets this one
  ASSERT_TRUE(running.settles());
  EXPECT_EQ(running.system.unhandledMessageTypes(),
            (std::vector<std::string>{"(anonymous namespace)::Unhandled",
                                      "(anonymous namespace)::Shape"}));
  EXPECT_EQ(running.padd<PingOnlyPadd>().pings_, 1);
}
