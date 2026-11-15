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
//   send(Network::BridgeHost{});                    // on one device: it makes a join code
//   send(Network::BridgeJoin{.address = "192.168.4.45", .code = "K7QM"});  // on the others
//
// Seats: every device in a game has a seat, a number the host gives it (the
// host is seat 1, the others 2 to 16, kept if a device reconnects while
// its seat is free). Seats say who sent what, and who to send it to:
//
//   receive<Network::From<Move>>(...)               // any Move, and the seat it came from
//   send(Network::To<Hand>{.seat = 3, .message = hand});   // a Hand to seat 3 only
//
// From<T> comes with every carried message, this device's own included, so
// a host's rules padd hears every player's moves the same way. The seats
// taken are in BridgeState, published as shared state and also sent as a
// message whenever they change.
//
// Join code: a host makes a short code (shown on its tab) and refuses a
// device that doesn't bring it, so only the people it's shown to can join.
// It decides who joins; the traffic itself isn't encrypted.
//
// A trivially copyable message is sent byte for byte, so it must be laid
// out the same on every device: fixed-width types (std::int32_t, float),
// no pointers or std::size_t (4 bytes on a console, 8 on a Mac). Its size
// travels with it, and one that doesn't match is dropped with a warning.
// Its tab shows what it's doing, its seat and the code, and its Mode,
// Address and Code controls host and join from a panel (remote_controls).
#pragma once

#include <ScratchPadd/Base.hpp>
#include <ScratchPadd/Network/FrameService.hpp>
#include <ScratchPadd/Network/Service.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace ScratchPadd::Network {

// A device's place in a game: the host gives them out
using Seat = std::uint8_t;
inline constexpr Seat kNoSeat = 0;   // not in a game (not hosting, or not seated yet)
inline constexpr Seat kHostSeat = 1; // the host's own
inline constexpr Seat kMostSeats = 16;

// Asks a bridge to host, join, or leave
struct BridgeHost {
  // Nothing: the host makes one; empty: anyone may join
  std::optional<std::string> code;
};
struct BridgeJoin {
  std::string address; // the host's ("192.168.4.45", or a name)
  std::string code;    // as the host's tab shows it
};
struct BridgeLeave {};

// A carried message and the seat it came from (kNoSeat: this device, while
// it isn't in a game)
template <typename T> struct From {
  Seat seat = kNoSeat;
  T message;
};
// A carried message for one seat only (this device's own seat: to its padds)
template <typename T> struct To {
  Seat seat = kNoSeat;
  T message;
};

// What a bridge is doing: shared state (currentState<BridgeState>()), and a
// message whenever it changes
struct BridgeState {
  enum class Role : std::uint8_t { Off, Hosting, Joining, Joined };
  Role role = Role::Off;
  std::uint8_t linked = 0; // other devices it's linked to (directly)
  Seat seat = kNoSeat;     // this device's
  std::uint32_t seats = 0; // the seats taken: bit n for seat n

  bool taken(Seat which) const { return which < 32 && (seats & (1u << which)) != 0; }
  int players() const { return std::popcount(seats); }
  bool operator==(const BridgeState &) const = default;
};

class BridgePadd : public Base {
public:
  static constexpr std::uint16_t kPort = 5007;

