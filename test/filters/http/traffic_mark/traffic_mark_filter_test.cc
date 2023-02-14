#include <memory>

#include "source/common/buffer/buffer_impl.h"
#include "source/common/http/message_impl.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/filters/http/traffic_mark/traffic_mark_filter.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

using testing::_;
using testing::AtLeast;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {
namespace {

class HttpTrafficMarkFilterTest : public testing::Test {
public:
  void SetUpTest() { SetUpTest(filter_config_); }

  void SetUpTest(const std::string& yaml) {
    proxy::filters::http::traffic_mark::v2::ProtoCommonConfig proto_config{};
    TestUtility::loadFromYaml(yaml, proto_config);

    config_.reset(new ConfigImpl(proto_config));

    filter_ = std::make_unique<HttpTrafficMarkFilter>(*config_, cluster_manager_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  const std::string filter_config_ = R"EOF(
  header_key: color
  match_key: match_color
  all_colors_key: all_colors
  )EOF";

  std::shared_ptr<ConfigImpl> config_;
  std::unique_ptr<HttpTrafficMarkFilter> filter_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
};

TEST_F(HttpTrafficMarkFilterTest, NoColor) {
  SetUpTest();

  auto metadata = TestUtility::parseYaml<envoy::config::core::v3::Metadata>(
      R"EOF(
        filter_metadata:
          qingzhou:
            all_colors: a,a.b,a.b.c1,a.b.c2
      )EOF");
  EXPECT_CALL(*(cluster_manager_.thread_local_cluster_.cluster_.info_), metadata())
      .WillRepeatedly(ReturnRef(metadata));

  Http::TestRequestHeaderMapImpl headers{
      {":method", "GET"}, {":authority", "www.proxy.com"}, {":path", "/users/123"}};
  EXPECT_CALL(*decoder_callbacks_.route_, routeEntry());

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ("", Envoy::Config::Metadata::metadataValue(
                    &decoder_callbacks_.stream_info_.metadata_,
                    Envoy::Config::MetadataFilters::get().ENVOY_LB, config_->matchKey())
                    .string_value());
}

TEST_F(HttpTrafficMarkFilterTest, Match) {
  SetUpTest();

  auto metadata = TestUtility::parseYaml<envoy::config::core::v3::Metadata>(
      R"EOF(
        filter_metadata:
          qingzhou:
            all_colors: a,a.b,a.b.c1,a.b.c2
      )EOF");
  EXPECT_CALL(*(cluster_manager_.thread_local_cluster_.cluster_.info_), metadata())
      .WillRepeatedly(ReturnRef(metadata));

  ON_CALL(cluster_manager_, getThreadLocalCluster(_))
      .WillByDefault(Return(&cluster_manager_.thread_local_cluster_));

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.proxy.com"},
                                         {":path", "/users/123"},
                                         {"color", "a.b.c1"}};
  EXPECT_CALL(*decoder_callbacks_.route_, routeEntry());

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ("a.b.c1", Envoy::Config::Metadata::metadataValue(
                          &decoder_callbacks_.stream_info_.metadata_,
                          Envoy::Config::MetadataFilters::get().ENVOY_LB, config_->matchKey())
                          .string_value());
}

TEST_F(HttpTrafficMarkFilterTest, NotMatchFallback) {
  SetUpTest();

  auto metadata = TestUtility::parseYaml<envoy::config::core::v3::Metadata>(
      R"EOF(
        filter_metadata:
          qingzhou:
            all_colors: a,a.b,a.b.c1,a.b.c2
      )EOF");

  EXPECT_CALL(*(cluster_manager_.thread_local_cluster_.cluster_.info_), metadata())
      .WillRepeatedly(ReturnRef(metadata));

  ON_CALL(cluster_manager_, getThreadLocalCluster(_))
      .WillByDefault(Return(&cluster_manager_.thread_local_cluster_));

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.proxy.com"},
                                         {":path", "/users/123"},
                                         {"color", "a.b.c3"}};
  EXPECT_CALL(*decoder_callbacks_.route_, routeEntry());

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ("a.b", Envoy::Config::Metadata::metadataValue(
                       &decoder_callbacks_.stream_info_.metadata_,
                       Envoy::Config::MetadataFilters::get().ENVOY_LB, config_->matchKey())
                       .string_value());
}

TEST_F(HttpTrafficMarkFilterTest, NotMatchFallbackToEmpty) {
  SetUpTest();

  auto metadata = TestUtility::parseYaml<envoy::config::core::v3::Metadata>(
      R"EOF(
        filter_metadata:
          qingzhou:
            all_colors: a,a.b,a.b.c
      )EOF");

  EXPECT_CALL(*(cluster_manager_.thread_local_cluster_.cluster_.info_), metadata())
      .WillRepeatedly(ReturnRef(metadata));

  ON_CALL(cluster_manager_, getThreadLocalCluster(_))
      .WillByDefault(Return(&cluster_manager_.thread_local_cluster_));

  Http::TestRequestHeaderMapImpl headers{{":method", "GET"},
                                         {":authority", "www.proxy.com"},
                                         {":path", "/users/123"},
                                         {"color", "abc"}};
  EXPECT_CALL(*decoder_callbacks_.route_, routeEntry());

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ("", Envoy::Config::Metadata::metadataValue(
                    &decoder_callbacks_.stream_info_.metadata_,
                    Envoy::Config::MetadataFilters::get().ENVOY_LB, config_->matchKey())
                    .string_value());
}

} // namespace
} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
