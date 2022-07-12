#include "source/filters/http/circuit_breaker/filter.h"

#include "envoy/stats/scope.h"

#include "source/common/http/codes.h"
#include "source/common/http/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

CircuitBreakerFilter::CircuitBreakerFilter(CircuitBreakerFilterConfigSharedPtr config,
                                           CircuitBreakerFactoryInternalCb cb)
    : config_(config), circuit_breaker_factory_internal_cb_(cb) {}

Http::FilterHeadersStatus CircuitBreakerFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                              bool end_stream) {
  Router::RouteConstSharedPtr route = decoder_callbacks_->route();
  const Router::RouteEntry* route_entry;
  if (!route || !(route_entry = route->routeEntry())) {
    return Http::FilterHeadersStatus::Continue;
  }

  const CircuitBreakerRouteSpecificFilterConfig* per_route_config =
      Http::Utility::resolveMostSpecificPerFilterConfig<CircuitBreakerRouteSpecificFilterConfig>(
          name(), route);

  if (per_route_config == nullptr) {
    ENVOY_LOG(debug, "{}", "circuitbreaker route config not found");
    return Http::FilterHeadersStatus::Continue;
  }

  start_time_ = config_->timeSource().systemTime();

  if (route_uuid_.empty()) {
    route_uuid_ = per_route_config->uuid();
  }

  auto& circuit_breaker_manager = config_->getCircuitBreakerManager();
  bool is_open =
      circuit_breaker_manager.isOpen(route_uuid_, [this, per_route_config]() -> CircuitBreakerPtr {
        return circuit_breaker_factory_internal_cb_(per_route_config);
      });

  if (is_open) {
    wait_body_ = per_route_config->settings().waitBody() &&
                 !(end_stream || Http::Utility::isWebSocketUpgradeRequest(headers) ||
                   Http::Utility::isH2UpgradeRequest(headers));

    if (wait_body_) {
      ENVOY_STREAM_LOG(debug, "circuitbreaker filter is buffering the request",
                       *decoder_callbacks_);
      route_config_ = per_route_config;
      // Buffer body which can be used in downgrade.
      return Http::FilterHeadersStatus::StopIteration;
    }

    local_replied_ = true;
    abortWithHTTPStatus(per_route_config->settings());
    return Http::FilterHeadersStatus::StopIteration;
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus CircuitBreakerFilter::decodeData(Buffer::Instance&, bool end_stream) {
  if (!wait_body_) {
    return Http::FilterDataStatus::Continue;
  }

  ASSERT(!local_replied_);
  ASSERT(route_config_);

  if (end_stream) {
    local_replied_ = true;
    abortWithHTTPStatus(route_config_->settings());
  }

  return Http::FilterDataStatus::StopIterationNoBuffer;
}

void CircuitBreakerFilter::abortWithHTTPStatus(const CircuitBreakerSettings& settings) {
  const static Http::LowerCaseString& circuit_breaking_header =
      Http::LowerCaseString("x-envoy-circuitbreaking");
  const static std::string& circuit_breaking_header_value = "true";

  std::string body = settings.httpBody();
  body = body.empty() ? "circuit breaker filter abort" : body;
  auto& headers = settings.httpHeaders();

  std::function<void(Http::HeaderMap & headerMap)> modify_headers =
      [&headers](Http::HeaderMap& headerMap) -> void {
    for (auto& p : headers) {
      headerMap.addCopy(Http::LowerCaseString(p.first), p.second);
    }
    headerMap.setReference(circuit_breaking_header, circuit_breaking_header_value);
  };

  decoder_callbacks_->sendLocalReply(static_cast<Http::Code>(settings.httpStatus()), body,
                                     modify_headers, absl::nullopt, "");
}

Http::FilterHeadersStatus CircuitBreakerFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                              bool) {
  if (local_replied_) {
    return Http::FilterHeadersStatus::Continue;
  }

  Router::RouteConstSharedPtr route = decoder_callbacks_->route();
  const Router::RouteEntry* route_entry;
  if (!route || !(route_entry = route->routeEntry())) {
    return Http::FilterHeadersStatus::Continue;
  }

  const CircuitBreakerRouteSpecificFilterConfig* per_route_config =
      Http::Utility::resolveMostSpecificPerFilterConfig<CircuitBreakerRouteSpecificFilterConfig>(
          name(), route);

  if (per_route_config == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  bool error(false);
  std::chrono::milliseconds rt_milliseconds(0);

  const uint64_t status_code = Http::Utility::getResponseStatus(headers);
  if (Http::CodeUtility::is5xx(status_code)) {
    error = true;
  }

  if (auto upstream_info = decoder_callbacks_->streamInfo().upstreamInfo();
      upstream_info != nullptr &&
      per_route_config->settings().averageResponseTimeThreshold().has_value()) {
    auto firstUpstreamRxByteReceived =
        upstream_info->upstreamTiming().first_upstream_rx_byte_received_;
    auto firstUpstreamTxByteSent = upstream_info->upstreamTiming().first_upstream_tx_byte_sent_;
    if (firstUpstreamRxByteReceived.has_value() && firstUpstreamTxByteSent.has_value()) {
      ENVOY_LOG(debug, "firstUpstreamRxByteReceived {}, firstUpstreamTxByteSent {}",
                firstUpstreamRxByteReceived.value().time_since_epoch().count(),
                firstUpstreamTxByteSent.value().time_since_epoch().count());
      rt_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
          firstUpstreamRxByteReceived.value() - firstUpstreamTxByteSent.value());
    } else {
      if (error) {
        rt_milliseconds = per_route_config->settings().averageResponseTimeThreshold().value() +
                          std::chrono::milliseconds(1);
      } else {
        auto duration = config_->timeSource().systemTime() - start_time_;
        rt_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
      }
    }
  }

  auto& circuit_breaker_manager = config_->getCircuitBreakerManager();
  circuit_breaker_manager.updateMetrics(route_uuid_, error, rt_milliseconds);

  return Http::FilterHeadersStatus::Continue;
}

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
