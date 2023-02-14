#include <cstddef>
#include <string>

#include "envoy/type/v3/percent.pb.h"

#include "source/common/protobuf/protobuf.h"
#include "source/filters/http/detailed_stats/detailed_stats.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/server/factory_context.h"
#include "test/test_common/test_runtime.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {
namespace {

class MockTypedMetadata : public Envoy::Config::TypedMetadata {
public:
  MOCK_METHOD(const Envoy::Config::TypedMetadata::Object*, getData, (const std::string& key),
              (const));
};

class TestTestScopedRuntimeWithStore : public TestScopedRuntime {
public:
  Stats::TestUtil::TestStore& storeForTest() { return store_; }
};

TEST(FactoryTest, FactoryTest) {
  const std::string string_config = R"EOF(
      stat_prefix: "host"
      stat_tags:
      - key: "key1"
        val: "val1"
      - key: "key2"
        val: "val2"
      )EOF";

  TestTestScopedRuntimeWithStore scoped_runtime;

  {
    TagsRouteTypedMetadataFactory factory;

    ProtobufWkt::Struct struct_config;
    TestUtility::loadFromYaml(string_config, struct_config);
    EXPECT_NE(nullptr, factory.parse(struct_config));
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags proto_config;
    TestUtility::loadFromYaml(string_config, proto_config);

    ProtobufWkt::Any any_config;
    any_config.PackFrom(proto_config);
    EXPECT_NE(nullptr, factory.parse(any_config));
  }

  {
    TagsHostTypedMetadataFactory factory;

    ProtobufWkt::Struct struct_config;
    TestUtility::loadFromYaml(string_config, struct_config);
    EXPECT_NE(nullptr, factory.parse(struct_config));
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags proto_config;
    TestUtility::loadFromYaml(string_config, proto_config);

    ProtobufWkt::Any any_config;
    any_config.PackFrom(proto_config);
    EXPECT_NE(nullptr, factory.parse(any_config));
  }
}

class FilterTest : public testing::Test {
public:
  void setUpTest(const std::string& common_config_string, const std::string& router_config_string) {
    proxy::filters::http::detailed_stats::v2::ProtoCommonConfig proto_common_config;
    {
      if (!common_config_string.empty()) {
        TestUtility::loadFromYaml(common_config_string, proto_common_config);
      }
    }
    config_ = std::make_shared<CommonConfig>(proto_common_config, context_);

    {
      if (!router_config_string.empty()) {
        proxy::filters::http::detailed_stats::v2::ProtoRouterConfig proto_router_config;
        TestUtility::loadFromYaml(router_config_string, proto_router_config);
        router_config_ = std::make_shared<RouterConfig>(proto_router_config);
      }
    }

    filter_ = std::make_unique<Filter>(config_.get());
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);

    {
      const std::string string_config = R"EOF(
      stat_prefix: "host"
      stat_tags:
      - key: "key1"
        val: "val1"
      - key: "key2"
        val: "val2"
      )EOF";

      proxy::filters::http::detailed_stats::v2::PrefixedStatTags proto_config;
      TestUtility::loadFromYaml(string_config, proto_config);

      stats_ = std::make_shared<HttpFilters::DetailedStats::TagsMetadataStats>(proto_config);
    }
    {
      const std::string string_config = R"EOF(
      stat_prefix: "route"
      stat_tags:
      - key: "key1"
        val: "val1"
      - key: "key2"
        val: "val2"
      )EOF";

      proxy::filters::http::detailed_stats::v2::PrefixedStatTags proto_config;
      TestUtility::loadFromYaml(string_config, proto_config);

      route_stats_ = std::make_shared<HttpFilters::DetailedStats::TagsMetadataStats>(proto_config);
    }
  }

  NiceMock<Server::Configuration::MockFactoryContext> context_;

  std::shared_ptr<CommonConfig> config_;
  std::shared_ptr<RouterConfig> router_config_;

  std::unique_ptr<Filter> filter_;

  NiceMock<MockTypedMetadata> typed_metadata_;
  NiceMock<MockTypedMetadata> route_typed_metadata_;

  std::shared_ptr<HttpFilters::DetailedStats::TagsMetadataStats> stats_;
  std::shared_ptr<HttpFilters::DetailedStats::TagsMetadataStats> route_stats_;

  NiceMock<Envoy::Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
};

