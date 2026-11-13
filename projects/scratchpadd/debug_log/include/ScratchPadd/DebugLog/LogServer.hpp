// LogServer.hpp - a LogBuffer served over the network: whoever connects to
// its port (tools/console_log, or nc) gets what's kept, then each line as
// it comes, until they disconnect; then the next one may. One at a time.
// On a 3DS it starts the network (soc) itself. Not on Windows or a DS.
#pragma once

#include <ScratchPadd/DebugLog/LogBuffer.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace ScratchPadd::DebugLog {

class LogServer {
public:
  // The port it listens on (FBI and ftpd use 5000)
  static constexpr std::uint16_t kPort = 5005;

  // Port 0 means any free one; port() then says which
  explicit LogServer(LogBuffer &buffer, std::uint16_t port = kPort);
  ~LogServer();
  LogServer(const LogServer &) = delete;
  LogServer &operator=(const LogServer &) = delete;

  bool listening() const { return socket_ >= 0; }
  // Why it isn't listening
  const std::string &error() const { return error_; }
  // Where to connect: the device's address and the port ("192.168.4.25:5005")
  std::string address() const;
  std::uint16_t port() const { return port_; }

private:
  void serve();
  LogBuffer &buffer_;
  std::uint16_t port_;
  int socket_ = -1;
  std::string error_;
  std::atomic<bool> stopping_{false};
  std::thread thread_;
};

} // namespace ScratchPadd::DebugLog
