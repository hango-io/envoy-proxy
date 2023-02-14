#include "source/filters/http/local_limit/local_limit_filter.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {
class LocalLimitFilterTest : public testing::Test {
public:
  void setFilterConfigPerRoute(const std::string& yaml) {
    filter_ = std::make_unique<HttpLocalLimitFilter>(config_.get());
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    ProtoRouteConfig proto_config;
    TestUtility::loadFromYaml(yaml, proto_config);

    route_config_ = std::make_shared<Proxy::HttpFilters::LocalLimit::LocalLimitRouteConfig>(
        proto_config, server_factory_context_);

    ON_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(HttpLocalLimitFilter::name()))
        .WillByDefault(Return(route_config_.get()));
  }

  std::unique_ptr<HttpLocalLimitFilter> filter_;
  std::shared_ptr<LocalLimitRouteConfig> route_config_;
  std::shared_ptr<LocalLimitCommonConfig> config_;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Server::Configuration::MockServerFactoryContext> server_factory_context_;
};

TEST_F(LocalLimitFilterTest, SingleRateLimitTest) {
  const std::string test_config = R"EOF(
  rate_limit:
    - config:
        unit: SS
        rate: 2
  )EOF";
  setFilterConfigPerRoute(test_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/path"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, true));
}

TEST_F(LocalLimitFilterTest, NotMatchRateLimitTest) {
  const std::string test_config = R"EOF(
  rate_limit:
    - matcher:
        headers:
        - name: :path
          exact_match: /path2
      config:
        unit: SS
        rate: 1
  )EOF";
  setFilterConfigPerRoute(test_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/path"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LocalLimitFilterTest, MultipleRateLimitTest) {
  const std::string test_config = R"EOF(
  rate_limit:
    - matcher:
        headers:
        - name: :path
          exact_match: /path
      config:
        unit: SS
        rate: 2
    - matcher:
        headers:
        - name: :path
          exact_match: /path
      config:
        unit: SS
        rate: 1
  )EOF";
  setFilterConfigPerRoute(test_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/path"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, true));
}

TEST_F(LocalLimitFilterTest, NoRouteTest) {
  const std::string test_config = R"EOF(
  rate_limit:
    - matcher:
        headers:
        - name: :path
          exact_match: /path2
      config:
        unit: SS
        rate: 1
  )EOF";

  filter_ = std::make_unique<HttpLocalLimitFilter>(config_.get());
  filter_->setDecoderFilterCallbacks(decoder_callbacks_);

  EXPECT_CALL(decoder_callbacks_, route()).WillOnce(Return(nullptr));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/path"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LocalLimitFilterTest, NoRouteConfigTest) {
  const std::string test_config = R"EOF(
  rate_limit:
    - matcher:
        headers:
        - name: :path
          exact_match: /path2
      config:
        unit: SS
        rate: 1
  )EOF";

  filter_ = std::make_unique<HttpLocalLimitFilter>(config_.get());
  filter_->setDecoderFilterCallbacks(decoder_callbacks_);

  ON_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(HttpLocalLimitFilter::name()))
      .WillByDefault(Return(nullptr));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/path"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