  explicit BridgePadd(System *system, std::uint16_t port = kPort) : Base(system), port_(port) {
    receive<BridgeHost>([this](const BridgeHost &host) { this->host(host.code); });
    receive<BridgeJoin>([this](const BridgeJoin &join) { this->join(join.address, join.code); });
    receive<BridgeLeave>([this](const BridgeLeave &) { leave(); });
  }
  std::string name() override { return "BridgePadd"; }
  ControlValueMap generateControls() override {
    return {{kMode, ControlType::String("off", std::vector<std::string>{"off", "host", "join"})},
            {kAddress, ControlType::String("127.0.0.1")},
            {kCode, ControlType::String("")},
            {kStatus, ControlType::Info("off")}};
  }
  void controlChanged(const std::string &name, const ControlTypeVariant &) override {
    if (name != kMode)
      return;
    const std::string &mode = control<ControlType::String>(kMode).value;
    if (mode == "host")
      host(std::nullopt); // (a new code, shown in Code)
    else if (mode == "join")
      join(control<ControlType::String>(kAddress).value, control<ControlType::String>(kCode).value);
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
    // Sent here: to the padds here as From<T>, and to every other seat
    receive<T>([this, name, encode](const T &value) {
      send(From<T>{.seat = seat(), .message = value});
      if (seated())
        sendMessage(name, encode(value), kNoSeat);
    });
    // Sent here to one seat
    receive<To<T>>([this, name, encode](const To<T> &to) {
      if (to.seat != kNoSeat && to.seat == seat())
        deliverHere(to.message, to.seat);
      else if (seated())
        sendMessage(name, encode(to.message), to.seat);
    });
    // Arrived: to the padds here
    decoders_[name] = [this, name, decode](std::string_view bytes, Seat from) {
      if (const std::optional<T> value = decode(bytes))
        deliverHere(*value, from);
      else
        logger().warn("A \"{}\" that isn't one here ({} bytes): is the other device's build "
                      "the same?",
                      name, bytes.size());
    };
  }

private:
  static constexpr const char *kMode = "Mode";
  static constexpr const char *kAddress = "Address";
  static constexpr const char *kCode = "Code";
  static constexpr const char *kStatus = "Linked";

  // Frames: a kind byte first
  enum class Kind : unsigned char {
    Message = 1, // from seat, to seat (kNoSeat: all), name length, name, bytes
    Hello = 2,   // (a joiner's first) protocol version, the seat it had, the code
    Welcome = 3, // (the host's answer) the joiner's seat
    Refused = 4, // (the host's answer, then it closes) why
    Seats = 5,   // (from the host) the seats taken, 4 bytes little-endian
  };
  static constexpr unsigned char kProtocol = 1;
  static char kind(Kind which) { return static_cast<char>(which); }

  template <typename T> void deliverHere(const T &value, Seat from) {
    send(value);
    send(From<T>{.seat = from, .message = value});
  }

  // Hosting and joining
  void host(const std::optional<std::string> &code) {
    code_ = code ? *code : makeCode();
    start(BridgeState::Role::Hosting, {.port = port_});
    seat_ = kHostSeat;
    if (hasControl(kCode) && control<ControlType::String>(kCode).value != code_)
      setControl(kCode, ControlType::String(code_));
    publish();
  }
  void join(const std::string &address, const std::string &code) {
    code_ = code;
    start(BridgeState::Role::Joining, {.host = address, .port = port_});
  }
  void leave() {
    if (service_)
      services().remove(service_.get()); // (returns once NetworkPadd has let go)
    service_.reset();
    role_ = BridgeState::Role::Off;
    seat_ = kNoSeat;
    seatOf_.clear();
    connectionOf_.clear();
    joinerSeats_ = 0;
    publish();
  }
  void start(BridgeState::Role role, Service::Endpoint endpoint) {
    leave();
    refused_.clear();
    role_ = role;
    // (handed on from the network loop's thread to this padd's)
    service_ = std::make_unique<FrameService>(
        "bridge", std::move(endpoint),
        FrameService::Handlers{.onFrame =
                                   [this](std::uint32_t from, std::string frame) {
                                     push([this, from, frame = std::move(frame)] {
                                       arrived(from, frame);
                                     });
                                   },
                               .onConnected =
                                   [this](std::uint32_t connection) {
                                     push([this, connection] { connected(connection); });
                                   },
                               .onDisconnected =
                                   [this](std::uint32_t connection) {
                                     push([this, connection] { disconnected(connection); });
                                   }});
    services().add(service_.get());
    publish();
  }

  void connected(std::uint32_t) {
    if (role_ == BridgeState::Role::Joining && service_) {
      std::string hello;
      hello += kind(Kind::Hello);
      hello += static_cast<char>(kProtocol);
      hello += static_cast<char>(lastSeat_); // (back to it if it's free)
      hello += code_;
      service_->send(hello);
    }
    publish(); // (a host seats a device once its hello has come)
  }
  void disconnected(std::uint32_t connection) {
    if (role_ == BridgeState::Role::Hosting) {
      if (const auto seat = seatOf_.find(connection); seat != seatOf_.end()) {
        logger().info("Seat {} left", int(seat->second));
        connectionOf_.erase(seat->second);
        seatOf_.erase(seat);
        sendSeats();
      }
    } else if (role_ == BridgeState::Role::Joined) {
      role_ = BridgeState::Role::Joining; // (its service connects again)
      seat_ = kNoSeat;
      joinerSeats_ = 0;
    }
    publish();
  }

