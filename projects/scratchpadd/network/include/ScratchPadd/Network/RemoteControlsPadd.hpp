// RemoteControlsPadd.hpp - an app's controls, from another device: whoever
// connects to port 5006 (the Mac's remote controls app) gets every padd's
// controls (ControlSnapshot), and each change after, as lines of text
// (ControlsText), and its changes (ControlChange) go to the padds they're
// for, as a panel's do. A LineService, run by NetworkPadd, which the app
// lists too:
//
//   ScratchPadd::SystemImplementation<..., ScratchPadd::Network::NetworkPadd,
//                                     ScratchPadd::Network::RemoteControlsPadd> system;
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Messages/ControlsText.hpp>
#include <ScratchPadd/Network/LineService.hpp>
#include <ScratchPadd/Network/Service.hpp>

#include <cstdint>
#include <string>

namespace ScratchPadd::Network {

class RemoteControlsPadd : public Base {
public:
  static constexpr std::uint16_t kPort = 5006;

  explicit RemoteControlsPadd(System *system) : RemoteControlsPadd(system, kPort) {}
  // (on another port: tests)
  RemoteControlsPadd(System *system, std::uint16_t port)
      : Base(system), service_("remote controls", {.port = port},
                               // (on the network loop's thread: handled on this padd's)
                               {.onLine =
                                    [this](std::uint32_t, std::string line) {
                                      push([this, line = std::move(line)] { handle(line); });
                                    },
                                .onConnected =
                                    [this](std::uint32_t) {
                                      push([this] { send(MessageType::ControlRequest{}); });
                                    }}) {
    // Every padd's controls: the answers to its requests, and each change a
    // padd sends every panel
    receive<MessageType::ControlSnapshot>([this](const MessageType::ControlSnapshot &snapshot) {
      service_.send(ControlsText::snapshot(snapshot));
    });
  }
  std::string name() override { return "RemoteControlsPadd"; }
  ControlValueMap generateControls() override { return {}; }
  void starting() override { services().add(&service_); }
  void finishing() override { services().remove(&service_); }

private:
  void handle(const std::string &line) {
    const auto message = ControlsText::decode(line);
    if (!message) {
      logger().warn("Not a control message: {}", line.substr(0, 60));
      return;
    }
    if (message->kind == ControlsText::Decoded::Kind::Request) {
      send(MessageType::ControlRequest{});
    } else if (message->kind == ControlsText::Decoded::Kind::Change) {
      if (!sendTo(addressOf(message->paddName), message->change))
        logger().warn("A change for {}, which isn't here", message->paddName);
    }
  }

  LineService service_;
};

} // namespace ScratchPadd::Network
