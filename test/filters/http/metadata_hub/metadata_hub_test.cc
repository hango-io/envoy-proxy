#include "source/common/config/metadata.h"
#include "source/filters/http/metadata_hub/metadata_hub.h"

#include "test/mocks/common.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/registry.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace MetadataHub {

class MetadataHubTest : public testing::Test {
public:
  MetadataHubTest() : registered_factory_foo_(metadata_hub_factory_) {}
  void SetUpTest(const std::string& yaml) {

    ProtoCommonConfig proto_config{};
    TestUtility::loadFromYaml(yaml, proto_config);

    config_.reset(new MetadataHubCommonConfig(proto_config));

    filter_ = std::make_unique<HttpMetadataHubFilter>(config_.get());
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    ON_CALL(decoder_callbacks_, streamInfo()).WillByDefault(testing::ReturnRef(stream_info_));
  }

  std::unique_ptr<HttpMetadataHubFilter> filter_;
  std::shared_ptr<MetadataHubCommonConfig> config_;

  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<StreamInfo::MockStreamInfo> stream_info_;
  TypedMetadataFactory metadata_hub_factory_;
  Registry::InjectFactory<Config::TypedMetadataFactory> registered_factory_foo_;
};

TEST_F(MetadataHubTest, DecodeHeadersToStateTest) {
  const std::string test_config = R"EOF(
  decode_headers_to_state:
    - name: :path
      rename: path
    - name: fake
      rename: refake
  )EOF";
  SetUpTest(test_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {"fake", "fakevalue"}, {":method", "GET"}, {":path", "/path"}};
  auto status = filter_->decodeHeaders(request_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);

  auto state = stream_info_.filterState();
  EXPECT_EQ(state->hasDataWithName("path"), true);
  EXPECT_EQ(state->hasDataWithName("refake"), true);
  EXPECT_EQ(state->getDataReadOnlyGeneric("path")->serializeAsString(), "/path");
  EXPECT_EQ(state->getDataReadOnlyGeneric("refake")->serializeAsString(), "fakevalue");
}

TEST_F(MetadataHubTest, EncodeHeadersToStateTest) {
  const std::string test_config = R"EOF(
  encode_headers_to_state:
    - name: :status
      rename: status
    - name: fake_res
      rename: refake_res
  )EOF";
  SetUpTest(test_config);

  Http::TestResponseHeaderMapImpl response_headers{{"fake_res", "fakevalue_res"},
                                                   {":status", "200"}};
  auto status = filter_->encodeHeaders(response_headers, true);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);

  auto state = stream_info_.filterState();
  EXPECT_EQ(state->hasDataWithName("status"), true);
  EXPECT_EQ(state->hasDataWithName("refake_res"), true);
  EXPECT_EQ(state->getDataReadOnlyGeneric("status")->serializeAsString(), "200");
  EXPECT_EQ(state->getDataReadOnlyGeneric("refake_res")->serializeAsString(), "fakevalue_res");
}

TEST_F(MetadataHubTest, RouteMetadataToStateTest) {

  const std::string test_config = R"EOF(
  route_metadata_to_state:
    - proxy.filters.http.metadatahub
  )EOF";
  SetUpTest(test_config);

  envoy::config::core::v3::Metadata metadata;
  (*metadata.mutable_filter_metadata())["proxy.filters.http.metadatahub"] =
      MessageUtil::keyValueStruct("fake_key", "fake_val");
  Config::TypedMetadataImpl<Config::TypedMetadataFactory> typed(metadata);

  EXPECT_CALL(*decoder_callbacks_.route_, typedMetadata()).WillOnce(testing::ReturnRef(typed));

  Http::TestRequestHeaderMapImpl request_headers{
      {"fake", "fakevalue"}, {":method", "GET"}, {":path", "/path"}};
  auto status = filter_->decodeHeaders(request_headers, true);
  auto state = stream_info_.filterState();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
  EXPECT_EQ(state->hasDataWithName("fake_key"), true);
  EXPECT_EQ(state->getDataReadOnlyGeneric("fake_key")->serializeAsString(), "fake_val");
}

TEST_F(MetadataHubTest, NoRouteTest) {

  const std::string test_config = R"EOF(
  route_metadata_to_state:
    - proxy.filters.http.metadatahub
  )EOF";
  SetUpTest(test_config);

  envoy::config::core::v3::Metadata metadata;
  (*metadata.mutable_filter_metadata())["proxy.filters.http.metadatahub"] =
      MessageUtil::keyValueStruct("fake_key", "fake_val");
  Config::TypedMetadataImpl<Config::TypedMetadataFactory> typed(metadata);

  EXPECT_CALL(*decoder_callbacks_.route_, typedMetadata()).Times(0);
  EXPECT_CALL(decoder_callbacks_, route()).WillOnce(Return(nullptr));

  Http::TestRequestHeaderMapImpl request_headers{
      {"fake", "fakevalue"}, {":method", "GET"}, {":path", "/path"}};
  auto status = filter_->decodeHeaders(request_headers, true);
  auto state = stream_info_.filterState();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
  EXPECT_EQ(state->hasDataWithName("fake_key"), false);
}

TEST_F(MetadataHubTest, NoRouteEntryTest) {

  const std::string test_config = R"EOF(
  route_metadata_to_state:
    - proxy.filters.http.metadatahub
  )EOF";
  SetUpTest(test_config);

  envoy::config::core::v3::Metadata metadata;
  (*metadata.mutable_filter_metadata())["proxy.filters.http.metadatahub"] =
      MessageUtil::keyValueStruct("fake_key", "fake_val");
  Config::TypedMetadataImpl<Config::TypedMetadataFactory> typed(metadata);

  EXPECT_CALL(*decoder_callbacks_.route_, typedMetadata()).Times(0);
  EXPECT_CALL(*decoder_callbacks_.route_, routeEntry()).WillOnce(Return(nullptr));

  Http::TestRequestHeaderMapImpl request_headers{
      {"fake", "fakevalue"}, {":method", "GET"}, {":path", "/path"}};
  auto status = filter_->decodeHeaders(request_headers, true);
  auto state = stream_info_.filterState();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, status);
  EXPECT_EQ(state->hasDataWithName("fake_key"), false);
}

} // namespace MetadataHub

} // namespace HttpFilters

} // namespace Proxy
} // namespace Envoy
