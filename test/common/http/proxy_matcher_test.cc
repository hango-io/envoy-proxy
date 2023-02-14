#include "source/common/http/proxy_matcher.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {
namespace {

TEST(CommonMatcherTest, HeadersTest) {
  {
    constexpr absl::string_view config_string = R"EOF(
    headers:
    - name: fake_header
      exact_match: fake_header_value
    - name: fake_header2
      suffix_match: efg
    )EOF";

    CommonMatcherProto common_matcher_proto;
    TestUtility::loadFromYaml(std::string(config_string), common_matcher_proto);
    CommonMatcher matcher(common_matcher_proto);

    Envoy::Http::TestRequestHeaderMapImpl headers = {
        {"fake_header", "fake_header_value"},
        {":method", "GET"},
        {":path", "/path?fake_parameter=fake_parameter_value"}};
    HttpCommonMatcherContext context(headers);
    EXPECT_FALSE(matcher.match(context));

    headers.setCopy(Envoy::Http::LowerCaseString("fake_header2"), "abcdefg");
    HttpCommonMatcherContext context2(headers);
    EXPECT_TRUE(matcher.match(context2));
  }
}

TEST(CommonMatcherTest, QueryParametersTest) {
  {
    constexpr absl::string_view config_string = R"EOF(
    parameters:
    - name: fake_parameter
      string_match:
        exact: fake_parameter_value
    - name: fake_parameter2
      present_match: true
    )EOF";

    CommonMatcherProto common_matcher_proto;
    TestUtility::loadFromYaml(std::string(config_string), common_matcher_proto);
    CommonMatcher matcher(common_matcher_proto);

    Envoy::Http::TestRequestHeaderMapImpl headers = {
        {"fake_header", "fake_header_value"},
        {":method", "GET"},
        {":path", "/path?fake_parameter=fake_parameter_value"}};
    HttpCommonMatcherContext context(headers);
    EXPECT_FALSE(matcher.match(context));

    headers.setCopy(Envoy::Http::LowerCaseString(":path"),
                    "/path?fake_parameter=fake_parameter_value&fake_parameter2=xxxx");
    HttpCommonMatcherContext context2(headers);
    EXPECT_TRUE(matcher.match(context2));
  }
}

TEST(CommonMatcherTest, CookiesTest) {
  {
    constexpr absl::string_view config_string = R"EOF(
    cookies:
    - name: fake_cookie
      string_match:
        exact: fake_cookie_value
    - name: fake_cookie2
      present_match: true
    )EOF";

    CommonMatcherProto common_matcher_proto;
    TestUtility::loadFromYaml(std::string(config_string), common_matcher_proto);
    CommonMatcher matcher(common_matcher_proto);

    Envoy::Http::TestRequestHeaderMapImpl headers = {{"fake_header", "fake_header_value"},
                                                     {"cookie", "fake_cookie=fake_cookie_value"}};
    HttpCommonMatcherContext context(headers);
    EXPECT_FALSE(matcher.match(context));

    headers.setCopy(Envoy::Http::LowerCaseString("cookie"),
                    "fake_cookie=fake_cookie_value; fake_cookie2=xxxx");
    HttpCommonMatcherContext context2(headers);
    EXPECT_TRUE(matcher.match(context2));
  }
}

TEST(CommonMatcherTest, MutipleTest) {
  {
    constexpr absl::string_view config_string = R"EOF(
    headers:
    - name: fake_header
      exact_match: fake_header_value
    - name: fake_header2
      suffix_match: efg
    parameters:
    - name: fake_parameter
      string_match:
        exact: fake_parameter_value
    - name: fake_parameter2
      present_match: true
    cookies:
    - name: fake_cookie
      string_match:
        exact: fake_cookie_value
    - name: fake_cookie2
      present_match: true
    )EOF";

    CommonMatcherProto common_matcher_proto;
    TestUtility::loadFromYaml(std::string(config_string), common_matcher_proto);
    CommonMatcher matcher(common_matcher_proto);

    Envoy::Http::TestRequestHeaderMapImpl headers = {
        {"fake_header", "fake_header_value"},
        {"cookie", "fake_cookie=fake_cookie_value"},
        {":path", "/path?fake_parameter=fake_parameter_value"}};
    HttpCommonMatcherContext context(headers);
    EXPECT_FALSE(matcher.match(context));

    headers.setCopy(Envoy::Http::LowerCaseString("fake_header2"), "abcdefg");
    HttpCommonMatcherContext context2(headers);
    EXPECT_FALSE(matcher.match(context2));

    headers.setCopy(Envoy::Http::LowerCaseString(":path"),
                    "/path?fake_parameter=fake_parameter_value&fake_parameter2=xxxx");
    HttpCommonMatcherContext context3(headers);
    EXPECT_FALSE(matcher.match(context3));

    headers.setCopy(Envoy::Http::LowerCaseString("cookie"),
                    "fake_cookie=fake_cookie_value; fake_cookie2=xxxx");
    HttpCommonMatcherContext context4(headers);
    EXPECT_TRUE(matcher.match(context4));
  }
}

} // namespace
} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
