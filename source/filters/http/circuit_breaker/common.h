#pragma once

#include <memory>

#include "envoy/api/api.h"
#include "envoy/common/pure.h"
#include "envoy/runtime/runtime.h"

#include "source/common/protobuf/utility.h"

#include "api/proxy/filters/http/circuit_breaker/v2/circuit_breaker.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

using CircuitBreakerFilterConfigProto = proxy::filters::http::circuit_breaker::v2::CircuitBreaker;
using CircuitBreakerRouteSpecificFilterConfigProto =
    proxy::filters::http::circuit_breaker::v2::CircuitBreakerPerRoute;

class CircuitBreakerSettings : public Logger::Loggable<Logger::Id::filter> {
public:
  CircuitBreakerSettings(const CircuitBreakerRouteSpecificFilterConfigProto& proto_config);

  uint32_t consecutiveSlowRequests() const { return consecutive_slow_requests_; }
  absl::optional<std::chrono::milliseconds> averageResponseTimeThreshold() const {
    return average_response_time_threshold_;
  }
  envoy::type::Percent errorPercentThreshold() const { return percentage_; }
  uint32_t minRequestAmount() const { return min_request_amount_; }
  std::chrono::milliseconds breakDuration() const { return break_duration_; }
  std::chrono::milliseconds lookbackDuration() const { return lookback_duration_; }
  uint32_t httpStatus() const { return http_status_; }
  const std::vector<std::pair<std::string, std::string>>& httpHeaders() const {
    return http_headers_;
  }
  const std::string& httpBody() const { return http_body_; }
  bool waitBody() const { return wait_body_; }

private:
  const uint32_t consecutive_slow_requests_;
  const absl::optional<std::chrono::milliseconds> average_response_time_threshold_;
  const envoy::type::Percent percentage_;
  const uint32_t min_request_amount_;
  const std::chrono::milliseconds break_duration_;
  const std::chrono::milliseconds lookback_duration_;
  const uint32_t http_status_;
  std::vector<std::pair<std::string, std::string>> http_headers_;
  std::string http_body_;
  const bool wait_body_{};
};

class CircuitBreaker {
public:
  virtual ~CircuitBreaker() = default;

  virtual bool isOpen() PURE;
  virtual void updateMetrics(bool error, std::chrono::milliseconds rt_milliseconds) PURE;
};

using CircuitBreakerSharedPtr = std::shared_ptr<CircuitBreaker>;
using CircuitBreakerPtr = std::unique_ptr<CircuitBreaker>;

/**
 * Callback function used to create a circuitbreaker.
 */
using CircuitBreakerFactoryCb = std::function<CircuitBreakerPtr()>;

using CircuitBreakerAccessCb = std::function<void(CircuitBreaker*)>;

/**
 * A manager for multiple circuitbreakers. Thread safe.
 */
class CircuitBreakerManager {
public:
  virtual ~CircuitBreakerManager() = default;

  virtual bool isOpen(const std::string& uuid, CircuitBreakerFactoryCb cb) PURE;

  virtual void updateMetrics(const std::string& uuid, bool error,
                             std::chrono::milliseconds rt_milliseconds) PURE;

  // This method is not thread safe and is for test only.
  virtual CircuitBreaker* getCircuitBreaker(const std::string& uuid) PURE;
};

using CircuitBreakerManagerSharedPtr = std::shared_ptr<CircuitBreakerManager>;

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
