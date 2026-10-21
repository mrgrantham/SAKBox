// WHttpParamParser, WHttpClient and WHttpServer.
#include <gtest/gtest.h>

#include <WClasses/WHttp.h>
#include <WClasses/WSocket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace WClasses;

TEST(HttpParamParserTest, ScrubKeepsOnlySafeCharacters) {
  std::string kept;
  for (char c = ' '; c <= '~'; c++) {
    if (c == '&' || c == '=' || c == '%' || c == '+')
      continue; // parameter syntax, not value characters
    const std::string params = std::string("v=x") + c + "x";
    WHttpParamParser pp(params.c_str());
    const char *v = pp.find("v");
    ASSERT_NE(v, nullptr) << "char " << c;
    ASSERT_EQ(strlen(v), 3u) << "char " << c;
    if (v[1] == c && c != '_') // ('_' is also what the others become)
      kept += c;
    else
      EXPECT_EQ(v[1], '_') << "char " << c;
  }
  EXPECT_EQ(kept, " !#$()*,-./0123456789:<>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                  "abcdefghijklmnopqrstuvwxyz");
}

namespace {
// (status() reports Aborted once the connection is gone, so read the
// outcome onLoseConnection records directly)
struct ClientInState : WHttpClient {
  ClientInState(bool chunked, size_t contentSize) {
    m_bChunked = chunked;
    m_nContentSize = contentSize;
    m_status = Downloading;
  }
  Status outcome() const { return m_status; }
};
WHttpClient::Status statusAfterLosingConnection(bool chunked, size_t contentSize) {
  ClientInState client(chunked, contentSize);
  client.onLoseConnection();
  return client.outcome();
}
} // namespace

TEST(HttpClientTest, LosingTheConnectionMidBodyIsAnError) {
  // The body's length was known (chunked, or a Content-Length): cut short
  EXPECT_EQ(statusAfterLosingConnection(true, 0), WHttpClient::Error);
  EXPECT_EQ(statusAfterLosingConnection(false, 100), WHttpClient::Error);
  // No length given: the server ends the body by closing the connection
  EXPECT_EQ(statusAfterLosingConnection(false, 0), WHttpClient::Done);
}

// A client's Content-Length was read with atoi into a size_t: "-1" became
// the largest size, so the +1 for the terminator wrapped to a zero-byte
// buffer that the body was then written into.
namespace {
struct ClientReadingHeaders : WHttpClient {
  void header(const char *line) { processHeader((const unsigned char *)line, strlen(line)); }
  Status outcome() const { return m_status; }
  size_t contentSize() const { return m_nContentSize; }
  bool hasBuffer() const { return m_pData != nullptr; }
  ClientReadingHeaders() { // the state sendGetRequest sets up
    m_nContentSize = 0;
    m_nDataPos = 0;
    m_bChunked = false;
    m_bPastHeader = false;
    m_nHeaderPos = 0;
    m_bAmCurrentlyDoingJustHeaders = false;
    m_status = Downloading;
  }
};
} // namespace

TEST(HttpClientTest, BadContentLengthIsAnError) {
  for (const char *value : {"-1", "abc", "99999999999999999999", "12x"}) {
    SCOPED_TRACE(value);
    ClientReadingHeaders client;
    client.header("HTTP/1.1 200 OK\n");
    client.header((std::string("Content-Length: ") + value + "\n").c_str());
    EXPECT_EQ(client.outcome(), WHttpClient::Error);
    EXPECT_FALSE(client.hasBuffer());
  }
  ClientReadingHeaders ok;
  ok.header("HTTP/1.1 200 OK\n");
  ok.header("Content-Length: 1234\n");
  EXPECT_EQ(ok.outcome(), WHttpClient::Downloading);
  EXPECT_EQ(ok.contentSize(), 1234u);
  EXPECT_TRUE(ok.hasBuffer());
}

TEST(HttpContentLengthTest, OnlyAWholeNumberWithinTheLimit) {
  size_t n = 0;
  EXPECT_TRUE(parseHttpContentLength("0", &n));
  EXPECT_EQ(n, 0u);
  EXPECT_TRUE(parseHttpContentLength(" 42\r", &n)); // spaces and the line end
  EXPECT_EQ(n, 42u);
  EXPECT_TRUE(parseHttpContentLength(std::to_string(MAX_HTTP_CONTENT_LENGTH).c_str(), &n));
  for (const char *bad : {"", "-1", "+5", "abc", "12x", "1.5", "1 2", "99999999999999999999"})
    EXPECT_FALSE(parseHttpContentLength(bad, &n)) << "\"" << bad << "\"";
  EXPECT_FALSE(parseHttpContentLength(std::to_string(MAX_HTTP_CONTENT_LENGTH + 1).c_str(), &n));
}

// A server answering every GET with "got <url>" and every POST with
// "posted <body>"
namespace {
struct EchoConnection : WHttpConnection {
  explicit EchoConnection(SOCKET s) : WHttpConnection(s) {}
  void doGet(std::ostream &response) override {
    response << "got " << m_szUrl;
    if (strcmp(m_szUrl, "/big") == 0)
      response << std::string(1 << 20, 'x'); // more than one send can take
  }
  void doPost(std::ostream &response) override {
    response << "posted " << std::string(m_pContent, m_nContentLength);
  }
  bool hasBeenModifiedSince(const char *, const char *) override { return true; }
  void setHeaders(const char *, const char *) override {}
};
struct EchoServer : WHttpServer {
  explicit EchoServer(bool networkAccessible = false)
      : WHttpServer(0, networkAccessible) {} // port 0: any free port
  WHttpConnection *makeConnection(SOCKET s) override { return new EchoConnection(s); }
};

// Sends a request to the server and returns what comes back: its reply, or
// "(closed)" if it hung up without one
std::string send(EchoServer &server, const std::string &request) {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server.socket()->port());
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0) {
    close(sock);
    return "(couldn't connect)";
  }
  ::send(sock, request.data(), request.size(), 0);
  std::string reply;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    server.process();
    char buf[4096];
    ssize_t n = recv(sock, buf, sizeof(buf), MSG_DONTWAIT);
    if (n > 0) {
      reply.append(buf, n);
      continue;
    }
    const bool closed = n == 0;
    const bool replied = reply.find("got ") != std::string::npos ||
                         reply.find("posted ") != std::string::npos; // (the body comes last)
    if (closed || replied)
      break;
    usleep(1000);
  }
  close(sock);
  return reply.empty() ? "(closed)" : reply;
}
} // namespace

