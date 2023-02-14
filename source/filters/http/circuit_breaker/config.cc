#include "source/filters/http/circuit_breaker/config.h"

#include "envoy/registry/registry.h"

#include "source/filters/http/circuit_breaker/impl.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

Http::FilterFactoryCb CircuitBreakerFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const proxy::filters::http::circuit_breaker::v2::CircuitBreaker&, const std::string&,
    Server::Configuration::FactoryContext& context) {
  CircuitBreakerManagerSharedPtr manager = std::make_shared<CircuitBreakerManagerImpl>();
  auto config = std::make_shared<CircuitBreakerFilterConfig>(*manager, context.timeSource(),
                                                             context.runtime());

  return [config, manager](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<CircuitBreakerFilter>(
        config,
        [config](const CircuitBreakerRouteSpecificFilterConfig* route_config) -> CircuitBreakerPtr {
          return std::make_unique<CircuitBreakerImpl>(route_config->settings(),
                                                      route_config->uuid(), config->timeSource());
        }));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
CircuitBreakerFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const proxy::filters::http::circuit_breaker::v2::CircuitBreakerPerRoute& proto_config,
    Server::Configuration::ServerFactoryContext& context, ProtobufMessage::ValidationVisitor&) {
  if (!proto_config.has_average_response_time() && !proto_config.has_error_percent_threshold()) {
    throw EnvoyException(
        "at least one of average_response_time and error_percent_threshold must be set");
  }

  if (proto_config.has_error_percent_threshold()) {
    if (proto_config.min_request_amount() == 0) {
      throw EnvoyException(
          "min_request_amount must be greater than 0 when error_percent_threshold is set");
    }
  }

  if (proto_config.has_average_response_time() && proto_config.consecutive_slow_requests() == 0) {
    throw EnvoyException(
        "consecutive_slow_requests must be greater than 0 when consecutive_slow_requests is set");
  }

  auto uuid = context.api().randomGenerator().uuid();
  return std::make_shared<const CircuitBreakerRouteSpecificFilterConfig>(proto_config, uuid);
}

REGISTER_FACTORY(CircuitBreakerFilterConfigFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);
} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
