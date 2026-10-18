// GHttpParamParser and GHttpClient.
#include <gtest/gtest.h>

#include <WClasses/GHttp.h>

#include <cstddef>
#include <cstring>
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
