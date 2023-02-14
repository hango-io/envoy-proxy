#include "source/common/buffer/buffer_impl.h"
#include "source/filters/http/service_downgrade/service_downgrade.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/utility.h"

#include "absl/strings/string_view.h"
#include "fmt/format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArg;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ServiceDowngrade {

namespace {

class CommonConfigTest : public testing::Test {
public:
  void initCommonConfig(const absl::string_view config_yaml) {
    ProtoCommonConfig proto_config;
    TestUtility::loadFromYaml(std::string(config_yaml), proto_config);

    config_ = std::make_unique<CommonConfig>(proto_config, context_);
  }

protected:
  std::unique_ptr<CommonConfig> config_;

  testing::StrictMock<Server::Configuration::MockFactoryContext> context_;
};

TEST_F(CommonConfigTest, NoConfig) {
  initCommonConfig("{}");
  EXPECT_EQ(nullptr, config_->usedRemote());
}

TEST_F(CommonConfigTest, DefaultTimeout) {
  ProtoCommonConfig proto_config;
  constexpr absl::string_view CONFIG_NO_TIMEOUT = R"EOF(
    used_remote:
      cluster: fake_cluster
  )EOF";

  initCommonConfig(CONFIG_NO_TIMEOUT);

  auto& config = config_->usedRemote();

  EXPECT_EQ("fake_cluster", config->cluster());
  EXPECT_EQ(200, config->timeout().count());
}

class RouteConfigTest : public testing::Test {
public:
  void initRouteConfig(const absl::string_view config_yaml) {
    ProtoRouteConfig proto_config;
    TestUtility::loadFromYaml(std::string(config_yaml), proto_config);

    config_ = std::make_unique<RouteConfig>(proto_config);
  }

protected:
  std::unique_ptr<RouteConfig> config_;
};

TEST_F(RouteConfigTest, HeaderMatchTestCase) {
  constexpr absl::string_view CONFIG_NO_RPX = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
  )EOF";

  initRouteConfig(CONFIG_NO_RPX);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_name", "fake_value"}};

  // No rpx, then no downgrade.
  EXPECT_EQ(false,
            config_->shouldDowngrade(request_headers, Proxy::Common::Http::Direction::DECODE));

  constexpr absl::string_view CONFIG_WITH_RPX = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    downgrade_rpx:
      headers:
      - name: "fake_name2"
        exact_match: "fake_value2"
  )EOF";

  initRouteConfig(CONFIG_WITH_RPX);

  EXPECT_EQ(true,
            config_->shouldDowngrade(request_headers, Proxy::Common::Http::Direction::DECODE));

  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name2", "fake_value2_xxx"}};
  EXPECT_EQ(false,
            config_->shouldDowngrade(response_headers, Proxy::Common::Http::Direction::ENCODE));

  response_headers.remove("fake_name2");
  response_headers.addCopy("fake_name2", "fake_value2");

  EXPECT_EQ(true,
            config_->shouldDowngrade(response_headers, Proxy::Common::Http::Direction::ENCODE));
}

TEST_F(RouteConfigTest, DowngradeURICase) {
  constexpr absl::string_view CONFIG_NO_RPX = R"EOF(
    downgrade_uri: http://test.com/testpath
  )EOF";

  initRouteConfig(CONFIG_NO_RPX);

  // No rpx and no uri initialized.
  EXPECT_EQ(nullptr, config_->downgradeUri());

  constexpr absl::string_view CONFIG_WITH_ERROR_FORMAT = R"EOF(
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    downgrade_uri: http:/test.com//testpath
  )EOF";

  initRouteConfig(CONFIG_WITH_ERROR_FORMAT);

  EXPECT_EQ(nullptr, config_->downgradeUri());

  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    downgrade_uri: http://test.com/testpath
  )EOF";

  initRouteConfig(CONFIG);

  auto& uri = config_->downgradeUri();

  EXPECT_EQ("test.com", uri->host);
  EXPECT_EQ("/testpath", uri->path);
}

