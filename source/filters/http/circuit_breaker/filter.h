#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/http/filter.h"
#include "envoy/runtime/runtime.h"
#include "envoy/server/filter_config.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "source/common/common/logger.h"
#include "source/common/protobuf/utility.h"
#include "source/common/stats/rolling_number.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "source/filters/http/circuit_breaker/common.h"
#include "source/filters/http/circuit_breaker/impl.h"

#include "api/proxy/filters/http/circuit_breaker/v2/circuit_breaker.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

class CircuitBreakerFilterConfig : Logger::Loggable<Logger::Id::filter> {
public:
  CircuitBreakerFilterConfig(CircuitBreakerManager& circuit_breaker_manager,
                             TimeSource& time_source, Runtime::Loader& runtime)
      : circuit_breaker_manager_(circuit_breaker_manager), time_source_(time_source),
        runtime_(runtime) {}

  CircuitBreakerManager& getCircuitBreakerManager() { return circuit_breaker_manager_; }

  TimeSource& timeSource() { return time_source_; }
  Runtime::Loader& runtime() { return runtime_; }

private:
  CircuitBreakerManager& circuit_breaker_manager_;
  TimeSource& time_source_;
  Runtime::Loader& runtime_;
};

class CircuitBreakerRouteSpecificFilterConfig : public Router::RouteSpecificFilterConfig {
public:
  CircuitBreakerRouteSpecificFilterConfig(
      const CircuitBreakerRouteSpecificFilterConfigProto& proto_config, std::string& uuid)
      : uuid_(uuid), settings_(CircuitBreakerSettings(proto_config)) {}

  const std::string& uuid() const { return uuid_; }
  const CircuitBreakerSettings& settings() const { return settings_; }

private:
  const std::string uuid_;
  const CircuitBreakerSettings settings_;
};

using CircuitBreakerFilterConfigSharedPtr = std::shared_ptr<CircuitBreakerFilterConfig>;
using CircuitBreakerRouteSpecificFilterConfigSharedPtr =
    std::shared_ptr<CircuitBreakerRouteSpecificFilterConfig>;
using CircuitBreakerFactoryInternalCb =
    std::function<CircuitBreakerPtr(const CircuitBreakerRouteSpecificFilterConfig*)>;

class CircuitBreakerFilter : public Envoy::Http::PassThroughFilter,
                             Logger::Loggable<Logger::Id::filter> {
public:
  CircuitBreakerFilter(CircuitBreakerFilterConfigSharedPtr config,
                       CircuitBreakerFactoryInternalCb wrapper);

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers,
                                          bool end_stream) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.circuitbreaker");
  }

private:
  void abortWithHTTPStatus(const CircuitBreakerSettings& settings);

private:
  CircuitBreakerFilterConfigSharedPtr config_;
  std::string route_uuid_;
  SystemTime start_time_;
  CircuitBreakerFactoryInternalCb circuit_breaker_factory_internal_cb_;
  bool local_replied_{};
  bool wait_body_{};
  const CircuitBreakerRouteSpecificFilterConfig* route_config_{};
};

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
