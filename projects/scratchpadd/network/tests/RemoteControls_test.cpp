// RemoteControlsPadd and NetworkPadd in a running system, with a client on
// localhost (a LineService in a loop of its own, as the Mac's remote app
// has): connecting brings every padd's controls; a change sent goes to its
// padd, whose new snapshot comes back; NetworkPadd's tab shows the service.
#include <ScratchPadd/Messages/ControlsText.hpp>
#include <ScratchPadd/Network/LineService.hpp>
#include <ScratchPadd/Network/Loop.hpp>
#include <ScratchPadd/Network/NetworkPadd.hpp>
#include <ScratchPadd/Network/RemoteControlsPadd.hpp>
#include <ScratchPadd/ScratchPadd.hpp>
#include <ScratchPadd/Testing.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace Network = ScratchPadd::Network;
namespace Text = ScratchPadd::ControlsText;

namespace {

template <typename Done> bool eventually(Done done, std::chrono::milliseconds within = 5s) {
  for (auto until = std::chrono::steady_clock::now() + within;
       std::chrono::steady_clock::now() < until; std::this_thread::sleep_for(5ms))
    if (done())
      return true;
  return done();
}

std::uint16_t freePort() {
  std::string error;
  return Network::Socket::listenTcp(0, error).port();
}
std::uint16_t gPort = 0;

class TestRemoteControlsPadd : public Network::RemoteControlsPadd {
public:
  explicit TestRemoteControlsPadd(ScratchPadd::System *system)
      : RemoteControlsPadd(system, gPort) {}
};

// A padd with a control to change from afar
class OwnerPadd : public ScratchPadd::Base {
public:
  explicit OwnerPadd(ScratchPadd::System *system) : Base(system) {}
  std::string name() override { return "Owner"; }
  ScratchPadd::ControlValueMap generateControls() override {
    return {{"speed", ScratchPadd::ControlType::Double(1.0, std::make_pair(0.0, 10.0))}};
  }
  double speed() {
    return ScratchPadd::onPaddThread(*this, [](OwnerPadd &p) {
      return ScratchPadd::PaddTestAccess::control<ScratchPadd::ControlType::Double>(p, "speed")
          .value;
    });
  }
};

} // namespace

TEST(RemoteControlsTest, ControlsFromAfar) {
  gPort = freePort();
  ScratchPadd::SystemImplementation<Network::NetworkPadd, TestRemoteControlsPadd, OwnerPadd> system;
  system.instantiate();
  std::thread running([&] { system.start(); });
  system.waitForStart();

  std::mutex mutex;
  std::vector<std::string> lines;
  Network::LineService client("client", {.host = "127.0.0.1", .port = gPort},
                              {.onLine = [&](std::uint32_t, std::string line) {
                                std::lock_guard lock(mutex);
                                lines.push_back(std::move(line));
                              }});
  Network::Loop loop;
  std::string error;
  ASSERT_TRUE(loop.start(error)) << error;
  loop.attach(&client);

  // Owner's controls, on connecting
  auto speedSent = [&]() -> std::optional<double> {
    std::lock_guard lock(mutex);
    std::optional<double> latest;
    for (const std::string &line : lines)
      if (const auto message = Text::decode(line); message &&
                                                   message->kind == Text::Decoded::Kind::Snapshot &&
                                                   message->snapshot.paddName == "Owner")
        latest =
            std::get<ScratchPadd::ControlType::Double>(message->snapshot.controlMap.at("speed"))
                .value;
    return latest;
  };
  ASSERT_TRUE(eventually([&] { return speedSent().has_value(); }, Network::Loop::kRetry + 3s));
  EXPECT_EQ(speedSent().value_or(-1), 1.0);

  // A change from afar: made, and its new snapshot sent back
  client.send(Text::change(
      "Owner", {.controlName = "speed", .controlTypeValue = ScratchPadd::ControlType::Double(4)}));
  EXPECT_TRUE(eventually([&] {
    return system.padds().size() == 3 &&
           std::get<std::unique_ptr<OwnerPadd>>(system.getWorkers())->speed() == 4.0;
  }));
  EXPECT_TRUE(eventually([&] { return speedSent() == 4.0; }));

  // NetworkPadd's tab: the service, and its client
  auto &network = *std::get<std::unique_ptr<Network::NetworkPadd>>(system.getWorkers());
  auto tab = [&] {
    return ScratchPadd::onPaddThread(network, [](Network::NetworkPadd &p) {
      const auto controls = ScratchPadd::PaddTestAccess::snapshot(p).controlMap;
      const auto found = controls.find("remote controls");
      return found == controls.end()
                 ? std::string()
                 : std::get<ScratchPadd::ControlType::Info>(found->second).value;
    });
  };
  EXPECT_TRUE(eventually([&] { return tab().find("1 connected") != std::string::npos; })) << tab();

  loop.stop();
  system.stop();
  running.join();
}
