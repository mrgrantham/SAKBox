#include <ScratchPadd/Network/Sockets.hpp>

#if SCRATCHPADD_HAS_NETWORK

#include <cerrno>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(__3DS__)
#include <3ds.h>

#include <malloc.h>
#endif

namespace ScratchPadd::Network {

namespace {

#if defined(_WIN32)
using Length = int;
int lastError() { return WSAGetLastError(); }
void closeHandle(Socket::Handle handle) { ::closesocket(static_cast<SOCKET>(handle)); }
#else
using Length = socklen_t;
int lastError() { return errno; }
void closeHandle(Socket::Handle handle) { ::close(handle); }
#endif

std::string describeError(int code) {
#if defined(_WIN32)
  return "socket error " + std::to_string(code);
#else
  return std::strerror(code);
#endif
}

// (Winsock's SOCKET, or a file descriptor, as the platform's call takes it)
auto native(Socket::Handle handle) {
#if defined(_WIN32)
  return static_cast<SOCKET>(handle);
#else
  return handle;
#endif
}

sockaddr_in addressOf(std::uint32_t hostOrder, std::uint16_t port) {
  sockaddr_in at{};
  at.sin_family = AF_INET;
  at.sin_port = htons(port);
  at.sin_addr.s_addr = htonl(hostOrder);
  return at;
}

} // namespace

bool start(std::string &error) {
#if defined(__3DS__)
  // soc needs a buffer of its own, page-aligned, for the app's life
  static const Result started = [] {
    constexpr std::size_t kSize = 0x100000;
    void *buffer = memalign(0x1000, kSize);
    return buffer ? socInit(static_cast<u32 *>(buffer), kSize) : -1;
  }();
  if (R_FAILED(started))
    error = "no network (Wi-Fi off?)";
  return R_SUCCEEDED(started);
#elif defined(_WIN32)
  static const int started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data);
  }();
  if (started != 0)
    error = "Winsock didn't start (" + std::to_string(started) + ")";
  return started == 0;
#else
  (void)error;
  return true;
#endif
}

std::string deviceAddress() {
#if defined(__3DS__)
  in_addr ip{};
  ip.s_addr = static_cast<in_addr_t>(gethostid()); // (the 3DS's, in network order)
  return inet_ntoa(ip);
#else
  // The address this device would send from: a UDP socket "connected" to an
  // outside address (nothing is sent) knows it
  std::string error;
  if (!start(error))
    return "127.0.0.1";
  const auto handle = ::socket(AF_INET, SOCK_DGRAM, 0);
  std::string address = "127.0.0.1";
  sockaddr_in outside = addressOf(0x08080808, 53); // (8.8.8.8: any outside address)
  if (::connect(handle, reinterpret_cast<sockaddr *>(&outside), sizeof outside) == 0) {
    sockaddr_in local{};
    Length size = sizeof local;
    char text[INET_ADDRSTRLEN] = {};
    if (::getsockname(handle, reinterpret_cast<sockaddr *>(&local), &size) == 0 &&
        ::inet_ntop(AF_INET, &local.sin_addr, text, sizeof text))
      address = text;
  }
  closeHandle(static_cast<Socket::Handle>(handle));
  return address;
#endif
}

Socket::~Socket() { close(); }

Socket::Socket(Socket &&other) noexcept : handle_(std::exchange(other.handle_, kNone)) {}

Socket &Socket::operator=(Socket &&other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, kNone);
  }
  return *this;
}

void Socket::close() {
  if (handle_ != kNone)
    closeHandle(std::exchange(handle_, kNone));
}

