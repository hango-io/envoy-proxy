#include "source/common/protobuf/utility.h"
#include "source/filters/http/circuit_breaker/config.h"
#include "source/filters/http/circuit_breaker/filter.h"

#include "test/mocks/server/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

TEST(CircuitBreakerFilterConfigFactoryTest, RouteSpecificConfig) {
  CircuitBreakerRouteSpecificFilterConfigProto proto_config;
  proto_config.mutable_break_duration()->set_seconds(1);
  proto_config.mutable_response()->set_http_status(419);
  proto_config.mutable_error_percent_threshold()->set_value(50);
  proto_config.set_min_request_amount(2);
  proto_config.mutable_average_response_time()->set_nanos(1000 * 1000 * 500);
  proto_config.set_consecutive_slow_requests(3);

  NiceMock<Server::Configuration::MockFactoryContext> context;
  CircuitBreakerFilterConfigFactory factory;

  auto route_config =
      factory.createRouteSpecificFilterConfig(proto_config, context.server_factory_context_,
                                              Envoy::ProtobufMessage::getNullValidationVisitor());
  EXPECT_TRUE(route_config.get());

  auto p = dynamic_cast<const CircuitBreakerRouteSpecificFilterConfig*>(route_config.get());
  auto settings = p->settings();

  EXPECT_EQ(std::chrono::milliseconds(PROTOBUF_GET_MS_REQUIRED(proto_config, break_duration)),
            settings.breakDuration());
  EXPECT_EQ(proto_config.response().http_status(), settings.httpStatus());
  EXPECT_EQ(proto_config.min_request_amount(), settings.minRequestAmount());
  EXPECT_EQ(proto_config.error_percent_threshold().value(),
            settings.errorPercentThreshold().value());
  EXPECT_EQ(proto_config.consecutive_slow_requests(), settings.consecutiveSlowRequests());
  EXPECT_EQ(
      std::chrono::milliseconds(PROTOBUF_GET_MS_REQUIRED(proto_config, average_response_time)),
      settings.averageResponseTimeThreshold().value());
}

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