  void arrived(std::uint32_t from, const std::string &frame) {
    if (frame.empty() || !service_)
      return;
    const std::string_view body = std::string_view(frame).substr(1);
    switch (static_cast<Kind>(frame[0])) {
    case Kind::Hello:
      if (role_ == BridgeState::Role::Hosting)
        hello(from, body);
      break;
    case Kind::Welcome:
      if (role_ == BridgeState::Role::Joining && !body.empty()) {
        seat_ = lastSeat_ = static_cast<Seat>(body[0]);
        role_ = BridgeState::Role::Joined;
        logger().info("Joined as seat {}", int(seat_));
        publish();
      }
      break;
    case Kind::Seats:
      if (role_ == BridgeState::Role::Joined && body.size() >= 4) {
        joinerSeats_ = 0;
        for (int i = 0; i < 4; ++i)
          joinerSeats_ |= static_cast<std::uint32_t>(static_cast<unsigned char>(body[i]))
                          << (8 * i);
        publish();
      }
      break;
    case Kind::Refused:
      if (role_ == BridgeState::Role::Joining) {
        const std::string why(body);
        logger().warn("The host refused this device: {}", why);
        leave();
        refused_ = why;
        publish();
      }
      break;
    case Kind::Message:
      message(from, frame);
      break;
    default:
      break;
    }
  }

  // At the host: a joiner's hello, seated or refused
  void hello(std::uint32_t connection, std::string_view body) {
    if (seatOf_.contains(connection))
      return;
    if (body.size() < 2 || static_cast<unsigned char>(body[0]) != kProtocol)
      return refuse(connection, "another version of the bridge (update both apps)");
    if (body.substr(2) != code_)
      return refuse(connection, "wrong code");
    const auto wanted = static_cast<Seat>(body[1]);
    Seat seat = kNoSeat;
    if (wanted > kHostSeat && wanted <= kMostSeats && !connectionOf_.contains(wanted))
      seat = wanted;
    for (Seat free = kHostSeat + 1; seat == kNoSeat && free <= kMostSeats; ++free)
      if (!connectionOf_.contains(free))
        seat = free;
    if (seat == kNoSeat)
      return refuse(connection, "the game is full");
    seatOf_[connection] = seat;
    connectionOf_[seat] = connection;
    logger().info("Seat {} joined", int(seat));
    std::string welcome;
    welcome += kind(Kind::Welcome);
    welcome += static_cast<char>(seat);
    service_->send(welcome, connection);
    sendSeats();
    publish();
  }
  void refuse(std::uint32_t connection, const std::string &why) {
    logger().info("Refused a device: {}", why);
    service_->sendThenClose(std::string(1, kind(Kind::Refused)) + why, connection);
  }
  void sendSeats() {
    const std::uint32_t seats = takenSeats();
    std::string frame(1, kind(Kind::Seats));
    for (int i = 0; i < 4; ++i)
      frame += static_cast<char>((seats >> (8 * i)) & 0xff);
    for (const auto &[seat, connection] : connectionOf_)
      service_->send(frame, connection);
  }

