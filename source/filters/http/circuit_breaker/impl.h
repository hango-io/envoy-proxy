#pragma once

#include "envoy/api/api.h"
#include "envoy/runtime/runtime.h"

#include "source/common/protobuf/utility.h"
#include "source/common/stats/rolling_number.h"
#include "source/filters/http/circuit_breaker/common.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

class CircuitBreakerImpl : public CircuitBreaker, Logger::Loggable<Logger::Id::filter> {
public:
  class RuntimeKeyValues {
  public:
    const std::string ErrorPercentKey = "circuitbreaker.http.error_percent";
  };

  using RuntimeKeys = ConstSingleton<RuntimeKeyValues>;

  CircuitBreakerImpl(const CircuitBreakerSettings& settings, const std::string& uuid,
                     TimeSource& time_source)
      : settings_(settings), uuid_(uuid),
        errors_(std::make_unique<Common::Stats::RollingNumberImpl>(time_source,
                                                                   settings.lookbackDuration())),
        response_times_(std::make_unique<Common::Stats::RollingNumberImpl>(
            time_source, settings.lookbackDuration())),
        requests_(std::make_unique<Common::Stats::RollingNumberImpl>(time_source,
                                                                     settings.lookbackDuration())),
        time_source_(time_source), update_time_(time_source.monotonicTime()) {}

  bool isOpen() override;
  void updateMetrics(bool error, std::chrono::milliseconds rt_milliseconds) override;

  Common::Stats::RollingNumber& rollingErrors() { return *(errors_.get()); }
  Common::Stats::RollingNumber& rollingResponseTimes() { return *(response_times_.get()); }
  Common::Stats::RollingNumber& rollingRequests() { return *(requests_.get()); }

  Envoy::SystemTime validTime() { return valid_time_.value(); }
  uint32_t consecutiveSlowRequestCount() { return consecutive_slow_request_count_; }

private:
  const CircuitBreakerSettings& settings_;
  const std::string uuid_;

  uint32_t consecutive_slow_request_count_{};

  Common::Stats::RollingNumberUniquePtr errors_;
  Common::Stats::RollingNumberUniquePtr response_times_;
  Common::Stats::RollingNumberUniquePtr requests_;

  TimeSource& time_source_;
  absl::optional<Envoy::SystemTime> valid_time_;

  Envoy::MonotonicTime update_time_;
};

class CircuitBreakerManagerImpl : public CircuitBreakerManager {
public:
  CircuitBreakerManagerImpl() {}

  bool isOpen(const std::string& uuid, CircuitBreakerFactoryCb cb) override;

  void updateMetrics(const std::string& uuid, bool error,
                     std::chrono::milliseconds rt_milliseconds) override;

  CircuitBreaker* getCircuitBreaker(const std::string& uuid) override;

private:
  absl::Mutex m_;
  std::map<std::string, CircuitBreakerPtr> circuit_breakers_;
};

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
