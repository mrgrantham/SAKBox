// LogServer.hpp - a LogBuffer served over the network, for a program that
// doesn't run padds: whoever connects to its port (tools/console_log, or nc)
// gets what's kept, then each line as it comes. Several readers at once.
//
//   DebugLog::LogBuffer buffer;
//   DebugLog::LogServer server(buffer);   // port 5005
//   if (!server.listening()) printf("log not served: %s\n", server.error().c_str());
//
// It's the network library's text stream (Network::StreamService) on a loop
// of its own (Network::Loop), so it works wherever the network library does:
// macOS, Linux, Windows and the 3DS. Not on a DS. (An app's log is served
// the same way by its DebugLog::Session, through NetworkPadd.)
#pragma once

#include <ScratchPadd/DebugLog/LogBuffer.hpp>
#include <ScratchPadd/Network/Loop.hpp>
#include <ScratchPadd/Network/StreamService.hpp>

#include <cstdint>
#include <string>

namespace ScratchPadd::DebugLog {

class LogServer {
public:
  // The port it listens on (FBI and ftpd use 5000)
  static constexpr std::uint16_t kPort = 5005;

  // Starts serving: returns once it's listening, or knows why it can't.
  // Port 0 means any free one; port() then says which.
  explicit LogServer(LogBuffer &buffer, std::uint16_t port = kPort);
  ~LogServer();
  LogServer(const LogServer &) = delete;
  LogServer &operator=(const LogServer &) = delete;

  bool listening() const { return error_.empty(); }
  // Why it isn't listening
  const std::string &error() const { return error_; }
  // Where to connect: the device's address and the port ("192.168.4.25:5005")
  std::string address() const;
  std::uint16_t port() const { return port_; }

private:
  LogBuffer &buffer_;
  std::uint16_t port_;
  std::string error_;
  Network::StreamService stream_; // (declared before the loop: the loop goes first)
  Network::Loop loop_;
};

} // namespace ScratchPadd::DebugLog
