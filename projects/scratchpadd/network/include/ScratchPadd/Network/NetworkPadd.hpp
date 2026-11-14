// NetworkPadd.hpp - an app's networking: it starts the network and runs
// every service registered with it (services(): the log's stream, remote
// controls, and later a game's own) in its I/O loop (Loop), a thread that
// waits on all their sockets at once, so no padd waits on the network.
// List it in an app's system:
//
//   ScratchPadd::SystemImplementation<..., ScratchPadd::Network::NetworkPadd> system;
//
// Its tab on a panel shows the device's address, and for each service where
// it listens or connects, what it's doing, and what it has sent (Info). On
// a DS it does nothing: no network (a DS Lite can't join a WPA2 network).
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Network/Loop.hpp>
#include <ScratchPadd/Network/Service.hpp>
#include <ScratchPadd/Network/Sockets.hpp>

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>

namespace ScratchPadd::Network {

class NetworkPadd : public Base {
public:
  explicit NetworkPadd(System *system) : Base(system) {}
  std::string name() override { return "NetworkPadd"; }
  std::optional<std::chrono::duration<double>> repeatInterval() override {
    return std::chrono::seconds(1);
  }
  ControlValueMap generateControls() override {
    return {{kAddress, ControlType::Info("(starting)")}};
  }

  void starting() override {
    std::string error;
    if (!loop_.start(error)) {
      setControl(kAddress, ControlType::Info("no network: " + error));
      logger().warn("No network: {}", error);
      return;
    }
    setControl(kAddress, ControlType::Info(deviceAddress()));
    // (on the registering thread: the loop takes it from there; a removed
    // one is let go of before remove() returns)
    services().listen([this](Service *added, Service *removed) {
      if (added)
        loop_.attach(added);
      if (removed)
        loop_.detach(removed);
    });
    for (Service *service : services().all())
      loop_.attach(service);
  }
  void finishing() override {
    services().listen(nullptr);
    loop_.stop();
  }
  // Each service's line on the tab, where it's changed
  void repeat() override {
    for (const Loop::ServiceState &state : loop_.states()) {
      char sent[48];
      std::snprintf(sent, sizeof sent, "%.1f KB sent, %.1f KB received",
                    static_cast<double>(state.sent) / 1024,
                    static_cast<double>(state.received) / 1024);
      const std::string line = state.where + ": " + state.state + ", " +
                               std::to_string(state.connections) + " connected; " + sent;
      if (!hasControl(state.name))
        addControl(state.name, ControlType::Info(line));
      else if (control<ControlType::Info>(state.name).value != line)
        setControl(state.name, ControlType::Info(line));
    }
  }

private:
  static constexpr const char *kAddress = "Address";
  Loop loop_;
};

} // namespace ScratchPadd::Network
