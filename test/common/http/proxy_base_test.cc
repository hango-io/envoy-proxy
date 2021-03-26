#include "common/http/proxy_base.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

TEST(HttpUriTest, HttpUriTest) {
  auto uri1 = HttpUri::makeHttpUri("tcp://localhost/error");
  EXPECT_EQ(nullptr, uri1);

  auto uri2 = HttpUri::makeHttpUri("http://");
  EXPECT_EQ(nullptr, uri2);

  auto uri3 = HttpUri::makeHttpUri("");
  EXPECT_EQ(nullptr, uri3);

  auto uri4 = HttpUri::makeHttpUri("http://localhost/hello?comefrom=test");
  EXPECT_NE(nullptr, uri4);
  EXPECT_EQ("http", uri4->scheme);
  EXPECT_EQ("localhost", uri4->host);
  EXPECT_EQ("/hello?comefrom=test", uri4->path);

  auto uri5 = HttpUri::makeHttpUri("abcdefg");
  EXPECT_EQ(nullptr, uri5);

  auto uri6 = HttpUri::makeHttpUri("https://localhost/hello?comefrom=test");
  EXPECT_NE(nullptr, uri6);
  EXPECT_EQ("https", uri6->scheme);
  EXPECT_EQ("localhost", uri6->host);
  EXPECT_EQ("/hello?comefrom=test", uri6->path);
}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
