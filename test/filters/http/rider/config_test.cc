#include "source/filters/http/rider/config.h"

#include "test/mocks/server/mocks.h"
#include "test/test_common/environment.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {
namespace {

class RiderFilterConfigTest : public testing::Test {
public:
  void SetUpTest(const std::string& yaml, bool allow_no_route = true) {
    TestUtility::loadFromYamlAndValidate(yaml, proto_config_);

    proto_config_.mutable_plugin()->set_allow_no_route(allow_no_route);

    std::string testlib_path =
        TestEnvironment::runfilesPath("test/filters/http/rider/testlib.lua", "envoy_netease") + ";";
    proto_config_.mutable_plugin()->mutable_vm_config()->set_package_path(testlib_path);
  }

  proxy::filters::http::rider::v3alpha1::FilterConfig proto_config_;
  NiceMock<Server::Configuration::MockFactoryContext> context_;
  Proxy::HttpFilters::Rider::RiderFilterConfigFactory factory_;
};

TEST_F(RiderFilterConfigTest, CorrectProto) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          function envoy_on_configure()
          end
          function envoy_on_request()
          end
          function envoy_on_response()
          end
          return {
            on_configure = envoy_on_configure,
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml);

  NiceMock<Server::Configuration::MockFactoryContext> context_;
  Proxy::HttpFilters::Rider::RiderFilterConfigFactory factory_;

  Http::FilterFactoryCb cb =
      factory_.createFilterFactoryFromProto(proto_config_, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  cb(filter_callback);
}

TEST_F(RiderFilterConfigTest, LoadEmpty) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          return {}
  )EOF";
  SetUpTest(yaml);

  Http::FilterFactoryCb cb =
      factory_.createFilterFactoryFromProto(proto_config_, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  cb(filter_callback);
}

TEST_F(RiderFilterConfigTest, LoadScriptError) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          error("some error")
  )EOF";
  SetUpTest(yaml);

  EXPECT_THROW_WITH_REGEX(factory_.createFilterFactoryFromProto(proto_config_, "stats", context_),
                          Proxy::HttpFilters::Rider::LuaException, "load lua script error: 1");
}

TEST_F(RiderFilterConfigTest, LoadScriptErrorBadCode) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          if foo
  )EOF";
  SetUpTest(yaml);

  EXPECT_THROW_WITH_REGEX(factory_.createFilterFactoryFromProto(proto_config_, "stats", context_),
                          Proxy::HttpFilters::Rider::LuaException, "load lua script error: 1");
}

TEST_F(RiderFilterConfigTest, LoadScriptErrorNoTable) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local ffi = require("ffi")
  )EOF";
  SetUpTest(yaml);

  EXPECT_THROW_WITH_REGEX(factory_.createFilterFactoryFromProto(proto_config_, "stats", context_),
                          Proxy::HttpFilters::Rider::LuaException,
                          "expect lua script to return a table");
}

TEST_F(RiderFilterConfigTest, OnConfigureSuccess) {
  auto testlib =
      TestEnvironment::runfilesPath("test/filters/http/rider/testlib.lua", "envoy_netease") + ";";

  const std::string yaml = R"EOF(
  plugin:
    name: test
    code:
      local:
        inline_string: |
          local testlib = require("testlib")
          function envoy_on_configure()
            envoy.logInfo("on configure inside plugin")
          end
          function envoy_on_request()
          end
          function envoy_on_response()
          end
          return {
            on_configure = envoy_on_configure,
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
    )EOF";
  SetUpTest(yaml);

  Http::FilterFactoryCb cb =
      factory_.createFilterFactoryFromProto(proto_config_, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  cb(filter_callback);
}

TEST_F(RiderFilterConfigTest, OnConfigureGetConfiguration) {
  const std::string yaml = R"EOF(
  plugin:
    name: test
    config:
      foo: bar
    code:
      local:
        inline_string: |
          local testlib = require("testlib")

          function envoy_on_configure()
            local configuration = envoy.get_configuration()
            if "{\"foo\":\"bar\"}" ~= configuration then
              error("got wrong configuration: "..configuration)
            end
            envoy.logInfo(configuration);
          end
          function envoy_on_request()
          end
          function envoy_on_response()
          end
          return {
            on_configure = envoy_on_configure,
            on_request = envoy_on_request,
            on_response = envoy_on_response,
          }
  )EOF";
  SetUpTest(yaml);

  Http::FilterFactoryCb cb =
      factory_.createFilterFactoryFromProto(proto_config_, "stats", context_);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamFilter(_));
  cb(filter_callback);
}

} // namespace
} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