// The wizard's and plot's servers accepted connections from the whole
// network (INADDR_ANY), so anyone on the same Wi-Fi could reach them. Now
// only this computer can, unless network access is asked for.
TEST(HttpServerTest, ListensOnThisComputerOnlyUnlessAsked) {
  EchoServer local;
  EXPECT_EQ(local.socket()->boundAddress(), "127.0.0.1");
  EXPECT_FALSE(local.socket()->networkAccessible());
  EXPECT_GT(local.socket()->port(), 0);
  EchoServer network(true);
  EXPECT_EQ(network.socket()->boundAddress(), "0.0.0.0");
  EXPECT_TRUE(network.socket()->networkAccessible());
}

TEST(HttpServerTest, AnswersGoodRequests) {
  EchoServer server;
  const std::string reply = send(server, "GET /hello HTTP/1.1\r\n\r\n");
  EXPECT_NE(reply.find("got /hello"), std::string::npos);
  // An RFC 1123 date, as WTime::httpDate makes
  EXPECT_TRUE(std::regex_search(
      reply, std::regex("\r\nDate: (Mon|Tue|Wed|Thu|Fri|Sat|Sun), [0-9]{2} "
                        "[A-Z][a-z]{2} [0-9]{4} [0-9]{2}:[0-9]{2}:[0-9]{2} GMT\r\n")))
      << reply;
  EXPECT_NE(
      send(server, "POST /form HTTP/1.1\r\nContent-Length: 5\r\n\r\nabcde").find("posted abcde"),
      std::string::npos);
}

