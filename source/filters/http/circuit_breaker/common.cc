#include "source/filters/http/circuit_breaker/common.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

static const std::chrono::milliseconds defaultLookbackDuration(10000);

CircuitBreakerSettings::CircuitBreakerSettings(
    const CircuitBreakerRouteSpecificFilterConfigProto& proto_config)
    : consecutive_slow_requests_(proto_config.consecutive_slow_requests()),
      average_response_time_threshold_(
          PROTOBUF_GET_OPTIONAL_MS(proto_config, average_response_time)),
      percentage_(proto_config.error_percent_threshold()),
      min_request_amount_(proto_config.min_request_amount()),
      break_duration_(
          std::chrono::milliseconds(PROTOBUF_GET_MS_REQUIRED(proto_config, break_duration))),
      lookback_duration_(std::chrono::milliseconds(PROTOBUF_GET_MS_OR_DEFAULT(
          proto_config, lookback_duration, defaultLookbackDuration.count()))),
      http_status_(proto_config.response().http_status()), wait_body_(proto_config.wait_body()) {
  static const ssize_t maxBodySize = 4096;

  for (auto& p : proto_config.response().headers()) {
    http_headers_.push_back({p.key(), p.value()});
  }

  std::string temp;
  if (!proto_config.response().body().inline_bytes().empty())
    temp = proto_config.response().body().inline_bytes();
  else
    temp = proto_config.response().body().inline_string();

  if (temp.length() <= maxBodySize)
    http_body_ = temp;
  else
    ENVOY_LOG(error, "circuit breaker response body string/bytes size: {} error", temp.size());
}

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
