#include "common/buffer/buffer_impl.h"
#include "filters/http/well_known_names.h"
#include "filters/http/static_downgrade/static_downgrade_filter.h"

#include "test/test_common/utility.h"

#include "test/mocks/common.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/upstream/mocks.h"

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
namespace StaticDowngrade {

class StaticDowngradeConfigTest : public testing::Test {
public:
  void initDowngradeRqx() {
    envoy::config::route::v3::HeaderMatcher hm1, hm2;
    hm1.set_name("comefrom");
    hm1.set_exact_match("unittest");

    hm2.set_name("should_downgrade");
    auto regex_match = hm2.mutable_safe_regex_match();
    regex_match->set_regex("s....d");
    regex_match->mutable_google_re2();

    auto rqx = proto_config_.mutable_downgrade_rqx()->mutable_headers();
    rqx->Add(std::move(hm1));
    rqx->Add(std::move(hm2));
  }
  void initDowngradeRpx() {
    envoy::config::route::v3::HeaderMatcher hm1;
    hm1.set_name("respfrom");
    hm1.set_exact_match("unittest");

    auto rpx = proto_config_.mutable_downgrade_rpx()->mutable_headers();
    rpx->Add(std::move(hm1));
  }

  void initHttpStatus() { proto_config_.mutable_static_response()->set_http_status(429); }

  void initStringBody() {
    proto_config_.mutable_static_response()->mutable_body()->set_inline_string(
        "body from unittest");
  }

  void initFileBody() {
    proto_config_.mutable_static_response()->mutable_body()->set_filename("/tmp/downgrade.log");
  }

