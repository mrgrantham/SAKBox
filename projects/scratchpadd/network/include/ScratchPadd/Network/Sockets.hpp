// Sockets.hpp - TCP and UDP sockets the same on every platform ScratchPadd
// networks on: POSIX (macOS, Linux), Winsock (Windows) and the 3DS's soc.
// Not on a DS (a DS Lite can't join a WPA2 network). Plain C++, for
// NetworkPadd's I/O loop and anyone else.
//
//   std::string error;
//   if (!Network::start(error)) ...                  // the 3DS's soc, Winsock
//   Network::Socket server = Network::Socket::listenTcp(5006, error);
//   Network::Socket client = Network::Socket::connectTcp("192.168.4.25", 5006, error);
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#if defined(__NDS__)
#define SCRATCHPADD_HAS_NETWORK 0
#else
#define SCRATCHPADD_HAS_NETWORK 1
#endif

namespace ScratchPadd::Network {

// Starts the platform's network, once for the app's life (the 3DS's soc,
// which takes a buffer of its own; Winsock). False, with `error`, if there's
// no network (Wi-Fi off, or a DS).
bool start(std::string &error);

// This device's address on its network ("192.168.4.25"), as others reach
// it; "127.0.0.1" if it has no other
std::string deviceAddress();

class Socket {
public:
#if defined(_WIN32)
  using Handle = std::uintptr_t; // (Winsock's SOCKET)
#else
  using Handle = int;
#endif
  static constexpr Handle kNone = static_cast<Handle>(-1);

  Socket() = default;
  ~Socket();
  Socket(Socket &&other) noexcept;
  Socket &operator=(Socket &&other) noexcept;
  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  // A TCP socket listening on `port` (0: any free one, port() says which),
  // on every address; or an invalid one, with `error`
  static Socket listenTcp(std::uint16_t port, std::string &error);
  // A TCP connection to `host` (a dotted address or a name) on `port`
  static Socket connectTcp(const std::string &host, std::uint16_t port, std::string &error);
  // A UDP socket on 127.0.0.1, any free port: something to poke a poll()
  // with (Waker)
  static Socket loopbackUdp(std::string &error);

  bool valid() const { return handle_ != kNone; }
  Handle handle() const { return handle_; }
  // The port it's bound to
  std::uint16_t port() const;

  // The next connection waiting on a listening socket (invalid if none)
  Socket accept() const;
  // Sends what it can of `bytes` without waiting: how many went, 0 if it
  // would have waited, -1 if the connection's gone
  long send(std::span<const char> bytes) const;
  // Receives what's there into `into`: how many came, 0 if the other side
  // closed, -1 if nothing's there (it would have waited) or it failed
  // (wouldBlock() says which)
  long receive(std::span<char> into) const;
  static bool wouldBlock();
  // Sends to its own port (a loopback UDP socket's poke)
  void poke() const;
  // Doesn't wait: send and receive return at once
  void setNonBlocking() const;
  void close();

private:
  explicit Socket(Handle handle) : handle_(handle) {}
  Handle handle_ = kNone;
};

// Waits up to `milliseconds` (-1: as long as it takes) for any of `sockets`
// to be readable, or writable where `wantWrite` says so; `readable` and
// `writable` say which are (each as long as `sockets`). False on failure.
bool waitForAny(const std::vector<Socket::Handle> &sockets, const std::vector<bool> &wantWrite,
                int milliseconds, std::vector<bool> &readable, std::vector<bool> &writable);

} // namespace ScratchPadd::Network
