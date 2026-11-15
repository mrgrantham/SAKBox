// LogServer on this machine: a reader gets what's kept, then each line as it
// comes; another reader, at the same time or later, gets it all from the
// start; a port that's taken is refused with why.
#include <ScratchPadd/DebugLog/LogServer.hpp>
#include <ScratchPadd/Network/Sockets.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

using namespace ScratchPadd;
using namespace ScratchPadd::DebugLog;
using namespace std::chrono_literals;

namespace {

Network::Socket connectTo(std::uint16_t port) {
  std::string error;
  Network::Socket reader = Network::Socket::connectTcp("127.0.0.1", port, error);
  EXPECT_TRUE(reader.valid()) << error;
  return reader;
}

// What arrives on `reader` until there are `length` bytes, or 2 s pass
std::string readUntil(const Network::Socket &reader, std::size_t length) {
  std::string got;
  char chunk[256];
  for (auto giveUp = std::chrono::steady_clock::now() + 2s;
       got.size() < length && std::chrono::steady_clock::now() < giveUp;
       std::this_thread::sleep_for(5ms)) {
    for (long n; (n = reader.receive(chunk)) > 0;)
      got.append(chunk, static_cast<std::size_t>(n));
  }
  return got;
}

} // namespace

TEST(LogServerTest, ServesWhatsKeptThenEachLine) {
  LogBuffer buffer;
  LogServer server(buffer, 0); // any free port (tests may run side by side)
  ASSERT_TRUE(server.listening()) << server.error();
  ASSERT_NE(server.port(), 0);
  EXPECT_EQ(server.address(), Network::deviceAddress() + ":" + std::to_string(server.port()));

  buffer.append("before\n");
  Network::Socket first = connectTo(server.port());
  first.setNonBlocking();
  EXPECT_EQ(readUntil(first, 7), "before\n");
  buffer.append("after\n");
  EXPECT_EQ(readUntil(first, 6), "after\n");

  // A second reader, while the first is still connected, gets everything kept
  Network::Socket second = connectTo(server.port());
  second.setNonBlocking();
  EXPECT_EQ(readUntil(second, 13), "before\nafter\n");
}

TEST(LogServerTest, SaysWhyItCantListen) {
  LogBuffer buffer;
  LogServer first(buffer, 0);
  ASSERT_TRUE(first.listening()) << first.error();
  LogBuffer other;
  LogServer second(other, first.port()); // the port taken
  EXPECT_FALSE(second.listening());
  EXPECT_NE(second.error().find(std::to_string(first.port())), std::string::npos) << second.error();
}
