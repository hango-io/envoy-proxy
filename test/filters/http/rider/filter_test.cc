#include <memory>

#include "source/common/buffer/buffer_impl.h"
#include "source/common/http/message_impl.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/filters/http/rider/filter.h"

#include "test/mocks/access_log/mocks.h"
#include "test/mocks/api/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/cluster_info.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

using testing::_;
using testing::AtLeast;
using testing::Eq;
using testing::HasSubstr;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::StrEq;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {
namespace {

class TestFilter : public Proxy::HttpFilters::Rider::Filter {
public:
  using Proxy::HttpFilters::Rider::Filter::Filter;

  MOCK_METHOD(void, log, (spdlog::level::level_enum level, const char* message), (override));
};

class RiderFilterTest : public testing::Test {
public:
  using RiderRouteSpecificFilterConfigSharedPtr =
      std::shared_ptr<Proxy::HttpFilters::Rider::RouteConfig>;

  RiderFilterTest() {
    EXPECT_CALL(decoder_callbacks_, addDecodedData(_, _))
        .Times(AtLeast(0))
        .WillRepeatedly(Invoke([this](Buffer::Instance& data, bool) {
          if (decoder_callbacks_.buffer_ == nullptr) {
            decoder_callbacks_.buffer_ = std::make_unique<Buffer::OwnedImpl>();
          }
          decoder_callbacks_.buffer_->move(data);
        }));

    EXPECT_CALL(encoder_callbacks_, addEncodedData(_, _))
        .Times(AtLeast(0))
        .WillRepeatedly(Invoke([this](Buffer::Instance& data, bool) {
          if (encoder_callbacks_.buffer_ == nullptr) {
            encoder_callbacks_.buffer_ = std::make_unique<Buffer::OwnedImpl>();
          }
          encoder_callbacks_.buffer_->move(data);
        }));
  }

  void SetUpTest(bool allow_no_route = true) { SetUpTest(filter_config_, allow_no_route); }

  void SetUpTest(const std::string& yaml, bool allow_no_route = true) {
    cluster_manager_.initializeThreadLocalClusters({"cluster"});

    proxy::filters::http::rider::v3alpha1::FilterConfig proto_config{};
    TestUtility::loadFromYaml(yaml, proto_config);

    proto_config.mutable_plugin()->set_allow_no_route(allow_no_route);

    std::string testlib_path =
        TestEnvironment::runfilesPath("test/filters/http/rider/testlib.lua", "envoy_proxy") + ";";
    proto_config.mutable_plugin()->mutable_vm_config()->set_package_path(testlib_path);

    config_.reset(new Proxy::HttpFilters::Rider::ConfigImpl(
        proto_config, cluster_manager_, log_manager_, api_.timeSource(), tls_, api_));

    filter_ = std::make_unique<TestFilter>(*config_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);

    rider_filter_ = std::make_unique<Filter>(*config_);
    rider_filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    rider_filter_->setEncoderFilterCallbacks(encoder_callbacks_);
  }

  void setFilterConfigPerRoute(const std::string& yaml) {
    proxy::filters::http::rider::v3alpha1::RouteFilterConfig proto_config;
    TestUtility::loadFromYaml(yaml, proto_config);

    route_config_ = std::make_shared<Proxy::HttpFilters::Rider::RouteConfig>(proto_config);

    ON_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(Filter::name()))
        .WillByDefault(Return(route_config_.get()));
    ON_CALL(*encoder_callbacks_.route_, mostSpecificPerFilterConfig(Filter::name()))
        .WillByDefault(Return(route_config_.get()));
  }

  const std::string filter_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            envoy.logInfo("on request :)")
          end
          function envoy_on_response()
            envoy.logInfo("on response :)")
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  const std::string decode_headers_error_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            nil_object(foo)
            envoy.logInfo("on request :)")
          end
          function envoy_on_response()
            envoy.logInfo("on response :)")
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  const std::string encode_headers_error_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            envoy.logInfo("on request :)")
          end
          function envoy_on_response()
            nil_object(foo)
            envoy.logInfo("on response :)")
          end
          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  const std::string get_header_map_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            local headers = envoy.req.get_headers()
            envoy.logInfo(envoy.concat_headers(headers))
          end

          function envoy_on_response()
            local headers = envoy.resp.get_headers()
            envoy.logInfo(envoy.concat_headers(headers))
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  const std::string decode_body_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            local body = envoy.req.get_body()
            envoy.logInfo(body:getBytes(0, body:length()))
          end
          function envoy_on_response()
            local body = envoy.resp.get_body()
            envoy.logInfo(body:getBytes(0, body:length()))
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Envoy::AccessLog::MockAccessLogManager> log_manager_;
  NiceMock<Envoy::Api::MockApi> api_;
  // tls_ must be placed before filter_ to ensure lifecycle of thread local PluginHandle
  // is longger than filter_.
  NiceMock<ThreadLocal::MockInstance> tls_;
  std::shared_ptr<Proxy::HttpFilters::Rider::Config> config_;
  std::unique_ptr<TestFilter> filter_;
  std::unique_ptr<Filter> rider_filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  RiderRouteSpecificFilterConfigSharedPtr route_config_;
};

TEST_F(RiderFilterTest, LogOK) {
  SetUpTest();

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("on request :)")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("on response :)")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, DecodeHeadersHandleScriptErrorAndSkip) {
  SetUpTest(decode_headers_error_config_);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::err, HasSubstr("runtime error:")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  // When error occur, rest of the script will be skipped.
  EXPECT_CALL(*filter_, log).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, EncodeHeadersHandleScriptErrorAndSkip) {
  SetUpTest(encode_headers_error_config_);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("on request :)")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::err, HasSubstr("runtime error:")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, GetHeaderMap) {
  SetUpTest(get_header_map_config_);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}, {"connection", "close"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info,
                            StrEq(":authority=www.foo.com,:method=GET,:path=/users/123")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_CALL(*filter_,
              log(spdlog::level::level_enum::info, StrEq(":status=200,connection=close")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, GetHeaderMapValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.req.get_header(":path"))
          end
          function envoy_on_response()
              envoy.logInfo(envoy.resp.get_header(":status"))
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("/users/123")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("200")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, SetHeaderMapValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.req.set_header(":path", "/abc")
              envoy.logInfo(envoy.req.get_header(":path"))
          end

          function envoy_on_response()
              envoy.resp.set_header(":status", "400")
              envoy.logInfo(envoy.resp.get_header(":status"))
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("/abc")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("400")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, RemoveHeaderMapValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              local size_before = envoy.req.get_header_map_size()
              envoy.req.remove_header("connection")
              local size_after = envoy.req.get_header_map_size()
              envoy.logInfo(size_before - size_after)
          end

          function envoy_on_response()
              local size_before = envoy.resp.get_header_map_size()
              envoy.resp.remove_header(":status")
              local size_after = envoy.resp.get_header_map_size()
              envoy.logInfo(size_before - size_after)
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.foo.com"},
                                                 {":path", "/users/123"},
                                                 {"connection", "keepalive"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("1")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("1")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, GetBodyResumeAndContinue) {
  SetUpTest(decode_body_config_);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  Buffer::OwnedImpl data_chunk_1("hello");
  Buffer::OwnedImpl data_chunk_2("world");
  Buffer::OwnedImpl data_chunk_3("aaaaa");
  Buffer::OwnedImpl data_chunk_4("bbbbb");

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, false));

  EXPECT_CALL(decoder_callbacks_, decodingBuffer()).Times(AtLeast(1));

  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer,
            filter_->decodeData(data_chunk_1, false));
  decoder_callbacks_.addDecodedData(data_chunk_1, false);

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("helloworld")));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data_chunk_2, true));

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->encodeHeaders(response_headers, false));

  EXPECT_EQ(Http::FilterDataStatus::StopIterationAndBuffer,
            filter_->encodeData(data_chunk_3, false));
  encoder_callbacks_.addEncodedData(data_chunk_3, false);

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("aaaaabbbbb")));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_chunk_4, true));
}

