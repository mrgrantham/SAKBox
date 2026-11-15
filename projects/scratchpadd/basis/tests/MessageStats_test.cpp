// MessageStats: what a system's messages cost. Off until turned on (nothing
// counted); on, messages and jobs by type (a message reaching two padds is
// one message, two jobs), sendTo and reply counted too, and each padd's
// handled count, time and deepest queue. MessageStatsPadd turns it on by
// being listed, its Counting switch turns it off and on, and its tab shows
// the busiest types and padds as tables.
#include <ScratchPadd/MessageStatsPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <thread>

namespace {

using ScratchPadd::MessageStats;

struct Ping {
  int n = 0;
};
struct Pong {};

class SenderPadd : public ScratchPadd::Base {
public:
  explicit SenderPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Sender"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};
template <int Id> class ListenerPadd : public ScratchPadd::Base {
public:
  explicit ListenerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Ping>([this](const Ping &, ScratchPadd::PaddAddress from) { reply(from, Pong{}); });
  }
  std::string name() override { return "Listener" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
};
class PongCounterPadd : public SenderPadd {
public:
  explicit PongCounterPadd(ScratchPadd::System *system) : SenderPadd(system) {
    receive<Pong>([](const Pong &) {});
  }
  std::string name() override { return "PongCounter"; }
};

template <typename... Padds> struct Running {
  ScratchPadd::SystemImplementation<Padds...> system;
  std::thread thread;
  Running() {
    system.instantiate();
    thread = std::thread([this] { system.start(); });
    system.waitForStart();
  }
  ~Running() {
    system.stop();
    thread.join();
  }
  template <typename Padd> Padd &padd() {
    return *std::get<std::unique_ptr<Padd>>(system.getWorkers());
  }
  void settle() {
    std::apply([](auto &...p) { (p->waitForWorkCompletion(), ...); }, system.getWorkers());
  }
};

const MessageStats::TypeCounts *typeNamed(const MessageStats::Snapshot &s,
                                          const std::string &part) {
  auto found = std::ranges::find_if(
      s.types, [&](const auto &t) { return t.name.find(part) != std::string::npos; });
  return found == s.types.end() ? nullptr : &*found;
}
const MessageStats::PaddCounts *paddNamed(const MessageStats::Snapshot &s,
                                          const std::string &name) {
  auto found = std::ranges::find(s.padds, name, &MessageStats::PaddCounts::name);
  return found == s.padds.end() ? nullptr : &*found;
}

} // namespace

TEST(MessageStatsTest, CountsNothingUntilTurnedOn) {
  Running<PongCounterPadd, ListenerPadd<1>> app;
  for (int i = 0; i < 5; ++i)
    ScratchPadd::PaddTestAccess::send(app.padd<PongCounterPadd>(), Ping{i});
  app.settle();
  const MessageStats::Snapshot snapshot = app.system.messageStats().take();
  EXPECT_TRUE(snapshot.types.empty());
  EXPECT_TRUE(snapshot.padds.empty());
}

TEST(MessageStatsTest, CountsMessagesJobsAndPadds) {
  Running<PongCounterPadd, ListenerPadd<1>, ListenerPadd<2>> app;
  app.system.messageStats().setEnabled(true);
  for (int i = 0; i < 10; ++i)
    ScratchPadd::PaddTestAccess::send(app.padd<PongCounterPadd>(), Ping{i});
  app.settle();
  app.settle(); // (the replies)
  const MessageStats::Snapshot snapshot = app.system.messageStats().take();
  const MessageStats::TypeCounts *ping = typeNamed(snapshot, "Ping");
  ASSERT_NE(ping, nullptr);
  EXPECT_EQ(ping->messages, 10u); // one message each...
  EXPECT_EQ(ping->jobs, 20u);     // ...reaching two listeners
  const MessageStats::TypeCounts *pong = typeNamed(snapshot, "Pong");
  ASSERT_NE(pong, nullptr);
  EXPECT_EQ(pong->messages, 20u); // a reply from each listener
  EXPECT_EQ(pong->jobs, 20u);
  EXPECT_EQ(snapshot.types.front().jobs, 20u); // (busiest first)
  EXPECT_EQ(ping->size, sizeof(Ping));
  EXPECT_TRUE(ping->triviallyCopyable);
  const MessageStats::PaddCounts *listener = paddNamed(snapshot, "Listener1");
  ASSERT_NE(listener, nullptr);
  EXPECT_EQ(listener->handled, 10u);
  EXPECT_GE(listener->deepestQueue, 1);
  EXPECT_GE(listener->longest.count(), 0);
  EXPECT_GT(snapshot.over.count(), 0);
  // take() starts again
  EXPECT_TRUE(app.system.messageStats().take().types.empty());
}

TEST(MessageStatsTest, CountsSendTo) {
  Running<PongCounterPadd, ListenerPadd<1>, ListenerPadd<2>> app;
  app.system.messageStats().setEnabled(true);
  PongCounterPadd &sender = app.padd<PongCounterPadd>();
  ScratchPadd::PaddTestAccess::sendTo(
      sender, ScratchPadd::PaddTestAccess::addressOf<ListenerPadd<2>>(sender), Ping{});
  app.settle();
  app.settle();
  const MessageStats::Snapshot snapshot = app.system.messageStats().take();
  const MessageStats::TypeCounts *ping = typeNamed(snapshot, "Ping");
  ASSERT_NE(ping, nullptr);
  EXPECT_EQ(ping->messages, 1u);
  EXPECT_EQ(ping->jobs, 1u);
  EXPECT_EQ(paddNamed(snapshot, "Listener1"), nullptr);
}

TEST(MessageStatsPaddTest, TurnsCountingOnAndShowsTables) {
  Running<ScratchPadd::MessageStatsPadd, PongCounterPadd, ListenerPadd<1>> app;
  EXPECT_TRUE(app.system.messageStats().enabled()); // (by being listed)
  for (int i = 0; i < 3; ++i)
    ScratchPadd::PaddTestAccess::send(app.padd<PongCounterPadd>(), Ping{i});
  app.settle();
  app.settle();
  auto &stats = app.padd<ScratchPadd::MessageStatsPadd>();
  const std::string types = ScratchPadd::onPaddThread(stats, [](ScratchPadd::MessageStatsPadd &p) {
    p.repeat();
    return ScratchPadd::PaddTestAccess::control<ScratchPadd::ControlType::Info>(p, "Busiest types")
        .value;
  });
  EXPECT_EQ(types.rfind("Type\tPer s\tJobs per s\tBytes\n", 0), 0u) << types;
  EXPECT_NE(types.find("Ping"), std::string::npos) << types;
  const std::string padds = ScratchPadd::onPaddThread(stats, [](ScratchPadd::MessageStatsPadd &p) {
    return ScratchPadd::PaddTestAccess::control<ScratchPadd::ControlType::Info>(p, "Busiest padds")
        .value;
  });
  EXPECT_NE(padds.find("Listener1"), std::string::npos) << padds;

  // Its switch: off stops the counting, on starts it again
  // (as a panel would: a ControlChange sent to it)
  PongCounterPadd &panel = app.padd<PongCounterPadd>();
  auto toStats = ScratchPadd::PaddTestAccess::addressOf<ScratchPadd::MessageStatsPadd>(panel);
  ScratchPadd::PaddTestAccess::sendTo(
      panel, toStats,
      ScratchPadd::MessageType::ControlChange{
          .controlName = "Counting", .controlTypeValue = ScratchPadd::ControlType::Boolean(false)});
  app.settle();
  EXPECT_FALSE(app.system.messageStats().enabled());
  ScratchPadd::PaddTestAccess::sendTo(
      panel, toStats,
      ScratchPadd::MessageType::ControlChange{
          .controlName = "Counting", .controlTypeValue = ScratchPadd::ControlType::Boolean(true)});
  app.settle();
  EXPECT_TRUE(app.system.messageStats().enabled());
}

TEST(MessageStatsPaddTest, MarksBusyTypesAsCandidates) {
  MessageStats::Snapshot snapshot;
  snapshot.over = std::chrono::seconds(1);
  snapshot.types.push_back({"Busy", 150, 150, 40, false});
  snapshot.types.push_back({"Samples", 200, 200, 64, true});
  snapshot.types.push_back({"Quiet", 3, 6, 8, true});
  const std::string table = ScratchPadd::MessageStatsPadd::typeTable(snapshot);
  EXPECT_EQ(table, "Type\tPer s\tJobs per s\tBytes\nBusy (pool?)\t150\t150\t40\n"
                   "Samples (stream?)\t200\t200\t64\nQuiet\t3.0\t6.0\t8");
}
