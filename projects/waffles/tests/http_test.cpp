// GHttpParamParser, GHttpClient and GHttpServer.
#include <gtest/gtest.h>

#include <WClasses/GHttp.h>
#include <WClasses/GSocket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <string>

using namespace GClasses;

TEST(HttpParamParserTest, ScrubKeepsOnlySafeCharacters) {
  std::string kept;
  for (char c = ' '; c <= '~'; c++) {
    if (c == '&' || c == '=' || c == '%' || c == '+')
      continue; // parameter syntax, not value characters
    const std::string params = std::string("v=x") + c + "x";
    GHttpParamParser pp(params.c_str());
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
struct ClientInState : GHttpClient {
  ClientInState(bool chunked, size_t contentSize) {
    m_bChunked = chunked;
    m_nContentSize = contentSize;
    m_status = Downloading;
  }
  Status outcome() const { return m_status; }
};
GHttpClient::Status statusAfterLosingConnection(bool chunked,
                                                size_t contentSize) {
  ClientInState client(chunked, contentSize);
  client.onLoseConnection();
  return client.outcome();
}
} // namespace

TEST(HttpClientTest, LosingTheConnectionMidBodyIsAnError) {
  // The body's length was known (chunked, or a Content-Length): cut short
  EXPECT_EQ(statusAfterLosingConnection(true, 0), GHttpClient::Error);
  EXPECT_EQ(statusAfterLosingConnection(false, 100), GHttpClient::Error);
  // No length given: the server ends the body by closing the connection
  EXPECT_EQ(statusAfterLosingConnection(false, 0), GHttpClient::Done);
}

// A client's Content-Length was read with atoi into a size_t: "-1" became
// the largest size, so the +1 for the terminator wrapped to a zero-byte
// buffer that the body was then written into.
namespace {
struct ClientReadingHeaders : GHttpClient {
  void header(const char *line) {
    processHeader((const unsigned char *)line, strlen(line));
  }
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
    EXPECT_EQ(client.outcome(), GHttpClient::Error);
    EXPECT_FALSE(client.hasBuffer());
  }
  ClientReadingHeaders ok;
  ok.header("HTTP/1.1 200 OK\n");
  ok.header("Content-Length: 1234\n");
  EXPECT_EQ(ok.outcome(), GHttpClient::Downloading);
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
  for (const char *bad : {"", "-1", "+5", "abc", "12x", "1.5", "1 2",
                          "99999999999999999999"})
    EXPECT_FALSE(parseHttpContentLength(bad, &n)) << "\"" << bad << "\"";
  EXPECT_FALSE(parseHttpContentLength(
      std::to_string(MAX_HTTP_CONTENT_LENGTH + 1).c_str(), &n));
}

// A server answering every GET with "got <url>" and every POST with
// "posted <body>"
namespace {
struct EchoConnection : GHttpConnection {
  explicit EchoConnection(SOCKET s) : GHttpConnection(s) {}
  void doGet(std::ostream &response) override { response << "got " << m_szUrl; }
  void doPost(std::ostream &response) override {
    response << "posted " << std::string(m_pContent, m_nContentLength);
  }
  bool hasBeenModifiedSince(const char *, const char *) override { return true; }
  void setHeaders(const char *, const char *) override {}
};
struct EchoServer : GHttpServer {
  explicit EchoServer(bool networkAccessible = false)
      : GHttpServer(0, networkAccessible) {} // port 0: any free port
  GHttpConnection *makeConnection(SOCKET s) override { return new EchoConnection(s); }
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
  EXPECT_NE(send(server, "GET /hello HTTP/1.1\r\n\r\n").find("got /hello"),
            std::string::npos);
  EXPECT_NE(send(server, "POST /form HTTP/1.1\r\nContent-Length: 5\r\n\r\nabcde")
                .find("posted abcde"),
            std::string::npos);
}

// Header values were copied into fixed buffers without checking their
// length, and Content-Length was read with atoi into a size_t ("-1" gave a
// zero-byte buffer the body was written into). Each request below
// overflowed a buffer, or would have; now the server hangs up on it.
TEST(HttpServerTest, HangsUpOnRequestsThatDontFit) {
  const std::string longUrl(3000, 'a');                 // URL buffer: 2048
  const std::string longParams = "x=" + std::string(3000, 'b'); // params: 2048
  const std::string longCookie(5000, 'c');              // cookie buffer: 4096
  const std::string longDate(500, 'd');                 // date buffer: 128
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
  EXPECT_NE(send(server, "GET /ok HTTP/1.1\r\n\r\n").find("got /ok"),
            std::string::npos);
}