TEST_F(RouteConfigTest, OverrideRemoteConfigCase) {
  constexpr absl::string_view CONFIG_NO_RPX = R"EOF(
    override_remote:
      cluster: fake_cluster
  )EOF";

  initRouteConfig(CONFIG_NO_RPX);

  // No rpx and no override remote config initialized.
  EXPECT_EQ(nullptr, config_->overrideRemote());

  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initRouteConfig(CONFIG);

  auto& config = config_->overrideRemote();

  EXPECT_EQ("fake_cluster", config->cluster());
}

} // namespace

class FilterTest : public testing::Test {
public:
  FilterTest() : async_request_(&async_client_) {}

  void initFilter(const absl::string_view common_config, const absl::string_view route_config) {
    if (!route_config.empty()) {
      ProtoRouteConfig proto_route_config;
      TestUtility::loadFromYaml(std::string(route_config), proto_route_config);
      route_config_ = std::make_unique<RouteConfig>(proto_route_config);
    }

    ProtoCommonConfig proto_common_config;
    TestUtility::loadFromYaml(std::string(common_config), proto_common_config);
    common_config_ = std::make_unique<CommonConfig>(proto_common_config, context_);

    filter_ = std::make_unique<Filter>(common_config_.get());
    filter_->setDecoderFilterCallbacks(decoder_filter_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_filter_callbacks_);

    buffered_rqx_body_ = &filter_->buffered_rqx_body_;
    buffered_rpx_body_ = &filter_->buffered_rpx_body_;
    should_downgrade_ = &filter_->should_downgrade_;

    downgrade_complete_ = &filter_->downgrade_complete_;
    downgrade_suspend_ = &filter_->downgrade_suspend_;
    headers_only_resp_ = &filter_->headers_only_resp_;
    is_sending_request_ = &filter_->is_sending_request_;
    request_sender_ = &filter_->request_sender_;

    ON_CALL(*decoder_filter_callbacks_.route_, mostSpecificPerFilterConfig(Filter::name()))
        .WillByDefault(Return(route_config_.get()));
  }

protected:
  // ===== Status of filter.
  Buffer::OwnedImpl* buffered_rqx_body_{};
  Buffer::OwnedImpl* buffered_rpx_body_{};

  bool* should_downgrade_{};

  bool* downgrade_complete_{};
  bool* downgrade_suspend_{};

  bool* headers_only_resp_{};

  bool* is_sending_request_{};

  Proxy::Common::Sender::HttpxRequestSenderPtr* request_sender_{};
  // ====

  std::unique_ptr<Filter> filter_;
  std::unique_ptr<CommonConfig> common_config_;
  std::unique_ptr<RouteConfig> route_config_;

  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> decoder_filter_callbacks_;

  NiceMock<Envoy::Http::MockStreamEncoderFilterCallbacks> encoder_filter_callbacks_;

  NiceMock<Envoy::Server::Configuration::MockFactoryContext> context_;
  NiceMock<Envoy::Upstream::MockThreadLocalCluster> cluster_;
  NiceMock<Envoy::Http::MockAsyncClient> async_client_;
  NiceMock<Envoy::Http::MockAsyncClientRequest> async_request_;
};

namespace {

TEST_F(FilterTest, NoRouteConfigNoDowngrade) {
  initFilter("{}", "");

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_name", "fake_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_EQ(false, *should_downgrade_);
}

TEST_F(FilterTest, RequestNotMatchCase) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_name", "fake_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_EQ(false, *should_downgrade_);

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));

  EXPECT_EQ(0, (*buffered_rqx_body_).length());
}

TEST_F(FilterTest, ResponseNotMatchCase) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_EQ(true, *should_downgrade_);

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));

  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  Http::TestResponseHeaderMapImpl response_headers(request_headers);

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(false, *should_downgrade_);

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(body, true));

  EXPECT_EQ(nullptr, (*request_sender_).get());
}

