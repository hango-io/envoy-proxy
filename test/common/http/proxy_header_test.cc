#include "source/common/http/header_map_impl.h"
#include "source/common/http/headers.h"
#include "source/common/http/proxy_header.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

TEST(HeaderUtilityTest, ReplaceHeaderTest) {
  auto headers1 = Envoy::Http::createHeaderMap<Envoy::Http::RequestHeaderMapImpl>(
      {{Envoy::Http::LowerCaseString("key1"), "old_value1"},
       {Envoy::Http::LowerCaseString("key2"), "old_value2"},
       {Envoy::Http::LowerCaseString("key3"), "old_value3"}});

  auto headers2 = Envoy::Http::createHeaderMap<Envoy::Http::RequestHeaderMapImpl>(
      {{Envoy::Http::LowerCaseString("key2"), "new_value2"},
       {Envoy::Http::LowerCaseString("key3"), "new_value3"},
       {Envoy::Http::LowerCaseString("key4"), "new_value4"}});

  HeaderUtility::replaceHeaders(*headers1, *headers2);

  EXPECT_TRUE(headers1->get(Envoy::Http::LowerCaseString("key1")).empty());
  EXPECT_EQ("new_value2",
            headers1->get(Envoy::Http::LowerCaseString("key2"))[0]->value().getStringView());
  EXPECT_EQ("new_value3",
            headers1->get(Envoy::Http::LowerCaseString("key3"))[0]->value().getStringView());
}

TEST(HeaderUtilityTest, getHeaderValueTest) {
  auto headers1 = Envoy::Http::createHeaderMap<Envoy::Http::RequestHeaderMapImpl>(
      {{Envoy::Http::LowerCaseString("key1"), "old_value1"},
       {Envoy::Http::LowerCaseString("key2"), "old_value2"},
       {Envoy::Http::LowerCaseString("key3"), "old_value3"}});

  EXPECT_EQ(
      HeaderUtility::getHeaderValue(Envoy::Http::LowerCaseString("key1"), *headers1, "xxxxxx"),
      "old_value1");

  EXPECT_EQ(
      HeaderUtility::getHeaderValue(Envoy::Http::LowerCaseString("key4"), *headers1, "xxxxxx"),
      "xxxxxx");
}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
