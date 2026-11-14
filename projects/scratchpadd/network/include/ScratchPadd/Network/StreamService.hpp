// StreamService.hpp - text served to whoever connects to a port: what's
// been kept so far, then the rest as it comes, to each reader from where it
// is (several at once). The log's: a debug build's on a 3DS, on port 5005,
// for tools/console_log or nc. Its text comes from a function, so any
// growing text can be served: the log's is a DebugLog::LogBuffer.
//
//   Network::StreamService log("log", 5005,
//       [&](std::uint64_t &position) { return buffer.readFrom(position); });
//   buffer.setOnAppend([&] { log.wake(); });   // new text: the loop sends it
//   Network::services().add(&log);
#pragma once

#include <ScratchPadd/Network/Service.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace ScratchPadd::Network {

class StreamService final : public Service {
public:
  // The text after `position` (0 the first time; the oldest kept if that's
  // gone), `position` moved past it
  using Read = std::function<std::string(std::uint64_t &position)>;

  StreamService(std::string name, std::uint16_t port, Read read)
      : name_(std::move(name)), port_(port), read_(std::move(read)) {}

  std::string name() const override { return name_; }
  Endpoint endpoint() const override { return {.port = port_}; }
  void connected(Connection &connection) override { positions_[connection.id()] = 0; }
  void received(Connection &, std::string_view) override {} // (readers only read)
  void pump(Connection &connection) override {
    std::uint64_t &position = positions_[connection.id()];
    const std::string text = read_(position);
    if (!text.empty())
      connection.send(text);
  }
  void disconnected(Connection &connection) override { positions_.erase(connection.id()); }

private:
  std::string name_;
  std::uint16_t port_;
  Read read_;
  std::unordered_map<std::uint32_t, std::uint64_t> positions_; // (the loop's thread)
};

} // namespace ScratchPadd::Network