TEST_F(FilterTest, ResponseNoBodyButDowngradeWithBody) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));

  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian. This test simulates the case where the original
  // response has no body but the degraded response has a body. So the end_stream of decodeHeaders
  // is set be true.
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, true));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(true, *headers_only_resp_);

  auto mock_downgrade_headers = std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>();
  *mock_downgrade_headers = {{"fake_name", "fake_new_value"}, {"fake_name2", "fake_value2"}};

  auto mock_downgrade_response =
      std::make_unique<Http::ResponseMessageImpl>(std::move(mock_downgrade_headers));
  mock_downgrade_response->body().add("Test Downgrade Body");

  EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).WillOnce(testing::ReturnNull());
  EXPECT_CALL(encoder_filter_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onSuccess(async_request_, std::move(mock_downgrade_response));

  EXPECT_EQ(true, *downgrade_complete_);
  EXPECT_EQ("fake_new_value", response_headers.get_("fake_name"));
  EXPECT_EQ("fake_value2", response_headers.get_("fake_name2"));
}

TEST_F(FilterTest, ResponseNoBodyAndDowngradeNoBody) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));

  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian. This test simulates the case where the original
  // response has no body but the degraded response has a body. So the end_stream of decodeHeaders
  // is set be true.
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, true));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(true, *headers_only_resp_);

  auto mock_downgrade_headers = std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>();
  *mock_downgrade_headers = {{"fake_name", "fake_new_value"}, {"fake_name2", "fake_value2"}};

  auto mock_downgrade_response =
      std::make_unique<Http::ResponseMessageImpl>(std::move(mock_downgrade_headers));

  EXPECT_CALL(encoder_filter_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onSuccess(async_request_, std::move(mock_downgrade_response));

  EXPECT_EQ(true, *downgrade_complete_);
  EXPECT_EQ("fake_new_value", response_headers.get_("fake_name"));
  EXPECT_EQ("fake_value2", response_headers.get_("fake_name2"));
}

TEST_F(FilterTest, ResponseWithBodyButDowngradeNoBody) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));

  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndBuffer,
            filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(false, *headers_only_resp_);

  auto mock_downgrade_headers = std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>();
  *mock_downgrade_headers = {{"fake_name", "fake_new_value"}, {"fake_name2", "fake_value2"}};

  // Downgrade response has no body.
  auto mock_downgrade_response =
      std::make_unique<Http::ResponseMessageImpl>(std::move(mock_downgrade_headers));

  EXPECT_CALL(encoder_filter_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onSuccess(async_request_, std::move(mock_downgrade_response));

  EXPECT_EQ(false, *downgrade_complete_);
  EXPECT_EQ("fake_new_value", response_headers.get_("fake_name"));
  EXPECT_EQ("fake_value2", response_headers.get_("fake_name2"));

  Envoy::Buffer::OwnedImpl response_body("Test Original Body");

  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer,
            filter_->encodeData(response_body, false));
  EXPECT_EQ(false, *downgrade_complete_);
  // Original response body will be drained.
  EXPECT_EQ(0, response_body.length());

  response_body.add("Test Original Body");

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, true));
  EXPECT_EQ(true, *downgrade_complete_);
  EXPECT_EQ(0, response_body.length());
}