TEST_F(RiderFilterTest, GetRouteConfig) {
  const std::string yaml_route_config = R"EOF(
  plugins:
  - name: test
    config:
      foo: bar
  )EOF";

  const std::string get_route_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            local hash = envoy.get_route_config_hash()

            local route_config = envoy.get_route_configuration()
            envoy.logInfo(tostring(hash).."||"..route_config)
          end

          function envoy_on_response()
            local hash = envoy.get_route_config_hash()
            local route_config = envoy.get_route_configuration()
            envoy.logInfo(tostring(hash).."||"..route_config)
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  setFilterConfigPerRoute(yaml_route_config);
  SetUpTest(get_route_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}, {"connection", "close"}};

  EXPECT_CALL(
      *filter_,
      log(spdlog::level::level_enum::info,
          StrEq("10889318279206509745ULL||{\"name\":\"test\",\"config\":{\"foo\":\"bar\"}}")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  EXPECT_CALL(
      *filter_,
      log(spdlog::level::level_enum::info,
          StrEq("10889318279206509745ULL||{\"name\":\"test\",\"config\":{\"foo\":\"bar\"}}")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, Respond) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            envoy.respond({[":status"] = 403,}, "Forbidden")
            envoy.logInfo("request :)")
          end

          function envoy_on_response()
            envoy.logInfo("response :)")
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";

  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl expected_headers{{":status", "403"}};

  EXPECT_CALL(*filter_, log).Times(0);
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_);
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, HttpCallImmediateResponse) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            local headers, body = envoy.httpCall(
              "cluster",
              {
                [":method"] = "GET",
                [":path"] = "/",
                [":authority"] = "foo"
              },
              nil,
              5000)
            envoy.respond({[":status"] = 403,}, "Forbidden")
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";

  InSequence s;
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::MockAsyncClientRequest request(&cluster_manager_.thread_local_cluster_.async_client_);
  Http::AsyncClient::Callbacks* callbacks;
  EXPECT_CALL(cluster_manager_, getThreadLocalCluster(Eq("cluster")));
  // EXPECT_CALL(cluster_manager_, httpAsyncClientForCluster("cluster"));
  EXPECT_CALL(cluster_manager_.thread_local_cluster_.async_client_, send_(_, _, _))
      .WillOnce(
          Invoke([&](Http::RequestMessagePtr& message, Http::AsyncClient::Callbacks& cb,
                     const Http::AsyncClient::RequestOptions&) -> Http::AsyncClient::Request* {
            const Http::TestRequestHeaderMapImpl expected_headers{
                {":path", "/"}, {":method", "GET"}, {":authority", "foo"}};
            EXPECT_THAT(&message->headers(), HeaderMapEqualIgnoreOrder(&expected_headers));
            callbacks = &cb;
            return &request;
          }));

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, false));

  Http::ResponseMessagePtr response_message(new Http::ResponseMessageImpl(
      Http::ResponseHeaderMapPtr{new Http::TestResponseHeaderMapImpl{{":status", "200"}}}));
  Http::TestResponseHeaderMapImpl expected_headers{{":status", "403"}, {"content-length", "9"}};
  // TODO(Tong Cai): verify body.
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(HeaderMapEqualRef(&expected_headers), false));
  callbacks->onFailure(request, Http::AsyncClient::FailureReason::Reset);
}