TEST_F(FilterTest, NoRequestOrResponse) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  filter_->onDestroy();

  // Default five counters for runtime:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(5, scoped_runtime.storeForTest().counters().size());

  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NoRoute) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(encoder_callbacks_, route()).WillOnce(Return(nullptr));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(5, scoped_runtime.storeForTest().counters().size());

  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NoUpstreamHostAndNoRouteStats) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(nullptr));
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(nullptr);

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(5, scoped_runtime.storeForTest().counters().size());

  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NoTypedMetadata) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(nullptr));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(nullptr));

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(5, scoped_runtime.storeForTest().counters().size());

  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequest) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/"}, {":scheme", "http"}, {":authority", "host"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequestWithMatcher) {
  TestTestScopedRuntimeWithStore scoped_runtime;

  const std::string router_config_yaml = R"EOF(
    matched_stats:
    - matcher:
        path:
          exact: "/"
      stat_tags:
      - key: "route_tag1"
        val: "route_val1"
      - key: "route_tag2"
        val: "route_val2"
    - matcher:
        headers:
        - name: "x-header"
          string_match:
            exact: "header_value"
      stat_tags:
      - key: "route_tag3"
        val: "route_val3"
      - key: "route_tag4"
        val: "route_val4"
    )EOF";

  setUpTest("", router_config_yaml);

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":path", "/"},
                                                 {":scheme", "http"},
                                                 {":authority", "host"},
                                                 {"x-header", "header_value"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  EXPECT_CALL(*encoder_callbacks_.route_, traversePerFilterConfig(Filter::name(), _))
      .WillOnce(
          testing::Invoke([this](const std::string&,
                                 std::function<void(const Router::RouteSpecificFilterConfig&)> cb) {
            cb(*router_config_);
          }));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-.route_tag1.route_val1.route_tag2."
                            "route_val2.route_tag3.route_val3.route_tag4.route_val4")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-.route_tag1.route_val1.route_tag2."
                            "route_val2.route_tag3.route_val3.route_tag4.route_val4")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequestWithGlobalMatcher) {
  TestTestScopedRuntimeWithStore scoped_runtime;

  const std::string config_yaml = R"EOF(
    matched_stats:
    - matcher:
        path:
          exact: "/"
      stat_tags:
      - key: "route_tag1"
        val: "route_val1"
      - key: "route_tag2"
        val: "route_val2"
      - key: "route_tag3"
        val: "route_val3"
      - key: "route_tag4"
        val: "route_val4"
    )EOF";

  setUpTest(config_yaml, "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":path", "/"},
                                                 {":scheme", "http"},
                                                 {":authority", "host"},
                                                 {"x-header", "header_value"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-.route_tag1.route_val1.route_tag2."
                            "route_val2.route_tag3.route_val3.route_tag4.route_val4")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-.route_tag1.route_val1.route_tag2."
                            "route_val2.route_tag3.route_val3.route_tag4.route_val4")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequestWithDuration) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/"}, {":scheme", "http"}, {":authority", "host"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  EXPECT_CALL(encoder_callbacks_.stream_info_, requestComplete())
      .WillOnce(testing::Return(
          absl::optional<std::chrono::nanoseconds>(std::chrono::nanoseconds(1000000000))));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.200.response_flags.-")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequestWithUnknownCode) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/"}, {":scheme", "http"}, {":authority", "host"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "600"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  EXPECT_CALL(encoder_callbacks_.stream_info_, requestComplete())
      .WillOnce(testing::Return(
          absl::optional<std::chrono::nanoseconds>(std::chrono::nanoseconds(1000000000))));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.-.response_flags.-")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.-.response_flags.-")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

TEST_F(FilterTest, NormalRequestWithFlag) {
  TestTestScopedRuntimeWithStore scoped_runtime;
  setUpTest("", "");

  EXPECT_CALL(*encoder_callbacks_.route_, typedMetadata())
      .WillOnce(testing::ReturnRef(route_typed_metadata_));
  EXPECT_CALL(route_typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(route_stats_.get()));

  auto mock_host = std::make_shared<testing::NiceMock<Upstream::MockHostDescription>>();
  encoder_callbacks_.stream_info_.upstream_info_->setUpstreamHost(mock_host);
  EXPECT_CALL(*mock_host, typedMetadata()).WillOnce(testing::ReturnRef(typed_metadata_));
  EXPECT_CALL(typed_metadata_, getData("proxy.metadata_stats.detailed_stats"))
      .WillOnce(testing::Return(stats_.get()));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":path", "/"}, {":scheme", "http"}, {":authority", "host"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "503"}};

  EXPECT_CALL(encoder_callbacks_.stream_info_, getRequestHeaders())
      .WillOnce(testing::Return(&request_headers));

  EXPECT_CALL(encoder_callbacks_.stream_info_, requestComplete())
      .WillOnce(testing::Return(
          absl::optional<std::chrono::nanoseconds>(std::chrono::nanoseconds(1000000000))));

  EXPECT_CALL(encoder_callbacks_.stream_info_, responseFlags())
      .WillOnce(testing::Return(static_cast<uint64_t>(StreamInfo::ResponseFlag::NoClusterFound)));

  filter_->encodeHeaders(response_headers, true);
  filter_->onDestroy();

  // Default five counters for runtime and new counters for detailed stats:
  // * runtime.deprecated_feature_use
  // * runtime.override_dir_exists
  // * runtime.load_success
  // * runtime.load_error
  // * runtime.override_dir_not_exists
  EXPECT_EQ(7, scoped_runtime.storeForTest().counters().size());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("route.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.503.response_flags.NC")
                   .value());
  EXPECT_EQ(1, scoped_runtime.storeForTest()
                   .counter("host.requests_total.key1.val1.key2.val2.request_protocol.http."
                            "response_code.503.response_flags.NC")
                   .value());
  stats_.reset();
  route_stats_.reset();
}

} // namespace
} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