Socket Socket::listenTcp(std::uint16_t port, std::string &error) {
  if (!start(error))
    return {};
  Socket socket(static_cast<Handle>(::socket(AF_INET, SOCK_STREAM, 0)));
  if (!socket.valid()) {
    error = "no socket: " + describeError(lastError());
    return {};
  }
  int yes = 1;
  ::setsockopt(native(socket.handle_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&yes), sizeof yes);
#if defined(SO_NOSIGPIPE)
  ::setsockopt(socket.handle_, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
  sockaddr_in at = addressOf(INADDR_ANY, port);
  if (::bind(native(socket.handle_), reinterpret_cast<sockaddr *>(&at), sizeof at) != 0 ||
      ::listen(native(socket.handle_), 4) != 0) {
    error = "port " + std::to_string(port) + ": " + describeError(lastError());
    return {};
  }
  socket.setNonBlocking();
  return socket;
}

Socket Socket::connectTcp(const std::string &host, std::uint16_t port, std::string &error) {
  if (!start(error))
    return {};
  sockaddr_in at = addressOf(0, port);
  if (::inet_pton(AF_INET, host.c_str(), &at.sin_addr) != 1) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *found = nullptr;
    if (::getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || !found) {
      error = "can't find " + host;
      return {};
    }
    at.sin_addr = reinterpret_cast<sockaddr_in *>(found->ai_addr)->sin_addr;
    ::freeaddrinfo(found);
  }
  Socket socket(static_cast<Handle>(::socket(AF_INET, SOCK_STREAM, 0)));
  if (!socket.valid()) {
    error = "no socket: " + describeError(lastError());
    return {};
  }
#if defined(SO_NOSIGPIPE)
  int yes = 1;
  ::setsockopt(socket.handle_, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
  if (::connect(native(socket.handle_), reinterpret_cast<sockaddr *>(&at), sizeof at) != 0) {
    error = host + ":" + std::to_string(port) + ": " + describeError(lastError());
    return {};
  }
  socket.setNonBlocking();
  return socket;
}

Socket Socket::loopbackUdp(std::string &error) {
  if (!start(error))
    return {};
  Socket socket(static_cast<Handle>(::socket(AF_INET, SOCK_DGRAM, 0)));
  sockaddr_in at = addressOf(INADDR_LOOPBACK, 0);
  if (!socket.valid() ||
      ::bind(native(socket.handle_), reinterpret_cast<sockaddr *>(&at), sizeof at) != 0) {
    error = "no loopback socket: " + describeError(lastError());
    return {};
  }
  socket.setNonBlocking();
  return socket;
}

std::uint16_t Socket::port() const {
  sockaddr_in at{};
  Length size = sizeof at;
  if (!valid() || ::getsockname(native(handle_), reinterpret_cast<sockaddr *>(&at), &size) != 0)
    return 0;
  return ntohs(at.sin_port);
}

Socket Socket::accept() const {
  Socket connection(static_cast<Handle>(::accept(native(handle_), nullptr, nullptr)));
  if (connection.valid()) {
#if defined(SO_NOSIGPIPE)
    int yes = 1;
    ::setsockopt(connection.handle_, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes);
#endif
    connection.setNonBlocking();
  }
  return connection;
}

long Socket::send(std::span<const char> bytes) const {
#if defined(MSG_NOSIGNAL)
  constexpr int kFlags = MSG_NOSIGNAL; // (a closed reader is an error, not a signal)
#else
  constexpr int kFlags = 0;
#endif
  const auto sent = ::send(native(handle_), bytes.data(), static_cast<int>(bytes.size()), kFlags);
  if (sent >= 0)
    return static_cast<long>(sent);
  return wouldBlock() ? 0 : -1;
}

long Socket::receive(std::span<char> into) const {
  return static_cast<long>(::recv(native(handle_), into.data(), static_cast<int>(into.size()), 0));
}

bool Socket::wouldBlock() {
#if defined(_WIN32)
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

void Socket::poke() const {
  sockaddr_in at = addressOf(INADDR_LOOPBACK, port());
  const char byte = 1;
  ::sendto(native(handle_), &byte, 1, 0, reinterpret_cast<sockaddr *>(&at), sizeof at);
}

void Socket::setNonBlocking() const {
#if defined(_WIN32)
  u_long yes = 1;
  ::ioctlsocket(native(handle_), FIONBIO, &yes);
#else
  ::fcntl(handle_, F_SETFL, ::fcntl(handle_, F_GETFL, 0) | O_NONBLOCK);
#endif
}

bool waitForAny(const std::vector<Socket::Handle> &sockets, const std::vector<bool> &wantWrite,
                int milliseconds, std::vector<bool> &readable, std::vector<bool> &writable) {
  std::vector<pollfd> waiting(sockets.size());
  for (std::size_t i = 0; i < sockets.size(); ++i) {
    waiting[i].fd = native(sockets[i]);
    waiting[i].events = static_cast<short>(POLLIN | (wantWrite[i] ? POLLOUT : 0));
  }
#if defined(_WIN32)
  const int ready = ::WSAPoll(waiting.data(), static_cast<ULONG>(waiting.size()), milliseconds);
#else
  const int ready = ::poll(waiting.data(), static_cast<nfds_t>(waiting.size()), milliseconds);
#endif
  readable.assign(sockets.size(), false);
  writable.assign(sockets.size(), false);
  if (ready < 0)
    return false;
  for (std::size_t i = 0; i < sockets.size(); ++i) {
    // (a closed or failed connection counts as readable: reading says so)
    readable[i] = (waiting[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    writable[i] = (waiting[i].revents & POLLOUT) != 0;
  }
  return true;
}

} // namespace ScratchPadd::Network

#endif // SCRATCHPADD_HAS_NETWORK
