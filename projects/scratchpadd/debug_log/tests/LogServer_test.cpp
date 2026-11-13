// LogServer on this machine's localhost: a reader gets what's kept, then
// each line as it comes; after it disconnects, the next reader gets it all.
#include <ScratchPadd/DebugLog/LogServer.hpp>

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>

using namespace ScratchPadd::DebugLog;
using namespace std::chrono_literals;

namespace {

// A reader connected to `port` on localhost, or -1
int connectTo(std::uint16_t port) {
  const int s = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in at{};
  at.sin_family = AF_INET;
  at.sin_port = htons(port);
  at.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(s, reinterpret_cast<sockaddr *>(&at), sizeof at) < 0) {
    ::close(s);
    return -1;
  }
  return s;
}

// What arrives on `s` until it ends with `expected`'s length, or 2 s pass
std::string readUntil(int s, std::size_t length) {
  std::string got;
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (got.size() < length && std::chrono::steady_clock::now() < deadline) {
    pollfd ready{s, POLLIN, 0};
    if (::poll(&ready, 1, 50) <= 0)
      continue;
    char chunk[256];
    const auto n = ::recv(s, chunk, sizeof chunk, 0);
    if (n <= 0)
      break;
    got.append(chunk, static_cast<std::size_t>(n));
  }
  return got;
}

} // namespace

TEST(LogServerTest, ServesWhatsKeptThenEachLine) {
  LogBuffer buffer;
  constexpr std::uint16_t kTestPort = 15005; // (not the real port, in use or not)
  LogServer server(buffer, kTestPort);
  ASSERT_TRUE(server.listening()) << server.error();
  EXPECT_EQ(server.address(), "localhost:15005");

  buffer.append("before\n");
  const int reader = connectTo(kTestPort);
  ASSERT_GE(reader, 0);
  EXPECT_EQ(readUntil(reader, 7), "before\n");
  buffer.append("after\n");
  EXPECT_EQ(readUntil(reader, 6), "after\n");
  ::close(reader);

  // The next reader, once the first has gone, gets everything kept
  std::string all;
  const auto deadline = std::chrono::steady_clock::now() + 3s;
  while (all.empty() && std::chrono::steady_clock::now() < deadline) {
    const int next = connectTo(kTestPort);
    if (next >= 0) {
      all = readUntil(next, 13);
      ::close(next);
    }
  }
  EXPECT_EQ(all, "before\nafter\n");
}

TEST(LogServerTest, SaysWhyItCantListen) {
  LogBuffer buffer;
  LogServer first(buffer, 15006);
  ASSERT_TRUE(first.listening());
  LogServer second(buffer, 15006); // the port taken
  EXPECT_FALSE(second.listening());
  EXPECT_NE(second.error().find("15006"), std::string::npos);
}
