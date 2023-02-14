#include "source/filters/http/circuit_breaker/impl.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace CircuitBreaker {

bool CircuitBreakerImpl::isOpen() {
  if (!valid_time_.has_value()) {
    return false;
  }

  if (time_source_.systemTime() > valid_time_.value()) {
    ENVOY_LOG(debug, "{}", "reset circuitbreaker due to timeout");
    valid_time_.reset();
    consecutive_slow_request_count_ = 0;
    return false;
  }

  return true;
}

void CircuitBreakerImpl::updateMetrics(bool error, std::chrono::milliseconds rt_milliseconds) {
  double rt = double(rt_milliseconds.count());
  requests_->Increment(1);
  response_times_->Increment(rt);
  if (error) {
    errors_->Increment(1);
  }

  if (settings_.averageResponseTimeThreshold().has_value()) {
    ENVOY_LOG(debug, "avg rt: consecutive_slow_request_count = {}, rt = {}",
              consecutive_slow_request_count_, rt);
    if (rt > double(settings_.averageResponseTimeThreshold().value().count())) {
      consecutive_slow_request_count_++;
    } else {
      consecutive_slow_request_count_ = 0;
    }
  }

  bool should_break(false);
  auto now = time_source_.systemTime();

  if (settings_.consecutiveSlowRequests() > 0) {
    if (consecutive_slow_request_count_ >= settings_.consecutiveSlowRequests()) {
      should_break = true;
    }
  }

  if (settings_.minRequestAmount() > 0 && settings_.errorPercentThreshold().value() > 0) {
    auto total = requests_->Sum(now);
    auto errors = errors_->Sum(now);
    ENVOY_LOG(debug, "error percent: total = {}, error = {}", total, errors);
    if (total >= settings_.minRequestAmount() &&
        (errors / total * 100 >= settings_.errorPercentThreshold().value())) {
      should_break = true;
    }
  }

  if (should_break) {
    ENVOY_LOG(debug, "{}", "circuitbreaker open");
    valid_time_.emplace(now + settings_.breakDuration());
  }
}

void CircuitBreakerManagerImpl::updateMetrics(const std::string& uuid, bool error,
                                              std::chrono::milliseconds rt_milliseconds) {
  absl::WriterMutexLock lock(&m_);

  auto it = circuit_breakers_.find(uuid);
  if (it == circuit_breakers_.end()) {
    return;
  }

  it->second->updateMetrics(error, rt_milliseconds);
  return;
}

bool CircuitBreakerManagerImpl::isOpen(const std::string& uuid, CircuitBreakerFactoryCb cb) {
  absl::WriterMutexLock lock(&m_);

  auto it = circuit_breakers_.find(uuid);
  if (it == circuit_breakers_.end()) {
    circuit_breakers_.emplace(uuid, cb());
    it = circuit_breakers_.find(uuid);
  }

  return it->second->isOpen();
}

CircuitBreaker* CircuitBreakerManagerImpl::getCircuitBreaker(const std::string& uuid) {
  absl::WriterMutexLock lock(&m_);

  auto it = circuit_breakers_.find(uuid);
  if (it == circuit_breakers_.end()) {
    return nullptr;
  }

  return it->second.get();
}

} // namespace CircuitBreaker
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
