// LineService.hpp - lines of text both ways: listening on a port (a
// console's remote controls) or connecting to one (the Mac's remote app).
// Each line received is handed, without its newline, to `onLine`, on the
// loop's thread; send() queues a line from any thread, to one connection
// or all, and wakes the loop to send it.
#pragma once

#include <ScratchPadd/Network/Service.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ScratchPadd::Network {

class LineService final : public Service {
public:
  struct Handlers {
    std::function<void(std::uint32_t connection, std::string line)> onLine;
    std::function<void(std::uint32_t connection)> onConnected;
    std::function<void(std::uint32_t connection)> onDisconnected;
  };
  static constexpr std::uint32_t kEveryConnection = 0;

  LineService(std::string name, Endpoint endpoint, Handlers handlers)
      : name_(std::move(name)), endpoint_(std::move(endpoint)), handlers_(std::move(handlers)) {}

  std::string name() const override { return name_; }
  Endpoint endpoint() const override { return endpoint_; }

  // From any thread: `line` (without a newline) to `connection`, or to
  // every connection (dropped if there are none)
  void send(std::string line, std::uint32_t connection = kEveryConnection) {
    {
      std::lock_guard lock(mutex_);
      if (connection == kEveryConnection && connections_.empty())
        return;
      line += '\n';
      queued_.push_back({connection, std::move(line)});
    }
    wake();
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
    for (std::size_t end; (end = partial.find('\n')) != std::string::npos;) {
      std::string line = partial.substr(0, end);
      partial.erase(0, end + 1);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (handlers_.onLine)
        handlers_.onLine(connection.id(), std::move(line));
    }
  }
  void pump(Connection &connection) override {
    std::lock_guard lock(mutex_);
    // Each queued line to its connection (or, for every connection, to
    // each, the last of them taking it off the queue)
    std::vector<Queued> kept;
    for (Queued &queued : queued_) {
      const bool sentAlready =
          std::ranges::find(queued.sentTo, connection.id()) != queued.sentTo.end();
      if (queued.connection == connection.id() ||
          (queued.connection == kEveryConnection && !sentAlready)) {
        connection.send(queued.line);
        if (queued.connection == kEveryConnection) {
          queued.sentTo.push_back(connection.id());
          if (queued.sentTo.size() < connections_.size()) {
            kept.push_back(std::move(queued));
            continue;
          }
        }
        continue;
      }
      kept.push_back(std::move(queued));
    }
    queued_ = std::move(kept);
  }
  void disconnected(Connection &connection) override {
    {
      std::lock_guard lock(mutex_);
      std::erase(connections_, connection.id());
      for (Queued &queued : queued_)
        std::erase(queued.sentTo, connection.id());
      std::erase_if(queued_, [&](const Queued &queued) {
        return queued.connection == connection.id() ||
               (queued.connection == kEveryConnection && connections_.empty());
      });
    }
    partial_.erase(connection.id());
    if (handlers_.onDisconnected)
      handlers_.onDisconnected(connection.id());
  }

private:
  struct Queued {
    std::uint32_t connection;
    std::string line;
    std::vector<std::uint32_t> sentTo; // (for every connection: those it's gone to)
  };
  std::string name_;
  Endpoint endpoint_;
  Handlers handlers_;
  std::mutex mutex_; // the queue and the connections
  std::vector<Queued> queued_;
  std::vector<std::uint32_t> connections_;
  std::unordered_map<std::uint32_t, std::string> partial_; // (the loop's thread)
};

} // namespace ScratchPadd::Network