  // A message frame: from a seat, to a seat or all, the type's name, its bytes
  void sendMessage(const std::string &name, const std::string &bytes, Seat to) {
    std::string frame;
    frame.reserve(4 + name.size() + bytes.size());
    frame += kind(Kind::Message);
    frame += static_cast<char>(seat());
    frame += static_cast<char>(to);
    frame += static_cast<char>(name.size());
    frame += name;
    frame += bytes;
    if (role_ == BridgeState::Role::Hosting)
      relay(frame, to, FrameService::kNone);
    else
      service_->send(frame);
  }
  // At the host: to one seat's device, or every seated device but `except`
  void relay(const std::string &frame, Seat to, std::uint32_t except) {
    if (to != kNoSeat) {
      if (const auto connection = connectionOf_.find(to); connection != connectionOf_.end())
        service_->send(frame, connection->second);
      return;
    }
    for (const auto &[seat, connection] : connectionOf_)
      if (connection != except)
        service_->send(frame, connection);
  }
  void message(std::uint32_t connection, std::string frame) {
    if (frame.size() < 4)
      return;
    const auto to = static_cast<Seat>(frame[2]);
    const std::size_t nameLength = static_cast<unsigned char>(frame[3]);
    if (frame.size() < 4 + nameLength)
      return;
    Seat from = static_cast<Seat>(frame[1]);
    if (role_ == BridgeState::Role::Hosting) {
      // Only from a seated device, and as its own seat (whatever it says)
      const auto seat = seatOf_.find(connection);
      if (seat == seatOf_.end())
        return;
      from = seat->second;
      frame[1] = static_cast<char>(from);
      if (to != kHostSeat)
        relay(frame, to, connection);
      if (to != kNoSeat && to != kHostSeat)
        return; // (for another seat, not the host)
    } else if (role_ != BridgeState::Role::Joined || (to != kNoSeat && to != seat_)) {
      return;
    }
    const std::string name = frame.substr(4, nameLength);
    const auto decoder = decoders_.find(name);
    if (decoder == decoders_.end()) {
      logger().warn("A \"{}\", which this bridge doesn't carry", name);
      return;
    }
    decoder->second(std::string_view(frame).substr(4 + nameLength), from);
  }

  Seat seat() const { return seat_; }
  bool seated() const { return service_ && seat_ != kNoSeat; }
  std::uint32_t takenSeats() const {
    if (role_ == BridgeState::Role::Hosting) {
      std::uint32_t seats = 1u << kHostSeat;
      for (const auto &[seat, connection] : connectionOf_)
        seats |= 1u << seat;
      return seats;
    }
    return role_ == BridgeState::Role::Joined ? joinerSeats_ : 0;
  }
  std::uint8_t linked() const {
    return service_ ? static_cast<std::uint8_t>(service_->connections()) : 0;
  }
  // Four letters and digits that can't be misread (no O and 0, I and 1)
  static std::string makeCode() {
    static constexpr std::string_view kLetters = "ACDEFGHJKMNPQRTUVWXY34679";
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<std::size_t> pick(0, kLetters.size() - 1);
    std::string code;
    for (int i = 0; i < 4; ++i)
      code += kLetters[pick(generator)];
    return code;
  }

  void publish() {
    const BridgeState state{
        .role = role_, .linked = linked(), .seat = seat_, .seats = takenSeats()};
    publishState(state);
    if (state != published_) {
      published_ = state;
      send(state);
    }
    std::string status;
    switch (role_) {
    case BridgeState::Role::Hosting:
      status = "hosting on port " + std::to_string(port_) +
               (code_.empty() ? std::string(", no code") : ", code " + code_) + ", " +
               std::to_string(state.players()) + " seated";
      break;
    case BridgeState::Role::Joining:
      status = "joining (port " + std::to_string(port_) + ")";
      break;
    case BridgeState::Role::Joined:
      status = "seat " + std::to_string(seat_) + " of " + std::to_string(state.players());
      break;
    case BridgeState::Role::Off:
      status = refused_.empty() ? "off" : "refused: " + refused_;
      break;
    }
    if (hasControl(kStatus) && control<ControlType::Info>(kStatus).value != status)
      setControl(kStatus, ControlType::Info(status));
  }

  std::uint16_t port_;
  BridgeState::Role role_ = BridgeState::Role::Off;
  BridgeState published_;
  Seat seat_ = kNoSeat;
  Seat lastSeat_ = kNoSeat; // (a joiner asks for it back when it reconnects)
  std::string code_;
  std::string refused_;           // why the host refused this device, while it's off
  std::uint32_t joinerSeats_ = 0; // (a joiner: the seats the host last sent)
  std::unique_ptr<FrameService> service_;
  std::map<std::uint32_t, Seat> seatOf_;       // (the host) a connection's seat
  std::map<Seat, std::uint32_t> connectionOf_; // (the host) a seat's connection
  std::unordered_map<std::string, std::function<void(std::string_view, Seat)>> decoders_;
};

} // namespace ScratchPadd::Network
