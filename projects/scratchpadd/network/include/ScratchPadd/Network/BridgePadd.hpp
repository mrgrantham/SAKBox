// BridgePadd.hpp - ScratchPadd messages between devices: the message
// bridge. An app's bridge lists the message types that cross (carry<T>);
// when one of its padds sends one, the bridge sends it to the other
// devices, and on each of them it's sent to the padds there, as if it had
// been sent locally. One device hosts and the others join it (a star); the
// host passes each one's messages on to the rest. Its connections are a
// FrameService, run by NetworkPadd, which the app lists too.
//
//   struct Move { std::int32_t x, y; };            // trivially copyable: crosses as it is
//   class GameBridge : public Network::BridgePadd {
//   public:
//     explicit GameBridge(System *system) : BridgePadd(system) {
//       carry<Move>("move");                       // the same name on every device
//       carry<Chat>("chat", encodeChat, decodeChat);  // anything else: its own encoding
//     }
//   };
//   send(Network::BridgeHost{});                    // on one device
//   send(Network::BridgeJoin{.address = "192.168.4.45"});   // on the others
//
// A trivially copyable message is sent byte for byte, so it must be laid
// out the same on every device: fixed-width types (std::int32_t, float),
// no pointers or std::size_t (4 bytes on a console, 8 on a Mac). Its size
// travels with it, and one that doesn't match is dropped with a warning.
// Its tab shows what it's doing and how many devices are linked, and its
// Mode and Address controls host and join from a panel (remote_controls).
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Network/FrameService.hpp>
#include <ScratchPadd/Network/Service.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace ScratchPadd::Network {

// Asks a bridge to host, join, or leave
struct BridgeHost {};
struct BridgeJoin {
  std::string address; // the host's ("192.168.4.45", or a name)
};
struct BridgeLeave {};

// What a bridge is doing, as shared state (currentState<BridgeState>())
struct BridgeState {
  enum class Role : std::uint8_t { Off, Hosting, Joining, Joined };
  Role role = Role::Off;
  std::uint8_t linked = 0; // other devices it's linked to (directly)
};

class BridgePadd : public Base {
public:
  static constexpr std::uint16_t kPort = 5007;

  explicit BridgePadd(System *system, std::uint16_t port = kPort) : Base(system), port_(port) {
    receive<BridgeHost>([this](const BridgeHost &) { host(); });
    receive<BridgeJoin>([this](const BridgeJoin &join) { this->join(join.address); });
    receive<BridgeLeave>([this](const BridgeLeave &) { leave(); });
  }
  std::string name() override { return "BridgePadd"; }
  ControlValueMap generateControls() override {
    return {{kMode, ControlType::String("off", std::vector<std::string>{"off", "host", "join"})},
            {kAddress, ControlType::String("127.0.0.1")},
            {kStatus, ControlType::Info("off")}};
  }
  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    if (name != kMode)
      return;
    const std::string &mode = control<ControlType::String>(kMode).value;
    if (mode == "host")
      host();
    else if (mode == "join")
      join(control<ControlType::String>(kAddress).value);
    else
      leave();
  }
  void starting() override { publish(); }
  void finishing() override { leave(); }

protected:
  // Carries T: sent byte for byte (it must be trivially copyable, and laid
  // out the same on every device: see the top)
  template <typename T> void carry(const std::string &name) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "a carried type that isn't trivially copyable needs carry(name, encode, decode)");
    carry<T>(
        name,
        [](const T &value) {
          return std::string(reinterpret_cast<const char *>(&value), sizeof(T));
        },
        [](std::string_view bytes) -> std::optional<T> {
          if (bytes.size() != sizeof(T))
            return std::nullopt;
          T value;
          std::copy(bytes.begin(), bytes.end(), reinterpret_cast<char *>(&value));
          return value;
        });
  }
  // Carries T, encoded and decoded by the app (decode: nothing if the bytes
  // aren't a T)
  template <typename T>
  void carry(const std::string &name, const std::function<std::string(const T &)> &encode,
             const std::function<std::optional<T>(std::string_view)> &decode) {
    // Sent here: to the other devices
    receive<T>([this, name, encode](const T &value) {
      if (linked() > 0)
        sendFrame(name, encode(value), FrameService::kNone);
    });
    // Arrived: to the padds here
    decoders_[name] = [this, name, decode](std::string_view bytes) {
      if (const std::optional<T> value = decode(bytes))
        send(*value);
      else
        logger().warn("A \"{}\" that isn't one here ({} bytes): is the other device's build "
                      "the same?",
                      name, bytes.size());
    };
  }

