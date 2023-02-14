#include "source/filters/http/traffic_mark/config.h"

#include "test/mocks/server/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {
namespace {

TEST(TrafficMarkFilterConfigTest, ValidateFail) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  HttpTrafficMarkFilterFactory factory;
  EXPECT_THROW(factory.createFilterFactoryFromProto(
                   proxy::filters::http::traffic_mark::v2::ProtoCommonConfig(), "stats", context),
               ProtoValidationException);
}

TEST(TrafficMarkFilterConfigTest, TrafficMarkCorrectProto) {
  const std::string yaml = R"EOF(
  header_key: color
  match_key: qz_color
  all_colors_key: all_colors
  )EOF";

  proxy::filters::http::traffic_mark::v2::ProtoCommonConfig proto_config{};
  TestUtility::loadFromYamlAndValidate(yaml, proto_config);

  NiceMock<Server::Configuration::MockFactoryContext> context;
  HttpTrafficMarkFilterFactory factory;
  Http::FilterFactoryCb cb = factory.createFilterFactoryFromProto(proto_config, "stats", context);
  Http::MockFilterChainFactoryCallbacks filter_callback;
  EXPECT_CALL(filter_callback, addStreamDecoderFilter(_));
  cb(filter_callback);
}

} // namespace
} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
