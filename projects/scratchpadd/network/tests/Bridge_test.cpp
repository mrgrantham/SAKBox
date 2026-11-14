// The message bridge between ScratchPadd systems, on localhost: three
// systems in one process (each as on its own device; one NetworkPadd runs
// the network for them all, as the services are registered process-wide).
// A carried message sent in one arrives in the others: from the host to
// those that joined, from one that joined to the host and, passed on by the
// host, to the other; a message with its own encoding crosses too; one of
// another size (another build) is dropped.
#include <ScratchPadd/Network/BridgePadd.hpp>
#include <ScratchPadd/Network/NetworkPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace Network = ScratchPadd::Network;

namespace {

template <typename Done> bool eventually(Done done, std::chrono::milliseconds within = 6s) {
  for (auto until = std::chrono::steady_clock::now() + within;
       std::chrono::steady_clock::now() < until; std::this_thread::sleep_for(5ms))
    if (done())
      return true;
  return done();
}

std::uint16_t gPort = 0;

struct Move {
  std::int32_t x = 0, y = 0;
};
struct Chat {
  std::string text;
};

// A bridge carrying Move (byte for byte) and Chat (its own encoding)
class GameBridge : public Network::BridgePadd {
public:
  explicit GameBridge(ScratchPadd::System *system) : BridgePadd(system, gPort) {
    carry<Move>("move");
    carry<Chat>(
        "chat", [](const Chat &chat) { return chat.text; },
        [](std::string_view bytes) { return std::optional<Chat>(Chat{std::string(bytes)}); });
  }
};

// Another build's Move: a field more, under the same name
struct WiderMove {
  std::int32_t x = 0, y = 0, z = 0;
};
class OtherBuildsBridge : public Network::BridgePadd {
public:
  explicit OtherBuildsBridge(ScratchPadd::System *system) : BridgePadd(system, gPort) {
    carry<WiderMove>("move");
  }
};
class WiderPlayerPadd : public ScratchPadd::Base {
public:
  explicit WiderPlayerPadd(ScratchPadd::System *system) : Base(system) {
    receive<WiderMove>([this](const WiderMove &) { ++received; });
  }
  std::string name() override { return "WiderPlayer"; }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  void join() {
    ScratchPadd::onPaddThread(*this, [](WiderPlayerPadd &p) {
      p.send(Network::BridgeJoin{.address = "127.0.0.1"});
      return 0;
    });
  }
  Network::BridgeState bridge() {
    return ScratchPadd::onPaddThread(
        *this, [](WiderPlayerPadd &p) { return p.currentState<Network::BridgeState>().value; });
  }
  std::atomic<int> received{0};
};

// Sends and keeps what it receives
template <int Id> class PlayerPadd : public ScratchPadd::Base {
public:
  explicit PlayerPadd(ScratchPadd::System *system) : Base(system) {
    receive<Move>([this](const Move &move) {
      std::lock_guard lock(mutex_);
      moves_.push_back(move.x * 100 + move.y);
    });
    receive<Chat>([this](const Chat &chat) {
      std::lock_guard lock(mutex_);
      chats_.push_back(chat.text);
    });
  }
  std::string name() override { return "Player" + std::to_string(Id); }
  ScratchPadd::ControlValueMap generateControls() override { return {}; }
  template <typename T> void say(T message) {
    ScratchPadd::onPaddThread(*this, [&](PlayerPadd &p) {
      p.send(message);
      return 0;
    });
  }
  // The bridge's state, as this padd sees it
  Network::BridgeState bridge() {
    return ScratchPadd::onPaddThread(
        *this, [](PlayerPadd &p) { return p.template currentState<Network::BridgeState>().value; });
  }
  std::vector<int> moves() {
    std::lock_guard lock(mutex_);
    return moves_;
  }
  std::vector<std::string> chats() {
    std::lock_guard lock(mutex_);
    return chats_;
  }

private:
  std::mutex mutex_;
  std::vector<int> moves_;
  std::vector<std::string> chats_;
};

template <typename... Padds> class Running {
public:
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
};

} // namespace

TEST(BridgeTest, MessagesCrossBetweenDevices) {
  {
    std::string error;
    gPort = Network::Socket::listenTcp(0, error).port();
  }
  Running<Network::NetworkPadd, GameBridge, PlayerPadd<1>> host;
  Running<GameBridge, PlayerPadd<2>> second;
  Running<GameBridge, PlayerPadd<3>> third;
  host.padd<PlayerPadd<1>>().say(Network::BridgeHost{});
  second.padd<PlayerPadd<2>>().say(Network::BridgeJoin{.address = "127.0.0.1"});
  third.padd<PlayerPadd<3>>().say(Network::BridgeJoin{.address = "127.0.0.1"});
  ASSERT_TRUE(eventually([&] { return host.padd<PlayerPadd<1>>().bridge().linked == 2; }))
      << int(host.padd<PlayerPadd<1>>().bridge().linked);
  ASSERT_TRUE(eventually([&] {
    return second.padd<PlayerPadd<2>>().bridge().role == Network::BridgeState::Role::Joined &&
           third.padd<PlayerPadd<3>>().bridge().role == Network::BridgeState::Role::Joined;
  }));

  // The host's, to both that joined
  host.padd<PlayerPadd<1>>().say(Move{.x = 1, .y = 2});
  EXPECT_TRUE(eventually([&] { return second.padd<PlayerPadd<2>>().moves() == std::vector{102}; }));
  EXPECT_TRUE(eventually([&] { return third.padd<PlayerPadd<3>>().moves() == std::vector{102}; }));
  // One that joined: to the host, and passed on to the other (not back)
  second.padd<PlayerPadd<2>>().say(Chat{"hello"});
  EXPECT_TRUE(eventually(
      [&] { return host.padd<PlayerPadd<1>>().chats() == std::vector<std::string>{"hello"}; }));
  EXPECT_TRUE(eventually(
      [&] { return third.padd<PlayerPadd<3>>().chats() == std::vector<std::string>{"hello"}; }));
  std::this_thread::sleep_for(200ms);
  EXPECT_TRUE(second.padd<PlayerPadd<2>>().chats().empty()); // (not echoed back)
  EXPECT_TRUE(host.padd<PlayerPadd<1>>().moves().empty());   // (a sender doesn't get its own)
}

// A carried message of another size (another build's layout) is dropped,
// not misread
TEST(BridgeTest, AMessageOfAnotherSizeIsDropped) {
  {
    std::string error;
    gPort = Network::Socket::listenTcp(0, error).port();
  }
  Running<Network::NetworkPadd, GameBridge, PlayerPadd<1>> host;
  Running<OtherBuildsBridge, WiderPlayerPadd> other;
  host.padd<PlayerPadd<1>>().say(Network::BridgeHost{});
  other.padd<WiderPlayerPadd>().join();
  ASSERT_TRUE(eventually([&] {
    return other.padd<WiderPlayerPadd>().bridge().role == Network::BridgeState::Role::Joined;
  }));
  host.padd<PlayerPadd<1>>().say(Move{.x = 1, .y = 2});
  std::this_thread::sleep_for(300ms);
  EXPECT_EQ(other.padd<WiderPlayerPadd>().received.load(), 0);
}
