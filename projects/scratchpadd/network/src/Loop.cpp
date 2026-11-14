#include <ScratchPadd/Network/Loop.hpp>

#if SCRATCHPADD_HAS_NETWORK

#include <algorithm>
#include <array>

namespace ScratchPadd::Network {

// A connection and its socket
struct Loop::Open {
  Socket socket;
  Connection connection;
};

// A service the loop runs: its listening socket or outgoing connection, its
// connections, and what it's done
struct Loop::Attached {
  Service *service;
  Service::Endpoint endpoint;
  Socket listening;                                // (listening services)
  Socket connecting;                               // (connecting services: on its way)
  std::vector<std::unique_ptr<Open>> open;         // its connections
  std::chrono::steady_clock::time_point retryAt{}; // (connecting services)
  std::string state;
  std::uint64_t sent = 0, received = 0;
  bool listens() const { return endpoint.host.empty(); }
};

Loop::Loop(bool wakeSocket) : wakeSocket_(wakeSocket) {}
Loop::~Loop() { stop(); }

bool Loop::start(std::string &error) {
  if (running())
    return true;
  if (!Network::start(error))
    return false;
  // (without a wake socket it still runs, looking for work every
  // kWithoutWaker instead of being woken)
  std::string why;
  if (wakeSocket_)
    waker_ = Socket::wakeUdp(why);
  else
    why = "none asked for";
  wakerProblem_ = waker_.valid() ? "" : why;
  stopping_ = false;
  thread_ = std::thread([this] { run(); });
  return true;
}

void Loop::stop() {
  if (!running())
    return;
  stopping_ = true;
  wake();
  thread_.join();
  waker_.close();
}

void Loop::attach(Service *service) {
  {
    std::lock_guard lock(mutex_);
    std::erase(toDetach_, service);
    toAttach_.push_back(service);
  }
  wake();
}

void Loop::detach(Service *service) {
  std::unique_lock lock(mutex_);
  std::erase(toAttach_, service);
  if (!running() || std::this_thread::get_id() == thread_.get_id()) {
    // (no loop to wait for; or called from the loop itself, in a callback,
    // where it lets go when the callback returns)
    toDetach_.push_back(service);
    return;
  }
  toDetach_.push_back(service);
  lock.unlock();
  wake();
  lock.lock();
  letGo_.wait(
      lock, [&] { return std::ranges::find(detached_, service) != detached_.end() || !running(); });
  std::erase(detached_, service);
}

std::vector<Loop::ServiceState> Loop::states() const {
  std::lock_guard lock(mutex_);
  return states_;
}

void Loop::wake() const {
  if (waker_.valid())
    waker_.poke();
}

void Loop::open(Attached &attached) {
  std::string error;
  if (attached.listens()) {
    attached.listening = Socket::listenTcp(attached.endpoint.port, error);
    attached.state = attached.listening.valid() ? "listening" : error;
    return;
  }
  // An outgoing connection: started now, waited for with the other sockets
  // (connected()), and tried again kRetry after it fails (the other side
  // may not be there yet)
  bool pending = false;
  Socket socket =
      Socket::connectTcp(attached.endpoint.host, attached.endpoint.port, error, &pending);
  if (!socket.valid()) {
    attached.state = "not connected: " + error;
    attached.retryAt = std::chrono::steady_clock::now() + kRetry;
    return;
  }
  if (pending) {
    attached.state = "connecting";
    attached.connecting = std::move(socket);
    return;
  }
  connected(attached, std::move(socket));
}

void Loop::connected(Attached &attached, Socket socket) {
  attached.state = "connected";
  auto open = std::make_unique<Open>(Open{std::move(socket), Connection(nextConnection_++)});
  attached.service->connected(open->connection);
  attached.open.push_back(std::move(open));
}

void Loop::letGo(Attached &attached) {
  for (auto &open : attached.open)
    attached.service->disconnected(open->connection);
  attached.open.clear();
  attached.listening.close();
  attached.connecting.close();
  attached.service->detach();
}

void Loop::applyRequests() {
  std::vector<Service *> attach, detach;
  {
    std::lock_guard lock(mutex_);
    attach.swap(toAttach_);
    detach.swap(toDetach_);
  }
  for (Service *service : detach) {
    auto found = std::ranges::find_if(
        attached_, [&](const auto &attached) { return attached->service == service; });
    if (found != attached_.end()) {
      letGo(**found);
      attached_.erase(found);
    }
  }
  if (!detach.empty()) {
    std::lock_guard lock(mutex_);
    detached_.insert(detached_.end(), detach.begin(), detach.end());
    letGo_.notify_all();
  }
  for (Service *service : attach) {
    if (std::ranges::any_of(attached_, [&](const auto &a) { return a->service == service; }))
      continue;
    auto attached = std::make_unique<Attached>();
    attached->service = service;
    attached->endpoint = service->endpoint();
    service->attach([this] { wake(); });
    open(*attached);
    attached_.push_back(std::move(attached));
  }
}

void Loop::run() {
  std::array<char, 4096> chunk;
  while (!stopping_) {
    applyRequests();
    const auto now = std::chrono::steady_clock::now();
    for (auto &attached : attached_) {
      if (!attached->listens() && attached->open.empty() && !attached->connecting.valid() &&
          now >= attached->retryAt)
        open(*attached); // (connecting again)
      for (auto &open : attached->open)
        attached->service->pump(open->connection); // anything new to send
    }

    // Every socket: the waker first, then each service's listening socket
    // and connections
    // (the waker first, if there is one)
    std::vector<Socket::Handle> sockets;
    std::vector<bool> wantWrite;
    if (waker_.valid()) {
      sockets.push_back(waker_.handle());
      wantWrite.push_back(false);
    }
    for (auto &attached : attached_) {
      if (attached->listening.valid()) {
        sockets.push_back(attached->listening.handle());
        wantWrite.push_back(false);
      }
      if (attached->connecting.valid()) { // (writable once made, or failed)
        sockets.push_back(attached->connecting.handle());
        wantWrite.push_back(true);
      }
      for (auto &open : attached->open) {
        sockets.push_back(open->socket.handle());
        wantWrite.push_back(!open->connection.outbox_.empty());
      }
    }
    std::vector<bool> readable, writable;
    // A second at most (outgoing connections are retried, and the states
    // kept up to date); without a waker, kWithoutWaker, to see new work
    const int wait = waker_.valid() ? 1000 : static_cast<int>(kWithoutWaker.count());
    if (sockets.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(wait)); // (WSAPoll takes no empty list)
      continue;
    }
    if (!waitForAny(sockets, wantWrite, wait, readable, writable))
      continue;
    std::size_t at = 0;
    if (waker_.valid() && readable[at++])
      while (waker_.receive(chunk) > 0) {
      } // (the pokes drained)

    for (auto &attached : attached_) {
      Service &service = *attached->service;
      // (those accepted below weren't polled: they're read from next pass)
      const std::size_t polled = attached->open.size();
      if (attached->connecting.valid()) {
        const std::size_t index = at++;
        if (readable[index] || writable[index]) {
          std::string error;
          Socket socket = std::move(attached->connecting);
          if (socket.finishConnecting(error)) {
            connected(*attached, std::move(socket));
          } else {
            attached->state = "not connected: " + error;
            attached->retryAt = std::chrono::steady_clock::now() + kRetry;
          }
        }
      }
      if (attached->listening.valid()) {
        if (readable[at++]) {
          for (Socket socket = attached->listening.accept(); socket.valid();
               socket = attached->listening.accept()) {
            auto open =
                std::make_unique<Open>(Open{std::move(socket), Connection(nextConnection_++)});
            service.connected(open->connection);
            service.pump(open->connection);
            attached->open.push_back(std::move(open));
          }
        }
      }
      for (std::size_t o = 0; o < polled; ++o) {
        auto &open = attached->open[o];
        const std::size_t index = at++;
        bool gone = false;
        if (readable[index]) {
          const long got = open->socket.receive(chunk);
          if (got > 0) {
            attached->received += static_cast<std::uint64_t>(got);
            service.received(open->connection,
                             std::string_view(chunk.data(), static_cast<std::size_t>(got)));
            service.pump(open->connection);
          } else if (got == 0 || !Socket::wouldBlock()) {
            gone = true;
          }
        }
        std::string &outbox = open->connection.outbox_;
        while (!gone && !outbox.empty()) {
          const long sent = open->socket.send(outbox);
          if (sent < 0)
            gone = true;
          else if (sent == 0)
            break; // (full: when it's writable again)
          else {
            attached->sent += static_cast<std::uint64_t>(sent);
            outbox.erase(0, static_cast<std::size_t>(sent));
          }
        }
        if (gone || (open->connection.closing_ && outbox.empty())) {
          service.disconnected(open->connection);
          open->socket.close();
        }
      }
      const auto before = attached->open.size();
      std::erase_if(attached->open, [](const auto &open) { return !open->socket.valid(); });
      if (!attached->listens() && before > 0 && attached->open.empty()) {
        attached->state = "disconnected";
        attached->retryAt = std::chrono::steady_clock::now() + kRetry;
      }
    }

    std::vector<ServiceState> states;
    for (const auto &attached : attached_) {
      const auto &e = attached->endpoint;
      const std::uint16_t port = attached->listening.valid() ? attached->listening.port() : e.port;
      states.push_back({.name = attached->service->name(),
                        .where = e.host.empty() ? "port " + std::to_string(port)
                                                : e.host + ":" + std::to_string(port),
                        .port = port,
                        .state = attached->state,
                        .connections = static_cast<unsigned>(attached->open.size()),
                        .sent = attached->sent,
                        .received = attached->received});
    }
    std::lock_guard lock(mutex_);
    states_ = std::move(states);
  }
  for (auto &attached : attached_)
    letGo(*attached);
  attached_.clear();
  std::lock_guard lock(mutex_);
  for (Service *service : toDetach_)
    detached_.push_back(service);
  toDetach_.clear();
  letGo_.notify_all();
}

} // namespace ScratchPadd::Network

#else // a DS: no network, a loop that does nothing

namespace ScratchPadd::Network {
struct Loop::Attached {};
Loop::Loop(bool wakeSocket) : wakeSocket_(wakeSocket) {}
Loop::~Loop() = default;
bool Loop::start(std::string &error) {
  error = "no network on a DS";
  return false;
}
void Loop::stop() {}
void Loop::attach(Service *) {}
void Loop::detach(Service *) {}
std::vector<Loop::ServiceState> Loop::states() const { return {}; }
} // namespace ScratchPadd::Network

#endif // SCRATCHPADD_HAS_NETWORK