  ProtoDowngradeConfig proto_config_;
  testing::StrictMock<Server::Configuration::MockServerFactoryContext> context_;
};

TEST_F(StaticDowngradeConfigTest, NoDowngradeRpx) {

  Http::TestRequestHeaderMapImpl request_headers;
  Http::TestResponseHeaderMapImpl response_headers{{"respfrom", "unittest"}};

  StaticDowngradeConfig config1(proto_config_, context_);

  EXPECT_EQ(false, config1.shouldDowngrade(request_headers, response_headers));

  initDowngradeRpx();

  StaticDowngradeConfig config2(proto_config_, context_);

  EXPECT_EQ(true, config2.shouldDowngrade(request_headers, response_headers));
}

TEST_F(StaticDowngradeConfigTest, RequestDisableDowngrade) {
  initDowngradeRqx();
  initDowngradeRpx();

  StaticDowngradeConfig config(proto_config_, context_);

  Http::TestRequestHeaderMapImpl request_headers;
  Http::TestResponseHeaderMapImpl response_headers{{"respfrom", "unittest"}};

  EXPECT_EQ(false, config.shouldDowngrade(request_headers, response_headers));

  request_headers.addCopy("comefrom", "unittest");
  request_headers.addCopy("should_downgrade", "sxxxxd");

  EXPECT_EQ(true, config.shouldDowngrade(request_headers, response_headers));
}

TEST_F(StaticDowngradeConfigTest, ResponseDisableDowngrade) {
  initDowngradeRqx();
  initDowngradeRpx();

  StaticDowngradeConfig config(proto_config_, context_);

  Http::TestRequestHeaderMapImpl request_headers{{"comefrom", "unittest"},
                                                 {"should_downgrade", "sxxxxd"}};
  Http::TestResponseHeaderMapImpl response_headers;

  EXPECT_EQ(false, config.shouldDowngrade(request_headers, response_headers));

  response_headers.addCopy("respfrom", "unittest");

  EXPECT_EQ(true, config.shouldDowngrade(request_headers, response_headers));
}

TEST_F(StaticDowngradeConfigTest, DowngradeStatus) {
  initDowngradeRqx();
  initDowngradeRpx();

  StaticDowngradeConfig config1(proto_config_, context_);

  EXPECT_EQ(200, config1.static_http_status_);

  initHttpStatus();

  StaticDowngradeConfig config2(proto_config_, context_);

  EXPECT_EQ(429, config2.static_http_status_);
}

TEST_F(StaticDowngradeConfigTest, DowngradeStringBody) {
  initDowngradeRqx();
  initDowngradeRpx();
  initStringBody();
  EXPECT_CALL(context_, api()).WillOnce(ReturnRef(context_.api_));

  StaticDowngradeConfig config1(proto_config_, context_);

  EXPECT_EQ("body from unittest", config1.static_http_body_);
}

TEST_F(StaticDowngradeConfigTest, DowngradeFileBody) {
  initDowngradeRqx();
  initDowngradeRpx();
  initFileBody();

  EXPECT_CALL(context_, api()).WillOnce(ReturnRef(context_.api_));

  ON_CALL(context_.api_, fileSystem()).WillByDefault(ReturnRef(context_.api_.file_system_));
  ON_CALL(context_.api_.file_system_, fileExists("/tmp/downgrade.log")).WillByDefault(Return(true));
  ON_CALL(context_.api_.file_system_, fileSize("/tmp/downgrade.log")).WillByDefault(Return(6));
  ON_CALL(context_.api_.file_system_, fileReadToEnd("/tmp/downgrade.log"))
      .WillByDefault(Return("AAAAAA"));

  StaticDowngradeConfig config1(proto_config_, context_);

  EXPECT_EQ("AAAAAA", config1.static_http_body_);
}

class StaticDowngradeRouteConfigTest : public testing::Test {
public:
  void initRouteConfig() {
    ProtoDowngradeConfig primary_proto_config, specific_proto_config;

    envoy::config::route::v3::HeaderMatcher hm1, hm2;
    hm1.set_name("comefrom");
    hm1.set_exact_match("unittest");

    hm2.set_name("respfrom");
    hm2.set_exact_match("unittest");

    auto rqx = primary_proto_config.mutable_downgrade_rqx()->mutable_headers();
    rqx->Add(std::move(hm1));
    auto rpx = primary_proto_config.mutable_downgrade_rpx()->mutable_headers();
    rpx->Add(std::move(hm2));

    envoy::config::route::v3::HeaderMatcher hm3, hm4;
    hm3.set_name("comefrom");
    hm3.set_exact_match("unittest_specific");

    hm4.set_name("respfrom");
    hm4.set_exact_match("unittest_specific");

    auto rqx2 = specific_proto_config.mutable_downgrade_rqx()->mutable_headers();
    rqx2->Add(std::move(hm3));
    auto rpx2 = specific_proto_config.mutable_downgrade_rpx()->mutable_headers();
    rpx2->Add(std::move(hm4));

    *proto_config_.mutable_downgrade_rqx() = primary_proto_config.downgrade_rqx();
    *proto_config_.mutable_downgrade_rpx() = primary_proto_config.downgrade_rpx();

    proto_config_.mutable_specific_configs()->Add(std::move(specific_proto_config));
  }

