// LogServer: a LogBuffer streamed by the network library (see LogServer.hpp)
#if !defined(__NDS__)
#include <ScratchPadd/DebugLog/LogServer.hpp>
#include <ScratchPadd/Network/Sockets.hpp>

#include <chrono>
#include <thread>

namespace ScratchPadd::DebugLog {

namespace {

// How long the constructor waits for the loop to open the port
constexpr std::chrono::seconds kOpenWait{3};

} // namespace

LogServer::LogServer(LogBuffer &buffer, std::uint16_t port)
    : buffer_(buffer), port_(port), stream_("log", port, [&buffer](std::uint64_t &position) {
        return buffer.readFrom(position);
      }) {
  if (!loop_.start(error_)) // (starts the network too: the 3DS's soc, Winsock)
    return;
  buffer_.setOnAppend([this] { stream_.wake(); }); // new text: the loop sends it
  loop_.attach(&stream_);

  // The loop opens the port on its own thread: wait to hear how that went
  const auto giveUp = std::chrono::steady_clock::now() + kOpenWait;
  while (std::chrono::steady_clock::now() < giveUp) {
    for (const Network::Loop::ServiceState &state : loop_.states()) {
      if (state.name != "log" || state.state.empty())
        continue;
      if (state.state == "listening")
        port_ = state.port;
      else
        error_ = "port " + std::to_string(port) + ": " + state.state;
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  error_ = "port " + std::to_string(port) + ": didn't open in time";
}

LogServer::~LogServer() {
  buffer_.setOnAppend(nullptr);
  loop_.stop();
}

std::string LogServer::address() const {
  return Network::deviceAddress() + ":" + std::to_string(port_);
}

} // namespace ScratchPadd::DebugLog
#endif