TEST_F(FilterTest, ResponseWithBodyAndDowngradeWithBody) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndBuffer,
            filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(false, *headers_only_resp_);

  auto mock_downgrade_headers = std::make_unique<Envoy::Http::TestResponseHeaderMapImpl>();
  *mock_downgrade_headers = {{"fake_name", "fake_new_value"}, {"fake_name2", "fake_value2"}};

  auto mock_downgrade_response =
      std::make_unique<Http::ResponseMessageImpl>(std::move(mock_downgrade_headers));
  mock_downgrade_response->body().add("Test Downgrade Body");

  EXPECT_CALL(encoder_filter_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onSuccess(async_request_, std::move(mock_downgrade_response));

  EXPECT_EQ("Test Downgrade Body", (*buffered_rpx_body_).toString());

  EXPECT_EQ(false, *downgrade_complete_);
  EXPECT_EQ("fake_new_value", response_headers.get_("fake_name"));
  EXPECT_EQ("fake_value2", response_headers.get_("fake_name2"));

  Envoy::Buffer::OwnedImpl response_body("Test Original Body");

  EXPECT_CALL(encoder_filter_callbacks_, addEncodedData(_, false)).WillOnce(testing::ReturnNull());
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, true));

  EXPECT_EQ(true, *downgrade_complete_);
  // Original response body will be drained.
  EXPECT_EQ(0, response_body.length());
}

TEST_F(FilterTest, DowngradeFailure) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndBuffer,
            filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(false, *headers_only_resp_);
  EXPECT_EQ(true, *is_sending_request_);

  EXPECT_CALL(encoder_filter_callbacks_, continueEncoding()).WillOnce(testing::ReturnNull());

  filter_->onFailure(async_request_, Envoy::Http::AsyncClient::FailureReason::Reset);
  EXPECT_EQ(false, *is_sending_request_);
  EXPECT_EQ(true, *downgrade_suspend_);

  Envoy::Buffer::OwnedImpl response_body("Test Original Body");
  // Downgrade suspend and still return Http::FilterDataStatus::Continue.
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, false));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(response_body, true));
}

TEST_F(FilterTest, DowngradeCancelBeforeCallback) {
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter("{}", CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopAllIterationAndBuffer,
            filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_NE(nullptr, (*request_sender_).get());
  EXPECT_EQ(false, *headers_only_resp_);
  EXPECT_EQ(true, *is_sending_request_);

  EXPECT_CALL(async_request_, cancel()).WillOnce(testing::ReturnNull());
  filter_->onDestroy();
  EXPECT_EQ(false, *is_sending_request_);
  EXPECT_EQ(nullptr, (*request_sender_).get());
  EXPECT_EQ(true, *downgrade_suspend_);
}

TEST_F(FilterTest, DowngradeRemoteNoOverride) {
  constexpr absl::string_view COMMON_CONFIG = R"EOF(
    used_remote:
      cluster: fake_common_cluster
  )EOF";
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
  )EOF";

  initFilter(COMMON_CONFIG, CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_common_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, true));
}

TEST_F(FilterTest, DowngradeRemoteWithOverride) {
  constexpr absl::string_view COMMON_CONFIG = R"EOF(
    used_remote:
      cluster: fake_common_cluster
  )EOF";
  constexpr absl::string_view CONFIG = R"EOF(
    downgrade_rqx:
      headers:
      - name: "fake_request_name"
        exact_match: "fake_request_value"
    downgrade_rpx:
      headers:
      - name: "fake_name"
        exact_match: "fake_value"
    override_remote:
      cluster: fake_cluster
  )EOF";

  initFilter(COMMON_CONFIG, CONFIG);

  Http::TestRequestHeaderMapImpl request_headers = {{"fake_request_name", "fake_request_value"}};

  Buffer::OwnedImpl body;
  body.add("Test Body");

  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(true, *should_downgrade_);
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(body, true));
  EXPECT_EQ("Test Body", (*buffered_rqx_body_).toString());

  // Response and downgrade.
  Http::TestResponseHeaderMapImpl response_headers = {{"fake_name", "fake_value"}};

  const absl::string_view target = "fake_cluster";
  EXPECT_CALL(context_.cluster_manager_, getThreadLocalCluster(target)).WillOnce(Return(&cluster_));
  EXPECT_CALL(cluster_, httpAsyncClient()).WillOnce(ReturnRef(async_client_));
  EXPECT_CALL(async_client_, send_(_, _, _)).WillOnce(Return(&async_request_));

  // Send downgrade request and stop filter chian.
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, true));
}

} // namespace

} // namespace ServiceDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