  ProtoRouteConfig proto_config_;
  testing::StrictMock<Server::Configuration::MockServerFactoryContext> context_;
};

TEST_F(StaticDowngradeRouteConfigTest, NoMatchConfig) {
  initRouteConfig();
  StaticDowngradeRouteConfig config(proto_config_, context_);

  Http::TestRequestHeaderMapImpl request_headers{{"comefrom", "xxxxxxxx"}};
  Http::TestResponseHeaderMapImpl response_headers{{"respfrom", "yyyyyyyy"}};

  EXPECT_EQ(nullptr, config.downgradeConfig(request_headers, response_headers));
}

TEST_F(StaticDowngradeRouteConfigTest, MatchFisrtConfig) {
  initRouteConfig();
  StaticDowngradeRouteConfig config(proto_config_, context_);

  Http::TestRequestHeaderMapImpl request_headers{{"comefrom", "unittest"}};
  Http::TestResponseHeaderMapImpl response_headers{{"respfrom", "unittest"}};

  EXPECT_EQ(config.downgrade_configs_[0].get(),
            config.downgradeConfig(request_headers, response_headers));
}

TEST_F(StaticDowngradeRouteConfigTest, MatchSecondConfig) {
  initRouteConfig();
  StaticDowngradeRouteConfig config(proto_config_, context_);

  Http::TestRequestHeaderMapImpl request_headers{{"comefrom", "unittest_specific"}};
  Http::TestResponseHeaderMapImpl response_headers{{"respfrom", "unittest_specific"}};

  EXPECT_EQ(config.downgrade_configs_[1].get(),
            config.downgradeConfig(request_headers, response_headers));
}

class HttpStaticDowngradeFilterTest : public testing::Test {
public:
  void initDowngradeProtoConfig() {
    envoy::config::route::v3::HeaderMatcher hm1, hm2, hm3;
    hm1.set_name("comefrom");
    hm1.set_exact_match("unittest");

    hm2.set_name("should_downgrade");
    auto regex_match = hm2.mutable_safe_regex_match();
    regex_match->set_regex("s....d");
    regex_match->mutable_google_re2();

    hm3.set_name("respfrom");
    hm3.set_exact_match("unittest");

    auto rqx = proto_config_.mutable_downgrade_rqx()->mutable_headers();
    rqx->Add(std::move(hm1));
    rqx->Add(std::move(hm2));

    auto rpx = proto_config_.mutable_downgrade_rpx()->mutable_headers();
    rpx->Add(std::move(hm3));

    // body
    proto_config_.mutable_static_response()->mutable_body()->set_inline_string(
        "body from unittest");

    // header
    envoy::config::core::v3::HeaderValue hv;
    hv.set_key("replacekey");
    hv.set_value("new");
    proto_config_.mutable_static_response()->mutable_headers()->Add(std::move(hv));
  }

  void initFilterCallback() {
    filter_.setDecoderFilterCallbacks(decode_filter_callbacks_);
    filter_.setEncoderFilterCallbacks(encode_filter_callbacks_);
  }
  Http::TestRequestHeaderMapImpl request_headers_{
      {"content-type", "test"}, {":method", "GET"},       {":authority", "www.proxy.com"},
      {":path", "/path"},       {"comefrom", "unittest"}, {"should_downgrade", "should"}};

  Http::TestResponseHeaderMapImpl response_headers_{{"content-type", "test"},
                                                    {"respfrom", "unittest"},
                                                    {":status", "429"},
                                                    {"replacekey", "old"}};

