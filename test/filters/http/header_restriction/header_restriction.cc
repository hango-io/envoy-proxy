#include "source/filters/http/header_restriction/header_restriction.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRestriction {

class HeaderRestrictionFilterTest : public testing::Test {
public:
  void SetUpTest(const std::string& yaml) {

    proxy::filters::http::header_restriction::v2::ProtoCommonConfig proto_config{};
    TestUtility::loadFromYaml(yaml, proto_config);

    config_.reset(new CommonConfig(proto_config));

    std::string filter_name = "proxy.filters.http.header_restriction";

    filter_ = std::make_unique<Filter>(filter_name, config_.get());
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  std::unique_ptr<Filter> filter_;
  std::shared_ptr<CommonConfig> config_;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;

  const std::string white_config_ = R"EOF(
  config:
    type: WHITE
    list:
      - headers:
          - name: user-agent
            exact_match: user-agent
      - headers:
          - name: test-header-1
            prefix_match: test-header-1
      - headers:
          - name: test-header-2
            suffix_match: test-header-2
      - headers:
          - name: test-header-3
            safe_regex_match:
              google_re2: {}
              regex: .*test-header-3.*
      - headers:
          - name: test-header-4
            exact_match: test-header-4
        path:
            exact: /test
      - path:
          exact: /anything
          ignore_case: true
      - path:
          prefix: /test-path-1
      - path:
          suffix: test-path-2
      - path:
          safe_regex:
            google_re2: {}
            regex: .*test-path-3.*
      - path:
          contains: test-path-4
      - parameters:
          name: name
          string_match: 
            exact: test-parameters-1
      - parameters:
          name: age
          present_match: true
      - cookies:
          name: name
          string_match:
            prefix: netease
      - cookies:
          name: age
          present_match: true
  )EOF";

  const std::string black_config_ = R"EOF(
  config:
    list:
      - headers:
          - name: user-agent
            exact_match: user-agent
      - headers:
          - name: test-header-1
            prefix_match: test-header-1
      - headers:
          - name: test-header-2
            suffix_match: test-header-2
      - headers:
          - name: test-header-3
            safe_regex_match:
              google_re2: {}
              regex: .*test-header-3.*
      - headers:
          - name: test-header-4
            exact_match: test-header-4
        path:
            exact: /test
      - path:
          exact: /anything
          ignore_case: true
      - path:
          prefix: /test-path-1
      - path:
          suffix: test-path-2
      - path:
          safe_regex:
            google_re2: {}
            regex: .*test-path-3.*
      - path:
          contains: test-path-4
      - parameters:
          name: name
          string_match: 
            exact: test-parameters-1
      - parameters:
          name: age
          present_match: true
      - cookies:
          name: name
          string_match:
            prefix: netease
      - cookies:
          name: age
          present_match: true
  )EOF";
};