// Header values were copied into fixed buffers without checking their
// length, and Content-Length was read with atoi into a size_t ("-1" gave a
// zero-byte buffer the body was written into). Each request below
// overflowed a buffer, or would have; now the server hangs up on it.
TEST(HttpServerTest, HangsUpOnRequestsThatDontFit) {
  const std::string longUrl(3000, 'a');                         // URL buffer: 2048
  const std::string longParams = "x=" + std::string(3000, 'b'); // params: 2048
  const std::string longCookie(5000, 'c');                      // cookie buffer: 4096
  const std::string longDate(500, 'd');                         // date buffer: 128
  const std::string requests[] = {
      "POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n" + std::string(100, 'x'),
      "POST / HTTP/1.1\r\nContent-Length: 99999999999\r\n\r\nx",
      "POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\nx",
      "GET /" + longUrl + " HTTP/1.1\r\n\r\n",
      "GET /page?" + longParams + " HTTP/1.1\r\n\r\n",
      "GET / HTTP/1.1\r\nCookie: " + longCookie + "\r\n\r\n",
      "GET / HTTP/1.1\r\nIf-Modified-Since: " + longDate + "\r\n\r\n",
  };
  EchoServer server;
  for (const std::string &request : requests) {
    SCOPED_TRACE(request.substr(0, 60));
    EXPECT_EQ(send(server, request), "(closed)");
  }
  // And it still serves the next good one
  EXPECT_NE(send(server, "GET /ok HTTP/1.1\r\n\r\n").find("got /ok"), std::string::npos);
}

// A client that hangs up before its reply is sent: writing to its socket
// raised SIGPIPE, which ends the process, so one client could stop the
// server. Now the send fails with an error, the server drops that
// connection, says why (the reason used to be lost: the error was rethrown
// as a plain std::exception) and carries on.
TEST(HttpServerTest, SurvivesAClientThatHangsUp) {
  EchoServer server;
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server.socket()->port());
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  ASSERT_EQ(connect(sock, (sockaddr *)&addr, sizeof(addr)), 0);
  const std::string request = "GET /big HTTP/1.1\r\n\r\n";
  ::send(sock, request.data(), request.size(), 0);
  usleep(50000);
  linger reset{1, 0}; // close with a reset, at once
  setsockopt(sock, SOL_SOCKET, SO_LINGER, &reset, sizeof(reset));
  close(sock);
  usleep(50000);

  std::ostringstream log;
  std::streambuf *old = std::cout.rdbuf(log.rdbuf());
  for (int i = 0; i < 20; i++) {
    server.process();
    usleep(5000);
  }
  std::cout.rdbuf(old);
  EXPECT_NE(log.str().find("Failed to send GET response: Error sending"), std::string::npos)
      << log.str();
  EXPECT_NE(send(server, "GET /ok HTTP/1.1\r\n\r\n").find("got /ok"), std::string::npos);
}

// serveRequests is the loop every Waffles server ran by hand: handle
// requests until told to stop, calling onIdle between them.
namespace {
struct IdleEchoServer : EchoServer {
  std::atomic<int> idleCalls{0};
  std::atomic<double> lastIdle{-1};
  std::vector<double> idleTimes; // (read after serveRequests returns)
  std::function<void(IdleEchoServer &)> whenIdle = [](IdleEchoServer &) {};
  void onIdle(double secondsSinceLastRequest) override {
    idleCalls++;
    lastIdle = secondsSinceLastRequest;
    idleTimes.push_back(secondsSinceLastRequest);
    whenIdle(*this);
  }
};

// Sends a request to a server that's serving on another thread
std::string fetch(WHttpServer &server, const std::string &url) {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  timeval timeout{5, 0};
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server.socket()->port());
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(sock, (sockaddr *)&addr, sizeof(addr)) != 0) {
    close(sock);
    return "(couldn't connect)";
  }
  const std::string request = "GET " + url + " HTTP/1.1\r\n\r\n";
  ::send(sock, request.data(), request.size(), 0);
  std::string reply;
  char buf[4096];
  while (reply.find("got ") == std::string::npos) {
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
    if (n <= 0)
      break;
    reply.append(buf, n);
  }
  close(sock);
  return reply;
}

void waitFor(const std::function<bool()> &condition) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!condition() && std::chrono::steady_clock::now() < deadline)
    usleep(1000);
}
} // namespace

TEST(HttpServeRequestsTest, AnswersUntilStopServing) {
  IdleEchoServer server;
  std::thread serving([&] { server.serveRequests(); });
  EXPECT_NE(fetch(server, "/one").find("got /one"), std::string::npos);
  EXPECT_NE(fetch(server, "/two").find("got /two"), std::string::npos);
  const auto asked = std::chrono::steady_clock::now();
  server.stopServing();
  serving.join();
  EXPECT_LT(std::chrono::steady_clock::now() - asked, std::chrono::seconds(1));
}