  HttpStaticDowngradeFilter filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decode_filter_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encode_filter_callbacks_;
  ProtoRouteConfig proto_config_;
  testing::StrictMock<Server::Configuration::MockServerFactoryContext> context_;
};

TEST_F(HttpStaticDowngradeFilterTest, NoHttpRoute) {
  initDowngradeProtoConfig();
  initFilterCallback();
  ON_CALL(encode_filter_callbacks_, route()).WillByDefault(Return(nullptr));

  filter_.decodeHeaders(request_headers_, true);
  filter_.encodeHeaders(response_headers_, true);

  // 无任何操作
  EXPECT_EQ("429", response_headers_.get_(":status"));
  EXPECT_EQ("old", response_headers_.get_("replacekey"));

  Buffer::OwnedImpl body("old body");
  // 即使end_stream被设置被false也会continue
  auto result = filter_.encodeData(body, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, result);
}

TEST_F(HttpStaticDowngradeFilterTest, NoFilterConfig) {
  initDowngradeProtoConfig();
  initFilterCallback();

  ON_CALL(*encode_filter_callbacks_.route_, perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(nullptr));

  ON_CALL(encode_filter_callbacks_.route_->route_entry_,
          perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(nullptr));

  ON_CALL(encode_filter_callbacks_.route_->route_entry_.virtual_host_,
          perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(nullptr));

  filter_.decodeHeaders(request_headers_, true);
  filter_.encodeHeaders(response_headers_, true);

  // 无任何操作
  EXPECT_EQ("429", response_headers_.get_(":status"));
  EXPECT_EQ("old", response_headers_.get_("replacekey"));

  Buffer::OwnedImpl body("old body");
  // 即使end_stream被设置被false也会continue
  auto result = filter_.encodeData(body, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, result);
  EXPECT_EQ("old body", body.toString());
}

TEST_F(HttpStaticDowngradeFilterTest, NoDowngrade) {
  initDowngradeProtoConfig();
  initFilterCallback();

  envoy::config::route::v3::HeaderMatcher hm1;
  hm1.set_name(":status");
  hm1.set_exact_match("503");
  proto_config_.mutable_downgrade_rpx()->mutable_headers()->Add(std::move(hm1));

  EXPECT_CALL(context_, api()).WillOnce(ReturnRef(context_.api_));

  StaticDowngradeRouteConfig config(proto_config_, context_);

  ON_CALL(*encode_filter_callbacks_.route_, perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  ON_CALL(encode_filter_callbacks_.route_->route_entry_,
          perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  filter_.decodeHeaders(request_headers_, true);
  filter_.encodeHeaders(response_headers_, true);

  // 无任何操作
  EXPECT_EQ("429", response_headers_.get_(":status"));
  EXPECT_EQ("old", response_headers_.get_("replacekey"));

  Buffer::OwnedImpl body("old body");
  // 即使end_stream被设置被false也会continue
  auto result = filter_.encodeData(body, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, result);
  EXPECT_EQ("old body", body.toString());
}

TEST_F(HttpStaticDowngradeFilterTest, DowngradeComplete) {
  initDowngradeProtoConfig();
  initFilterCallback();

  EXPECT_CALL(context_, api()).WillOnce(ReturnRef(context_.api_));

  StaticDowngradeRouteConfig config(proto_config_, context_);

  ON_CALL(*encode_filter_callbacks_.route_, perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  ON_CALL(encode_filter_callbacks_.route_->route_entry_,
          perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  filter_.decodeHeaders(request_headers_, true);
  filter_.encodeHeaders(response_headers_, true);

  EXPECT_EQ("200", response_headers_.get_(":status"));
  EXPECT_EQ("new", response_headers_.get_("replacekey"));

  Buffer::OwnedImpl body("old body");
  // 即使end_stream被设置被false也会continue
  auto result = filter_.encodeData(body, false);
  EXPECT_EQ(Http::FilterDataStatus::Continue, result);
  EXPECT_EQ("old body", body.toString());
}

TEST_F(HttpStaticDowngradeFilterTest, ReplaceBody) {
  initDowngradeProtoConfig();
  initFilterCallback();
  EXPECT_CALL(context_, api()).WillOnce(ReturnRef(context_.api_));
  StaticDowngradeRouteConfig config(proto_config_, context_);

  ON_CALL(*encode_filter_callbacks_.route_, perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  ON_CALL(encode_filter_callbacks_.route_->route_entry_,
          perFilterConfig(HttpFilterNames::get().StaticDowngrade))
      .WillByDefault(Return(&config));

  filter_.decodeHeaders(request_headers_, true);
  filter_.encodeHeaders(response_headers_, false);

  EXPECT_EQ("200", response_headers_.get_(":status"));
  EXPECT_EQ("new", response_headers_.get_("replacekey"));

  Buffer::OwnedImpl body("old body");

  auto result = filter_.encodeData(body, false);
  EXPECT_EQ(Http::FilterDataStatus::StopIterationNoBuffer, result);
  EXPECT_EQ(0, body.length());

  EXPECT_CALL(encode_filter_callbacks_, addEncodedData(_, false));

  result = filter_.encodeData(body, true);
  EXPECT_EQ(Http::FilterDataStatus::Continue, result);
}

} // namespace StaticDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
