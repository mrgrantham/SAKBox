// Loop.hpp - NetworkPadd's I/O loop: one thread that waits on every
// socket at once (each service's listening socket or outgoing connection,
// every connection, and a wake socket a service's wake() pokes), and does
// the accepting, receiving and sending for the services attached to it.
// Plain C++ (no padds), so it's tested on its own. Not on a DS.
#pragma once

#include <ScratchPadd/Network/Service.hpp>
#include <ScratchPadd/Network/Sockets.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ScratchPadd::Network {

class Loop {
public:
  // What each service is doing, for NetworkPadd's tab
  struct ServiceState {
    std::string name;
    std::string where; // "port 5005", "192.168.4.25:5006"
    std::uint16_t port =
        0;             // the port it listens on (the one it got, if asked for any) or connects to
    std::string state; // "listening", "connecting", "connected", or why not
    unsigned connections = 0;
    std::uint64_t sent = 0, received = 0; // bytes
  };
  // How long an outgoing connection waits before trying again
  static constexpr std::chrono::seconds kRetry{2};

  Loop();
  ~Loop(); // stop()
  Loop(const Loop &) = delete;
  Loop &operator=(const Loop &) = delete;

  // Starts the network and the loop's thread; false, with `error`, if it
  // can't (no network)
  bool start(std::string &error);
  // Lets go of every service (each connection's disconnected()) and joins
  void stop();
  bool running() const { return thread_.joinable(); }

  // From any thread. attach() returns at once; detach() returns once the
  // loop has let go of `service` (its connections closed), so it can go.
  void attach(Service *service);
  void detach(Service *service);

  std::vector<ServiceState> states() const;

private:
  struct Attached;
  struct Open;
  void run();
  void wake() const;
  void applyRequests();
  void open(Attached &attached);
  void letGo(Attached &attached);

  Socket waker_;
  std::thread thread_;
  std::atomic<bool> stopping_{false};
  std::uint32_t nextConnection_ = 1;

  mutable std::mutex mutex_; // the requests, and the states
  std::condition_variable letGo_;
  std::vector<Service *> toAttach_, toDetach_;
  std::vector<Service *> detached_;                 // let go since asked
  std::vector<std::unique_ptr<Attached>> attached_; // (the loop's thread only)
  std::vector<ServiceState> states_;
};

} // namespace ScratchPadd::Network
