#include "source/filters/http/dynamic_downgrade/dynamic_downgrade_filter.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DynamicDowngrade {

class DynamicDowngradeFilterTest : public testing::Test {
public:
  DynamicDowngradeFilterTest() : async_request_(&async_client_) {}

  void SetUpTest(const std::string& yaml) {

    ProtoCommonConfig proto_config{};
    TestUtility::loadFromYaml(yaml, proto_config);

    config_.reset(new Proxy::HttpFilters::DynamicDowngrade::DynamicDowngradeCommonConfig(
        proto_config, context_));

    filter_ = std::make_unique<HttpDynamicDowngradeFilter>(config_.get());
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  void setFilterConfigPerRoute(const std::string& yaml) {
    ProtoRouteConfig proto_config;
    TestUtility::loadFromYaml(yaml, proto_config);

    route_config_ =
        std::make_shared<Proxy::HttpFilters::DynamicDowngrade::DynamicDowngradeRouteConfig>(
            proto_config);

    ON_CALL(*decoder_callbacks_.route_,
            mostSpecificPerFilterConfig(HttpDynamicDowngradeFilter::name()))
        .WillByDefault(Return(route_config_.get()));
    ON_CALL(*encoder_callbacks_.route_,
            mostSpecificPerFilterConfig(HttpDynamicDowngradeFilter::name()))
        .WillByDefault(Return(route_config_.get()));
  }

  std::unique_ptr<HttpDynamicDowngradeFilter> filter_;
  std::shared_ptr<DynamicDowngradeRouteConfig> route_config_;
  std::shared_ptr<DynamicDowngradeCommonConfig> config_;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Envoy::Server::Configuration::MockFactoryContext> context_;
  NiceMock<Envoy::Upstream::MockThreadLocalCluster> cluster_;
  NiceMock<Envoy::Http::MockAsyncClient> async_client_;
  NiceMock<Envoy::Http::MockAsyncClientRequest> async_request_;
};

TEST_F(DynamicDowngradeFilterTest, HTTPXDowngrade) {
  const std::string test_common_config = R"EOF(
  used_remote:
    cluster: httpbin
    timeout: 2s
  used_caches:
    - local: {}
  )EOF";

  const std::string test_route_config = R"EOF(
  downgrade_rpx:
    headers:
      - name: :status
        safe_regex_match:
          { "regex": "503", "google_re2": {} }
  downgrade_uri: http://httpbin.org/anything?httpbin=downgrade
  downgrade_src: HTTPX
  )EOF";

  SetUpTest(test_common_config);
  setFilterConfigPerRoute(test_route_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"}, {":path", "/status/503"}};

  EXPECT_EQ(true, route_config_->shouldDowngrade(request_headers,
                                                 Proxy::Common::Http::Direction::DECODE));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "503"}};
  const absl::string_view target = "httpbin";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, true));

  auto mock_downgrade_headers = std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>();
  *mock_downgrade_headers = {{":status", "200"}, {"fake_name2", "fake_value2"}};

  auto mock_downgrade_response =
      std::make_unique<Http::ResponseMessageImpl>(std::move(mock_downgrade_headers));
  mock_downgrade_response->body().add("Test Downgrade Body");

  EXPECT_CALL(encoder_callbacks_, addEncodedData(_, false)).WillOnce(testing::ReturnNull());
  EXPECT_CALL(encoder_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onSuccess(async_request_, std::move(mock_downgrade_response));

  EXPECT_EQ("200", response_headers.get_(":status"));
  EXPECT_EQ("fake_value2", response_headers.get_("fake_name2"));
}

TEST_F(DynamicDowngradeFilterTest, CacheDowngrade) {
  const std::string test_common_config = R"EOF(
  used_remote:
    cluster: httpbin
    timeout: 2s
  used_caches:
    - local: {}
  )EOF";

  const std::string test_route_config = R"EOF(
  downgrade_rqx:
    headers:
      - name: comefrom
        exact_match: "test"
  downgrade_rpx:
    headers:
      - name: httpbin
        safe_regex_match:
          { "regex": "downgrade", "google_re2": {} }
  cache_rpx_rpx:
    headers:
      - name: :status
        safe_regex_match:
          { "regex": "200", "google_re2": {} }
  )EOF";

  SetUpTest(test_common_config);
  setFilterConfigPerRoute(test_route_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/"}, {"host", "a.com"}, {"comefrom", "test"}};

  EXPECT_EQ(true, route_config_->shouldDowngrade(request_headers,
                                                 Proxy::Common::Http::Direction::DECODE));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}, {"httpbin", "downgrade"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, true));
}

} // namespace DynamicDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