// onIdle is told how long it's been since the last request, so a server can
// shut down when no one is using it (as plot and the wizard do).
TEST(HttpServeRequestsTest, OnIdleCountsFromTheLastRequest) {
  IdleEchoServer server;
  std::thread serving([&] { server.serveRequests(); });
  waitFor([&] { return server.lastIdle > 0.25; });
  EXPECT_GT(server.lastIdle, 0.25);
  EXPECT_NE(fetch(server, "/hello").find("got /hello"), std::string::npos);
  const int callsBefore = server.idleCalls;
  waitFor([&] { return server.idleCalls > callsBefore + 1; });
  EXPECT_LT(server.lastIdle, 0.25); // counting again from the request
  server.stopServing();
  serving.join();
}

// A server that stops itself from onIdle
TEST(HttpServeRequestsTest, OnIdleCanStopServing) {
  IdleEchoServer server;
  server.whenIdle = [](IdleEchoServer &s) {
    if (s.idleCalls == 3)
      s.stopServing();
  };
  server.serveRequests();
  EXPECT_EQ(server.idleCalls, 3);
  EXPECT_LT(server.idleTimes[0], server.idleTimes[2]); // (it sleeps between calls)
}

// Ctrl-C or kill stop it; SIGPIPE, which means a client hung up, doesn't.
TEST(HttpServeRequestsTest, StopsOnSigtermButNotSigpipe) {
  IdleEchoServer server;
  server.whenIdle = [](IdleEchoServer &s) {
    if (s.idleCalls == 2)
      raise(SIGPIPE);
    if (s.idleCalls == 4)
      raise(SIGTERM);
    if (s.idleCalls == 10)
      s.stopServing(); // (in case the signal doesn't stop it)
  };
  server.serveRequests();
  EXPECT_EQ(server.idleCalls, 4);
}

// A URL's port was read with atoi: "abc" gave port 0, and 70000 was cut to
// 16 bits, connecting to port 4464.
TEST(HttpUrlTest, PortMustBeValid) {
  EXPECT_EQ(WHttp_urlPort("http://example.com/page"), 80);
  EXPECT_EQ(WHttp_urlPort("http://example.com:8080/page"), 8080);
  EXPECT_EQ(WHttp_urlPort("http://example.com:65535/"), 65535);
  for (const char *bad :
       {"http://example.com:abc/", "http://example.com:70000/", "http://example.com:0/",
        "http://example.com:/", "http://example.com:80x/"})
    EXPECT_THROW(WHttp_urlPort(bad), std::exception) << bad;
}

// unescapeUrl turned any letter after '%' into a "digit" up to 35 and a
// non-letter into 2 or 0, so "%G0" decoded to '\0' (cutting the value short)
// and "%zz" to a made-up byte. Now only %XY with two hex digits is decoded;
// any other '%' is kept as it is.
namespace {
std::string unescaped(const std::string &s) {
  std::string out(s.size() + 1, '?');
  WHttpServer::unescapeUrl(&out[0], s.c_str(), s.size());
  return out.c_str();
}
} // namespace

TEST(HttpUnescapeUrlTest, DecodesOnlyValidEscapes) {
  EXPECT_EQ(unescaped("a%20b"), "a b");
  EXPECT_EQ(unescaped("%41%62%7e"), "Ab~");
  EXPECT_EQ(unescaped("a+b"), "a b");
  EXPECT_EQ(unescaped("caf%C3%A9"), "caf\xC3\xA9");
  const char *kept[][2] = {
      {"100%", "100%"}, {"%4", "%4"},   {"%zz", "%zz"},
      {"%G0x", "%G0x"}, {"%%41", "%A"}, {"50%+", "50% "},
  };
  for (auto &k : kept)
    EXPECT_EQ(unescaped(k[0]), k[1]) << k[0];
  // Only the first nInLen characters are read
  std::string out(8, '?');
  WHttpServer::unescapeUrl(&out[0], "ab%41cd", 2);
  EXPECT_STREQ(out.c_str(), "ab");
  WHttpServer::unescapeUrl(&out[0], "ab%41cd", 4);
  EXPECT_STREQ(out.c_str(), "ab%4");
}
