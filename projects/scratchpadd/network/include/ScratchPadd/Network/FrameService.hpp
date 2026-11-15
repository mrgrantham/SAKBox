// FrameService.hpp - binary frames both ways: listening on a port (a
// bridge's host) or connecting to one (a bridge joining it). Each frame goes
// as its length (4 bytes, little-endian) and then its bytes, so a frame can
// hold anything; one received whole is handed to `onFrame` on the network
// loop's thread. send() queues a frame from any thread: to one connection,
// to every connection, or to every connection but one (a host passing a
// frame on from the device that sent it). sendThenClose() sends one
// connection a last frame (a bridge refusing a device: why) and closes it.
#pragma once

#include <ScratchPadd/Network/Service.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ScratchPadd::Network {

class FrameService final : public Service {
public:
  struct Handlers {
    std::function<void(std::uint32_t connection, std::string frame)> onFrame;
    std::function<void(std::uint32_t connection)> onConnected;
    std::function<void(std::uint32_t connection)> onDisconnected;
  };
  static constexpr std::uint32_t kNone = 0;
  // A longer frame means the other side isn't speaking frames: it's dropped
  static constexpr std::uint32_t kLongestFrame = 1 << 20;

  FrameService(std::string name, Endpoint endpoint, Handlers handlers)
      : name_(std::move(name)), endpoint_(std::move(endpoint)), handlers_(std::move(handlers)) {}

  std::string name() const override { return name_; }
  Endpoint endpoint() const override { return endpoint_; }

  // From any thread: `frame` to `to` (kNone: every connection), but not to
  // `except`. Dropped if there's no one to send it to.
  void send(std::string_view frame, std::uint32_t to = kNone, std::uint32_t except = kNone) {
    queue(frame, to, except, false);
  }
  // From any thread: `frame` to `to` only, which is then closed (once it's sent)
  void sendThenClose(std::string_view frame, std::uint32_t to) { queue(frame, to, kNone, true); }
  std::size_t connections() const {
    std::lock_guard lock(mutex_);
    return connections_.size();
  }

  void connected(Connection &connection) override {
    {
      std::lock_guard lock(mutex_);
      connections_.push_back(connection.id());
    }
    partial_[connection.id()].clear();
    if (handlers_.onConnected)
      handlers_.onConnected(connection.id());
  }
  void received(Connection &connection, std::string_view bytes) override {
    std::string &partial = partial_[connection.id()];
    partial.append(bytes);
    while (partial.size() >= 4) {
      std::uint32_t length = 0;
      for (int i = 0; i < 4; ++i)
        length |= static_cast<std::uint32_t>(
                      static_cast<unsigned char>(partial[static_cast<std::size_t>(i)]))
                  << (8 * i);
      if (length > kLongestFrame) {
        connection.close(); // (not frames: let it go)
        partial.clear();
        return;
      }
      if (partial.size() < 4 + length)
        return; // (the rest still to come)
      std::string frame = partial.substr(4, length);
      partial.erase(0, 4 + length);
      if (handlers_.onFrame)
        handlers_.onFrame(connection.id(), std::move(frame));
    }
  }
  void pump(Connection &connection) override {
    std::lock_guard lock(mutex_);
    std::vector<Queued> kept;
    for (Queued &queued : queued_) {
      const std::uint32_t id = connection.id();
      const bool forThis = queued.to == id || (queued.to == kNone && queued.except != id);
      const bool sentAlready = std::ranges::find(queued.sentTo, id) != queued.sentTo.end();
      if (forThis && !sentAlready) {
        connection.send(queued.bytes);
        queued.sentTo.push_back(id);
        if (queued.thenClose)
          connection.close(); // (after what's queued, this frame too)
      }
      if (!done(queued))
        kept.push_back(std::move(queued));
    }
    queued_ = std::move(kept);
  }
  void disconnected(Connection &connection) override {
    {
      std::lock_guard lock(mutex_);
      std::erase(connections_, connection.id());
      std::erase_if(queued_, [&](const Queued &queued) { return done(queued); });
    }
    partial_.erase(connection.id());
    if (handlers_.onDisconnected)
      handlers_.onDisconnected(connection.id());
  }

private:
  struct Queued {
    std::uint32_t to, except;
    std::string bytes;
    std::vector<std::uint32_t> sentTo;
    bool thenClose = false;
  };
  void queue(std::string_view frame, std::uint32_t to, std::uint32_t except, bool thenClose) {
    {
      std::lock_guard lock(mutex_);
      if (to == kNone && connections_.size() <= (except == kNone ? 0u : 1u))
        return;
      std::string bytes(4, '\0');
      const auto length = static_cast<std::uint32_t>(frame.size());
      for (int i = 0; i < 4; ++i)
        bytes[static_cast<std::size_t>(i)] = static_cast<char>((length >> (8 * i)) & 0xff);
      bytes.append(frame);
      queued_.push_back({to, except, std::move(bytes), {}, thenClose});
    }
    wake();
  }
  // Whether every connection it's for has it (under the lock)
  bool done(const Queued &queued) const {
    if (queued.to != kNone)
      return std::ranges::find(queued.sentTo, queued.to) != queued.sentTo.end() ||
             std::ranges::find(connections_, queued.to) == connections_.end();
    return std::ranges::all_of(connections_, [&](std::uint32_t id) {
      return id == queued.except || std::ranges::find(queued.sentTo, id) != queued.sentTo.end();
    });
  }

  std::string name_;
  Endpoint endpoint_;
  Handlers handlers_;
  mutable std::mutex mutex_; // the queue and the connections
  std::vector<Queued> queued_;
  std::vector<std::uint32_t> connections_;
  std::unordered_map<std::uint32_t, std::string> partial_; // (the loop's thread)
};

} // namespace ScratchPadd::Network
