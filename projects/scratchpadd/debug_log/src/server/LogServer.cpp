#if !defined(_WIN32) && !defined(__NDS__)
#include <ScratchPadd/DebugLog/LogServer.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__3DS__)
#include <3ds.h>

#include <cstdlib>
#include <malloc.h>
#endif

#include <cerrno>
#include <cstring>

namespace ScratchPadd::DebugLog {

namespace {

#if defined(__3DS__)
// The 3DS's network (soc), started once for the app's life: it needs a
// buffer of its own, page-aligned
bool startNetwork(std::string &error) {
  static const Result started = [] {
    constexpr std::size_t kSize = 0x100000;
    void *buffer = memalign(0x1000, kSize);
    return buffer ? socInit(static_cast<u32 *>(buffer), kSize) : -1;
  }();
  if (R_FAILED(started))
    error = "no network (Wi-Fi off?)";
  return R_SUCCEEDED(started);
}
#else
bool startNetwork(std::string &) { return true; }
#endif

// Sends all of `text`, or returns false (the reader's gone)
bool sendAll(int socket, const std::string &text) {
#if defined(MSG_NOSIGNAL)
  constexpr int kFlags = MSG_NOSIGNAL; // (a closed reader is an error, not a signal)
#else
  constexpr int kFlags = 0;
#endif
  std::size_t sent = 0;
  while (sent < text.size()) {
    const auto n = ::send(socket, text.data() + sent, text.size() - sent, kFlags);
    if (n <= 0)
      return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

} // namespace

LogServer::LogServer(LogBuffer &buffer, std::uint16_t port) : buffer_(buffer), port_(port) {
  if (!startNetwork(error_))
    return;
  socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket_ < 0) {
    error_ = std::string("no socket: ") + std::strerror(errno);
    return;
  }
  int yes = 1;
  ::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
#if defined(SO_NOSIGPIPE)
  ::setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
  sockaddr_in at{};
  at.sin_family = AF_INET;
  at.sin_port = htons(port_);
  at.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(socket_, reinterpret_cast<sockaddr *>(&at), sizeof at) < 0 ||
      ::listen(socket_, 1) < 0) {
    error_ = "port " + std::to_string(port_) + ": " + std::strerror(errno);
    ::close(socket_);
    socket_ = -1;
    return;
  }
  thread_ = std::thread([this] { serve(); });
}

LogServer::~LogServer() {
  stopping_ = true;
  if (thread_.joinable())
    thread_.join();
  if (socket_ >= 0)
    ::close(socket_);
}

std::string LogServer::address() const {
#if defined(__3DS__)
  in_addr ip{};
  ip.s_addr = static_cast<in_addr_t>(gethostid()); // (the 3DS's, in network order)
  return std::string(inet_ntoa(ip)) + ":" + std::to_string(port_);
#else
  return "localhost:" + std::to_string(port_);
#endif
}

void LogServer::serve() {
  while (!stopping_) {
    // A reader, checking every quarter second whether to stop
    pollfd waiting{socket_, POLLIN, 0};
    if (::poll(&waiting, 1, 250) <= 0)
      continue;
    const int reader = ::accept(socket_, nullptr, nullptr);
    if (reader < 0)
      continue;
#if defined(SO_NOSIGPIPE)
    int yes = 1;
    ::setsockopt(reader, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
    // What's kept, then each line as it comes, until the reader goes
    std::uint64_t position = 0;
    while (!stopping_) {
      const std::string text = buffer_.readFrom(position, std::chrono::milliseconds(250));
      if (!text.empty() && !sendAll(reader, text))
        break;
      // Gone? (readable with nothing to read: closed)
      pollfd closed{reader, POLLIN, 0};
      if (::poll(&closed, 1, 0) > 0) {
        char discard[64];
        if (::recv(reader, discard, sizeof discard, 0) <= 0)
          break;
      }
    }
    ::close(reader);
  }
}

} // namespace ScratchPadd::DebugLog
#endif