TEST_F(HeaderRestrictionFilterTest, BlackList) {
  SetUpTest(black_config_);

  Http::TestRequestHeaderMapImpl request_headers_limit{{"content-type", "test"},
                                                       {":method", "GET"},
                                                       {":authority", "www.proxy.com"},
                                                       {":path", "/path"},
                                                       {"user-agent", "user-agent"}};
  auto status = filter_->decodeHeaders(request_headers_limit, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status);

  Http::TestRequestHeaderMapImpl request_headers_no_limit{{"content-type", "test"},
                                                          {":method", "GET"},
                                                          {":authority", "www.proxy.com"},
                                                          {":path", "/path"}};
  auto status_no_limit = filter_->decodeHeaders(request_headers_no_limit, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_no_limit);

  Http::TestRequestHeaderMapImpl request_path_limit_1{{"content-type", "test"},
                                                      {":method", "GET"},
                                                      {":authority", "www.proxy.com"},
                                                      {":path", "/test"},
                                                      {"test-header-4", "test-header-4"}};
  auto status_request_path_limit_1 = filter_->decodeHeaders(request_path_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_path_limit_1);

  Http::TestRequestHeaderMapImpl request_path_limit_2{{"content-type", "test"},
                                                      {":method", "GET"},
                                                      {":authority", "www.proxy.com"},
                                                      {":path", "/AnYthINg"}};
  auto status_request_path_limit_2 = filter_->decodeHeaders(request_path_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_path_limit_2);

  Http::TestRequestHeaderMapImpl request_path_no_limit_3{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/path"}};
  auto status_request_path_no_limit_3 = filter_->decodeHeaders(request_path_no_limit_3, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_path_no_limit_3);

  Http::TestRequestHeaderMapImpl request_path_limit_4{{"content-type", "test"},
                                                      {":method", "GET"},
                                                      {":authority", "www.proxy.com"},
                                                      {":path", "/test-path-1-netease"}};
  auto status_request_path_limit_4 = filter_->decodeHeaders(request_path_limit_4, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_path_limit_4);

  Http::TestRequestHeaderMapImpl request_parameters_limit_1{
      {"content-type", "test"},
      {":method", "GET"},
      {":authority", "www.proxy.com"},
      {":path", "/path?name=test-parameters-1"}};
  auto status_request_parameters_limit_1 = filter_->decodeHeaders(request_parameters_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_parameters_limit_1);

  Http::TestRequestHeaderMapImpl request_parameters_limit_2{{"content-type", "test"},
                                                            {":method", "GET"},
                                                            {":authority", "www.proxy.com"},
                                                            {":path", "/path?age=18"}};
  auto status_request_parameters_limit_2 = filter_->decodeHeaders(request_parameters_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_parameters_limit_2);

  Http::TestRequestHeaderMapImpl request_cookies_limit_1{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/path"},
                                                         {"cookie", "name=netease123"}};
  auto status_request_cookies_limit_1 = filter_->decodeHeaders(request_cookies_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_cookies_limit_1);

  Http::TestRequestHeaderMapImpl request_cookies_limit_2{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/path"},
                                                         {"cookie", "age=18"}};
  auto status_request_cookies_limit_2 = filter_->decodeHeaders(request_cookies_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_cookies_limit_2);

  Http::TestRequestHeaderMapImpl request_cookies_no_limit_3{{"content-type", "test"},
                                                            {":method", "GET"},
                                                            {":authority", "www.proxy.com"},
                                                            {":path", "/path"},
                                                            {"cookie", "name=123NETease"}};
  auto status_request_cookies_no_limit_3 = filter_->decodeHeaders(request_cookies_no_limit_3, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_cookies_no_limit_3);
}

TEST_F(HeaderRestrictionFilterTest, WhiteList) {
  SetUpTest(white_config_);

  Http::TestRequestHeaderMapImpl request_headers_no_limit{{"content-type", "test"},
                                                          {":method", "GET"},
                                                          {":authority", "www.proxy.com"},
                                                          {":path", "/path"},
                                                          {"test-header-1", "test-header-1-2"}};
  auto status = filter_->decodeHeaders(request_headers_no_limit, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);

  Http::TestRequestHeaderMapImpl request_headers_limit{{"content-type", "test"},
                                                       {":method", "GET"},
                                                       {":authority", "www.proxy.com"},
                                                       {":path", "/path"},
                                                       {"test-header-1", "test-header-2-1"}};
  auto status_limit = filter_->decodeHeaders(request_headers_limit, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_limit);

  Http::TestRequestHeaderMapImpl request_path_no_limit_1{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/test"},
                                                         {"test-header-4", "test-header-4"}};
  auto status_request_path_no_limit_1 = filter_->decodeHeaders(request_path_no_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_path_no_limit_1);

  Http::TestRequestHeaderMapImpl request_path_no_limit_2{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/AnYthINg"}};
  auto status_request_path_no_limit_2 = filter_->decodeHeaders(request_path_no_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_path_no_limit_2);

  Http::TestRequestHeaderMapImpl request_path_limit_3{{"content-type", "test"},
                                                      {":method", "GET"},
                                                      {":authority", "www.proxy.com"},
                                                      {":path", "/path"}};
  auto status_request_path_limit_3 = filter_->decodeHeaders(request_path_limit_3, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_path_limit_3);

  Http::TestRequestHeaderMapImpl request_path_no_limit_4{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/test-path-1-netease"}};
  auto status_request_path_no_limit_4 = filter_->decodeHeaders(request_path_no_limit_4, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_path_no_limit_4);

  Http::TestRequestHeaderMapImpl request_parameters_no_limit_1{
      {"content-type", "test"},
      {":method", "GET"},
      {":authority", "www.proxy.com"},
      {":path", "/path?name=test-parameters-1"}};
  auto status_request_parameters_no_limit_1 =
      filter_->decodeHeaders(request_parameters_no_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_parameters_no_limit_1);

  Http::TestRequestHeaderMapImpl request_parameters_no_limit_2{{"content-type", "test"},
                                                               {":method", "GET"},
                                                               {":authority", "www.proxy.com"},
                                                               {":path", "/path?age=18"}};
  auto status_request_parameters_no_limit_2 =
      filter_->decodeHeaders(request_parameters_no_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_parameters_no_limit_2);

  Http::TestRequestHeaderMapImpl request_cookies_no_limit_1{{"content-type", "test"},
                                                            {":method", "GET"},
                                                            {":authority", "www.proxy.com"},
                                                            {":path", "/path"},
                                                            {"cookie", "name=netease123"}};
  auto status_request_cookies_no_limit_1 = filter_->decodeHeaders(request_cookies_no_limit_1, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_cookies_no_limit_1);

  Http::TestRequestHeaderMapImpl request_cookies_no_limit_2{{"content-type", "test"},
                                                            {":method", "GET"},
                                                            {":authority", "www.proxy.com"},
                                                            {":path", "/path"},
                                                            {"cookie", "age=18"}};
  auto status_request_cookies_no_limit_2 = filter_->decodeHeaders(request_cookies_no_limit_2, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status_request_cookies_no_limit_2);

  Http::TestRequestHeaderMapImpl request_cookies_limit_3{{"content-type", "test"},
                                                         {":method", "GET"},
                                                         {":authority", "www.proxy.com"},
                                                         {":path", "/path"},
                                                         {"cookie", "name=123NETease"}};
  auto status_request_cookies_limit_3 = filter_->decodeHeaders(request_cookies_limit_3, true);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration, status_request_cookies_limit_3);
}

} // namespace HeaderRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
