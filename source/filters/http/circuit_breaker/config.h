#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/circuit_breaker/filter.h"

#include "api/proxy/filters/http/circuit_breaker/v2/circuit_breaker.pb.h"
#include "api/proxy/filters/http/circuit_breaker/v2/circuit_breaker.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

class CircuitBreakerFilterConfigFactory
    : public Extensions::HttpFilters::Common::FactoryBase<
          proxy::filters::http::circuit_breaker::v2::CircuitBreaker,
          proxy::filters::http::circuit_breaker::v2::CircuitBreakerPerRoute> {
public:
  CircuitBreakerFilterConfigFactory() : FactoryBase(CircuitBreakerFilter::name()) {}

private:
  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const proxy::filters::http::circuit_breaker::v2::CircuitBreaker& proto_config,
      const std::string& stats_prefix, Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr createRouteSpecificFilterConfigTyped(
      const proxy::filters::http::circuit_breaker::v2::CircuitBreakerPerRoute& proto_config,
      Server::Configuration::ServerFactoryContext& context,
      ProtobufMessage::ValidationVisitor&) override;
};

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
