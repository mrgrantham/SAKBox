// The network loop on this machine's localhost, without padds: a stream
// served to readers from the start and as it grows; lines both ways between
// a listening service and a connecting one, through two loops; a connecting
// service waiting for its listener; letting go of a service closing its
// connections.
#include <ScratchPadd/Network/LineService.hpp>
#include <ScratchPadd/Network/Loop.hpp>
#include <ScratchPadd/Network/StreamService.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace Network = ScratchPadd::Network;

namespace {

template <typename Done> bool eventually(Done done, std::chrono::milliseconds within = 3s) {
  for (auto until = std::chrono::steady_clock::now() + within;
       std::chrono::steady_clock::now() < until; std::this_thread::sleep_for(5ms))
    if (done())
      return true;
  return done();
}

// The port the loop's service `name` got
std::uint16_t portOf(const Network::Loop &loop, const std::string &name) {
  std::uint16_t port = 0;
  eventually([&] {
    for (const auto &state : loop.states())
      if (state.name == name && state.state == "listening")
        port = state.port;
    return port != 0;
  });
  return port;
}

// What a plain socket reads within `within`
std::string readFor(Network::Socket &socket, std::chrono::milliseconds within) {
  std::string got;
  char chunk[1024];
  for (auto until = std::chrono::steady_clock::now() + within;
       std::chrono::steady_clock::now() < until; std::this_thread::sleep_for(5ms))
    for (long n; (n = socket.receive(chunk)) > 0;)
      got.append(chunk, static_cast<std::size_t>(n));
  return got;
}

// A growing text, as a LogBuffer is
struct Text {
  std::mutex mutex;
  std::string text;
  std::string from(std::uint64_t &position) {
    std::lock_guard lock(mutex);
    std::string after = text.substr(position);
    position = text.size();
    return after;
  }
  void append(const std::string &more) {
    std::lock_guard lock(mutex);
    text += more;
  }
};

} // namespace

TEST(LoopTest, StreamsToEachReaderFromTheStartAndAsItGrows) {
  Text log;
  log.append("one\n");
  Network::StreamService stream("log", 0, [&](std::uint64_t &p) { return log.from(p); });
  Network::Loop loop;
  std::string error;
  ASSERT_TRUE(loop.start(error)) << error;
  loop.attach(&stream);
  const std::uint16_t port = portOf(loop, "log");
  ASSERT_NE(port, 0);

  Network::Socket first = Network::Socket::connectTcp("127.0.0.1", port, error);
  ASSERT_TRUE(first.valid()) << error;
  EXPECT_EQ(readFor(first, 300ms), "one\n");
  log.append("two\n");
  stream.wake();
  EXPECT_EQ(readFor(first, 300ms), "two\n");
  Network::Socket second = Network::Socket::connectTcp("127.0.0.1", port, error);
  EXPECT_EQ(readFor(second, 300ms), "one\ntwo\n"); // (from the start too)
  EXPECT_TRUE(eventually([&] { return loop.states().at(0).connections == 2; }));

  loop.detach(&stream); // its connections closed
  char chunk[16];
  EXPECT_TRUE(eventually([&] { return first.receive(chunk) == 0; }));
}

TEST(LoopTest, LinesBothWaysAndAConnectionWaitingForItsListener) {
  // A free port, for the server to listen on once the client is waiting
  std::string error;
  std::uint16_t port = 0;
  {
    Network::Socket probe = Network::Socket::listenTcp(0, error);
    ASSERT_TRUE(probe.valid()) << error;
    port = probe.port();
  }
  std::mutex mutex;
  std::vector<std::string> atServer, atClient;
  Network::LineService *serverPtr = nullptr;
  Network::LineService server("server", {.port = port},
                              {.onLine = [&](std::uint32_t from, const std::string &line) {
                                {
                                  std::lock_guard lock(mutex);
                                  atServer.push_back(line);
                                }
                                serverPtr->send("echo " + line, from);
                              }});
  serverPtr = &server;

  Network::Loop serverLoop;
  ASSERT_TRUE(serverLoop.start(error)) << error;

  Network::LineService client("client", {.host = "127.0.0.1", .port = port},
                              {.onLine =
                                   [&](std::uint32_t, const std::string &line) {
                                     std::lock_guard lock(mutex);
                                     atClient.push_back(line);
                                   },
                               .onConnected = [&](std::uint32_t) { /* (sent below) */ }});
  Network::Loop clientLoop;
  ASSERT_TRUE(clientLoop.start(error)) << error;
  clientLoop.attach(&client);
  EXPECT_TRUE(eventually([&] { return clientLoop.states().size() == 1; }));
  EXPECT_NE(clientLoop.states().at(0).state, "connected");

  serverLoop.attach(&server); // now listening: the client connects on its next try
  EXPECT_TRUE(eventually([&] { return clientLoop.states().at(0).state == "connected"; },
                         Network::Loop::kRetry + 2s));
  client.send("hello");
  client.send("two\\tparts"); // (text as it is)
  EXPECT_TRUE(eventually([&] {
    std::lock_guard lock(mutex);
    return atClient.size() == 2;
  }));
  std::lock_guard lock(mutex);
  EXPECT_EQ(atServer, (std::vector<std::string>{"hello", "two\\tparts"}));
  EXPECT_EQ(atClient, (std::vector<std::string>{"echo hello", "echo two\\tparts"}));
}
