// The message bridge between ScratchPadd systems, on localhost: three
// systems in one process (each as on its own device; one NetworkPadd runs
// the network for them all, as the services are registered process-wide).
// A carried message sent in one arrives in the others: from the host to
// those that joined, from one that joined to the host and, passed on by the
// host, to the other; a message with its own encoding crosses too; one of
// another size (another build) is dropped. Seats: the host gives them out,
// every message comes with the seat it came from, a message can go to one
// seat, a device without the join code is refused, and one that leaves and
// comes back gets its seat again.
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
    receive<Network::BridgeState>([this](const Network::BridgeState &state) {
      std::lock_guard lock(mutex_);
      roles_.push_back(state.role);
    });
    receive<Network::From<Move>>([this](const Network::From<Move> &from) {
      std::lock_guard lock(mutex_);
      movesFrom_.push_back(from.seat * 10000 + from.message.x * 100 + from.message.y);
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
  // Every Move with its seat, this device's own too: seat * 10000 + x * 100 + y
  std::vector<int> movesFrom() {
    std::lock_guard lock(mutex_);
    return movesFrom_;
  }

  // The bridge's role each time its state changed (from its message)
  std::vector<Network::BridgeState::Role> roles() {
    std::lock_guard lock(mutex_);
    return roles_;
  }

private:
  std::mutex mutex_;
  std::vector<int> moves_, movesFrom_;
  std::vector<Network::BridgeState::Role> roles_;
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
  host.padd<PlayerPadd<1>>().say(Network::BridgeHost{.code = ""});
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
  host.padd<PlayerPadd<1>>().say(Network::BridgeHost{.code = ""});
  other.padd<WiderPlayerPadd>().join();
  ASSERT_TRUE(eventually([&] {
    return other.padd<WiderPlayerPadd>().bridge().role == Network::BridgeState::Role::Joined;
  }));
  host.padd<PlayerPadd<1>>().say(Move{.x = 1, .y = 2});
  std::this_thread::sleep_for(300ms);
  EXPECT_EQ(other.padd<WiderPlayerPadd>().received.load(), 0);
}

namespace {

std::uint16_t freshPort() {
  std::string error;
  return Network::Socket::listenTcp(0, error).port();
}

// Three devices, the host with a code and both others brought in with it
struct ThreeSeated {
  Running<Network::NetworkPadd, GameBridge, PlayerPadd<1>> host;
  Running<GameBridge, PlayerPadd<2>> second;
  Running<GameBridge, PlayerPadd<3>> third;
  PlayerPadd<1> &one() { return host.padd<PlayerPadd<1>>(); }
  PlayerPadd<2> &two() { return second.padd<PlayerPadd<2>>(); }
  PlayerPadd<3> &three() { return third.padd<PlayerPadd<3>>(); }
  ThreeSeated() {
    one().say(Network::BridgeHost{.code = "K7QM"});
    two().say(Network::BridgeJoin{.address = "127.0.0.1", .code = "K7QM"});
    three().say(Network::BridgeJoin{.address = "127.0.0.1", .code = "K7QM"});
  }
  bool allSeated() {
    return one().bridge().players() == 3 && two().bridge().players() == 3 &&
           three().bridge().players() == 3;
  }
};

} // namespace

// The host is seat 1; the others get 2 and 3, and all three see the same seats
TEST(BridgeTest, TheHostGivesOutSeats) {
  gPort = freshPort();
  ThreeSeated game;
  ASSERT_TRUE(eventually([&] { return game.allSeated(); }));
  EXPECT_EQ(game.one().bridge().seat, Network::kHostSeat);
  const Network::Seat a = game.two().bridge().seat, b = game.three().bridge().seat;
  EXPECT_TRUE((a == 2 && b == 3) || (a == 3 && b == 2)) << int(a) << " " << int(b);
  EXPECT_EQ(game.two().bridge().role, Network::BridgeState::Role::Joined);
  for (Network::Seat seat : {1, 2, 3})
    EXPECT_TRUE(game.three().bridge().taken(seat)) << int(seat);
  EXPECT_FALSE(game.three().bridge().taken(4));
}

// From<T> says which seat sent it, on every device, the sender's own included
TEST(BridgeTest, MessagesSayWhichSeatSentThem) {
  gPort = freshPort();
  ThreeSeated game;
  ASSERT_TRUE(eventually([&] { return game.allSeated(); }));
  const int seat2 = game.two().bridge().seat;
  game.two().say(Move{.x = 4, .y = 5});
  const std::vector<int> expected{seat2 * 10000 + 405};
  EXPECT_TRUE(eventually([&] { return game.one().movesFrom() == expected; }));
  EXPECT_TRUE(eventually([&] { return game.three().movesFrom() == expected; }));
  EXPECT_TRUE(eventually([&] { return game.two().movesFrom() == expected; })); // (its own)
  game.one().say(Move{.x = 0, .y = 1});
  EXPECT_TRUE(eventually(
      [&] { return game.three().movesFrom() == std::vector<int>{seat2 * 10000 + 405, 10001}; }));
}

// To<T> reaches one seat only: from the host, to the host, and from one
// joiner to another through the host
TEST(BridgeTest, AMessageCanGoToOneSeat) {
  gPort = freshPort();
  ThreeSeated game;
  ASSERT_TRUE(eventually([&] { return game.allSeated(); }));
  const Network::Seat seat3 = game.three().bridge().seat;
  game.one().say(Network::To<Chat>{.seat = seat3, .message = Chat{"for three"}});
  game.two().say(Network::To<Chat>{.seat = Network::kHostSeat, .message = Chat{"for the host"}});
  game.two().say(Network::To<Chat>{.seat = seat3, .message = Chat{"two to three"}});
  EXPECT_TRUE(eventually([&] {
    return game.three().chats() == std::vector<std::string>{"for three", "two to three"};
  }));
  EXPECT_TRUE(
      eventually([&] { return game.one().chats() == std::vector<std::string>{"for the host"}; }));
  std::this_thread::sleep_for(200ms);
  EXPECT_TRUE(game.two().chats().empty());
  EXPECT_EQ(game.one().chats().size(), 1u);
}

// Without the code, a device is refused and left off; the host's seats don't change
TEST(BridgeTest, AWrongCodeIsRefused) {
  gPort = freshPort();
  Running<Network::NetworkPadd, GameBridge, PlayerPadd<1>> host;
  Running<GameBridge, PlayerPadd<2>> other;
  host.padd<PlayerPadd<1>>().say(Network::BridgeHost{.code = "K7QM"});
  other.padd<PlayerPadd<2>>().say(Network::BridgeJoin{.address = "127.0.0.1", .code = "NOPE"});
  // Joining, then refused as soon as its hello reaches the host (which closes it): off
  using Role = Network::BridgeState::Role;
  EXPECT_TRUE(eventually([&] {
    const std::vector<Role> roles = other.padd<PlayerPadd<2>>().roles();
    return roles.size() >= 2 && roles[roles.size() - 2] == Role::Joining &&
           roles.back() == Role::Off;
  }));
  EXPECT_EQ(other.padd<PlayerPadd<2>>().bridge().role, Role::Off);
  EXPECT_TRUE(eventually([&] { return host.padd<PlayerPadd<1>>().bridge().linked == 0; }));
  EXPECT_EQ(host.padd<PlayerPadd<1>>().bridge().players(), 1);
  host.padd<PlayerPadd<1>>().say(Move{.x = 1, .y = 1});
  std::this_thread::sleep_for(200ms);
  EXPECT_TRUE(other.padd<PlayerPadd<2>>().moves().empty());
}

// A device that leaves and joins again gets its seat back, if it's still free
TEST(BridgeTest, RejoiningKeepsTheSeat) {
  gPort = freshPort();
  ThreeSeated game;
  ASSERT_TRUE(eventually([&] { return game.allSeated(); }));
  const Network::Seat seat3 = game.three().bridge().seat;
  game.three().say(Network::BridgeLeave{});
  ASSERT_TRUE(eventually([&] { return game.one().bridge().players() == 2; }));
  EXPECT_FALSE(game.one().bridge().taken(seat3));
  game.three().say(Network::BridgeJoin{.address = "127.0.0.1", .code = "K7QM"});
  ASSERT_TRUE(eventually([&] { return game.allSeated(); }));
  EXPECT_EQ(game.three().bridge().seat, seat3);
}