private:
  static constexpr const char *kMode = "Mode";
  static constexpr const char *kAddress = "Address";
  static constexpr const char *kStatus = "Linked";

  void host() { start(BridgeState::Role::Hosting, {.port = port_}); }
  void join(const std::string &address) {
    start(BridgeState::Role::Joining, {.host = address, .port = port_});
  }
  void leave() {
    if (service_)
      services().remove(service_.get()); // (returns once NetworkPadd has let go)
    service_.reset();
    role_ = BridgeState::Role::Off;
    publish();
  }
  void start(BridgeState::Role role, Service::Endpoint endpoint) {
    leave();
    role_ = role;
    // (handed on from the network loop's thread to this padd's)
    service_ = std::make_unique<FrameService>(
        "bridge", std::move(endpoint),
        FrameService::Handlers{
            .onFrame =
                [this](std::uint32_t from, std::string frame) {
                  push([this, from, frame = std::move(frame)] { arrived(from, frame); });
                },
            .onConnected = [this](std::uint32_t) { push([this] { linksChanged(); }); },
            .onDisconnected = [this](std::uint32_t) { push([this] { linksChanged(); }); }});
    services().add(service_.get());
    publish();
  }
  void linksChanged() {
    if (role_ == BridgeState::Role::Joining || role_ == BridgeState::Role::Joined)
      role_ = linked() > 0 ? BridgeState::Role::Joined : BridgeState::Role::Joining;
    publish();
  }
  std::uint8_t linked() const {
    return service_ ? static_cast<std::uint8_t>(service_->connections()) : 0;
  }

  // A frame: its kind (1, a message), its name's length and name, then the
  // message's bytes
  void sendFrame(const std::string &name, const std::string &bytes, std::uint32_t except) {
    std::string frame;
    frame.reserve(2 + name.size() + bytes.size());
    frame += static_cast<char>(kMessage);
    frame += static_cast<char>(name.size());
    frame += name;
    frame += bytes;
    service_->send(frame, FrameService::kNone, except);
  }
  void arrived(std::uint32_t from, const std::string &frame) {
    if (frame.size() < 2 || static_cast<unsigned char>(frame[0]) != kMessage)
      return;
    const std::size_t nameLength = static_cast<unsigned char>(frame[1]);
    if (frame.size() < 2 + nameLength)
      return;
    const std::string name = frame.substr(2, nameLength);
    const std::string_view bytes = std::string_view(frame).substr(2 + nameLength);
    // A host passes it on to the other devices that joined it
    if (role_ == BridgeState::Role::Hosting && service_)
      service_->send(frame, FrameService::kNone, from);
    const auto decoder = decoders_.find(name);
    if (decoder == decoders_.end()) {
      logger().warn("A \"{}\", which this bridge doesn't carry", name);
      return;
    }
    decoder->second(bytes);
  }

  void publish() {
    const std::uint8_t links = linked();
    publishState(BridgeState{.role = role_, .linked = links});
    const char *what = role_ == BridgeState::Role::Hosting   ? "hosting"
                       : role_ == BridgeState::Role::Joining ? "joining"
                       : role_ == BridgeState::Role::Joined  ? "joined"
                                                             : "off";
    std::string status = what;
    if (role_ != BridgeState::Role::Off)
      status += " on port " + std::to_string(port_) + ", " + std::to_string(links) + " linked";
    if (hasControl(kStatus) && control<ControlType::Info>(kStatus).value != status)
      setControl(kStatus, ControlType::Info(status));
  }

  static constexpr unsigned char kMessage = 1;
  std::uint16_t port_;
  BridgeState::Role role_ = BridgeState::Role::Off;
  std::unique_ptr<FrameService> service_;
  std::unordered_map<std::string, std::function<void(std::string_view)>> decoders_;
};

} // namespace ScratchPadd::Network