TEST_F(RiderFilterTest, GetQueryParametersValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.concat_headers(envoy.req.get_query_parameters()))
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{{":method", "GET"},
                                                 {":authority", "www.foo.com"},
                                                 {":path", "/users/123?test1=test2&test3=test4"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("test1=test2,test3=test4")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetUpStreamHostValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.streaminfo.upstream_host())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  std::shared_ptr<Network::MockResolvedAddress> address =
      std::make_shared<Network::MockResolvedAddress>("192.168.1.1:443", "192.168.1.1:443");
  std::shared_ptr<Upstream::MockHostDescription> hd =
      std::make_shared<Upstream::MockHostDescription>();
  testing::NiceMock<StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info));
  stream_info.upstreamInfo()->setUpstreamHost(hd);
  EXPECT_CALL(*hd, address()).WillOnce(Return(address));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("192.168.1.1:443")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetUpStreamClusterValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.streaminfo.upstream_cluster())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  testing::NiceMock<Upstream::MockClusterInfo> cluster;
  std::shared_ptr<Upstream::MockHostDescription> hd =
      std::make_shared<Upstream::MockHostDescription>();
  testing::NiceMock<StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info));
  stream_info.upstreamInfo()->setUpstreamHost(hd);
  EXPECT_CALL(*hd, cluster()).WillOnce(ReturnRef(cluster));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("fake_cluster")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetDownStreamLocalAddrValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.streaminfo.downstream_local_address())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  testing::NiceMock<StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("127.0.0.2:0")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetDownStreamRemoteAddrValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.streaminfo.downstream_remote_address())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  testing::NiceMock<StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("127.0.0.1:0")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetMetadataValue) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.req.get_metadata("key", "envoy.test"))
              envoy.logInfo(envoy.req.get_metadata("number", "envoy.numbertest"))
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  envoy::config::core::v3::Metadata metadata;
  (*metadata.mutable_filter_metadata())["envoy.test"] = MessageUtil::keyValueStruct("key", "value");

  ProtobufWkt::Struct number_struct;
  ProtobufWkt::Value val;
  auto& fields_map = *number_struct.mutable_fields();
  val.set_number_value(222);
  fields_map["number"] = val;

  (*metadata.mutable_filter_metadata())["envoy.numbertest"] = number_struct;

  EXPECT_CALL(*decoder_callbacks_.route_, metadata()).WillRepeatedly(ReturnRef(metadata));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("value")));
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("222")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, LogLevel) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logTrace("trace")
              envoy.logDebug("debug")
              envoy.logInfo("info")
              envoy.logWarn("warn")
              envoy.logErr("error")
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            rider_filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetStartTime) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.streaminfo.start_time())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  testing::NiceMock<StreamInfo::MockStreamInfo> stream_info;
  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillOnce(ReturnRef(stream_info));

  const auto time = std::chrono::milliseconds(1234567891234);
  EXPECT_CALL(stream_info, startTime()).WillOnce(Return(SystemTime(time)));

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("1234567891234")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetCurrentTimeMilliseconds) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo(envoy.nowms())
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            rider_filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, FilteLog) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    log_file:
      path: ./test.log
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.filelog("test")
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            rider_filter_->decodeHeaders(request_headers, false));
}

