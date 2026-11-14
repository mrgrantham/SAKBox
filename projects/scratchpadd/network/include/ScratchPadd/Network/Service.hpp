// Service.hpp - something an app does over the network, which NetworkPadd's
// I/O loop (Loop) runs: it listens on a port or connects to one, and
// handles its connections' bytes. The loop does the waiting and the
// sending; a service only reacts, on the loop's thread:
//   connected(connection), received(connection, bytes), pump(connection)
//   (each pass: anything new to send), disconnected(connection).
// From any other thread a service rings wake() when it has something new
// for pump() to send. The first services are StreamService (text out to
// whoever connects: the log) and LineService (lines both ways: remote
// controls).
//
// Services made outside the padds (the log's, in main) are registered with
// services(), where NetworkPadd finds them, as Storage's sources are.
#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ScratchPadd::Network {

// One TCP connection a service has, on the loop's thread
class Connection {
public:
  explicit Connection(std::uint32_t id) : id_(id) {}
  std::uint32_t id() const { return id_; }
  // Queued; the loop sends it as the connection takes it
  void send(std::string_view bytes) { outbox_.append(bytes); }
  // Closed once what's queued has gone
  void close() { closing_ = true; }

private:
  friend class Loop;
  std::uint32_t id_;
  std::string outbox_;
  bool closing_ = false;
};

class Service {
public:
  virtual ~Service() = default;
  // For panels and logs: "log", "remote controls"
  virtual std::string name() const = 0;
  // Where: listening on `port` (an empty host), or connecting to host:port
  struct Endpoint {
    std::string host;
    std::uint16_t port = 0;
  };
  virtual Endpoint endpoint() const = 0;

  // On the loop's thread
  virtual void connected(Connection &) {}
  virtual void received(Connection &, std::string_view) {}
  virtual void pump(Connection &) {}
  virtual void disconnected(Connection &) {}

  // The loop's doorbell, which the loop gives a service it runs; wake()
  // from any thread when there's something new for pump()
  void attach(std::function<void()> wake) {
    std::lock_guard lock(wakeMutex_);
    wake_ = std::move(wake);
  }
  void detach() { attach(nullptr); }
  void wake() const {
    std::lock_guard lock(wakeMutex_);
    if (wake_)
      wake_();
  }

private:
  mutable std::mutex wakeMutex_;
  std::function<void()> wake_;
};

// The services registered so far, for NetworkPadd. Thread-safe. `onChange`
// (one listener, NetworkPadd's) is called on the registering thread after
// each add or remove, and once remove() returns the loop has let go of it.
class Services {
public:
  void add(Service *service);
  void remove(Service *service);
  std::vector<Service *> all() const;
  void listen(std::function<void(Service *added, Service *removed)> onChange);

private:
  mutable std::mutex mutex_;
  std::vector<Service *> services_;
  std::function<void(Service *, Service *)> onChange_;
};

// The app's
Services &services();

} // namespace ScratchPadd::Network