TEST_F(RiderFilterTest, GetOrCreateSharedTable) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              local shared = envoy.stream.shared
              shared.message = "fff"
          end
          function envoy_on_response()
              local shared = envoy.stream.shared
              envoy.logInfo(shared.message)
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("fff")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));

  filter_->onDestroy();
}

TEST_F(RiderFilterTest, AllowNoRoute) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              envoy.logInfo("info")
          end
          function envoy_on_response()
              envoy.logInfo("info")
          end

          return {
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml_config, false);
  EXPECT_EQ(false, config_->allowNoRoute());

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  EXPECT_CALL(decoder_callbacks_, route()).WillOnce(Return(nullptr));

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("info"))).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
}

TEST_F(RiderFilterTest, DecodeDataHandleScriptErrorAndSkip) {

  const std::string decode_data_error_config_ = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
            local body = envoy.req.get_body()
            nil_object(foo)
            envoy.logInfo(body:getBytes(0, body:length()))
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(decode_data_error_config_);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "200"}};

  Buffer::OwnedImpl data_chunk_1("hello");

  EXPECT_CALL(*filter_, log).Times(0);

  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers, false));

  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data_chunk_1, true));
}

TEST_F(RiderFilterTest, BadArguement) {
  const std::string yaml_config = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_request()
              if (envoy.req.get_header("") == nil) then
                envoy.logInfo("BadArguement")
              end
              if (envoy.req.get_header("test") == nil) then
                envoy.logInfo("NotFound")
              end
              if (envoy.req.get_metadata("key", "fake") == nil) then
                envoy.logInfo("NotFound")
              end
              if (envoy.req.get_metadata("fake", "envoy.test") == nil) then
                envoy.logInfo("NotFound")
              end
              envoy.req.set_header("key", "")
          end

          return {
            on_request = envoy_on_request,
          }
  )EOF";
  SetUpTest(yaml_config);

  Http::TestRequestHeaderMapImpl request_headers{
      {":method", "GET"}, {":authority", "www.foo.com"}, {":path", "/users/123"}};

  envoy::config::core::v3::Metadata metadata;
  (*metadata.mutable_filter_metadata())["envoy.test"] = MessageUtil::keyValueStruct("key", "value");

  EXPECT_CALL(*decoder_callbacks_.route_, metadata()).WillRepeatedly(ReturnRef(metadata));

  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("BadArguement")));
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::info, StrEq("NotFound"))).Times(3);
  EXPECT_CALL(*filter_, log(spdlog::level::level_enum::err, HasSubstr("runtime error:")));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));
}

} // namespace
} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
