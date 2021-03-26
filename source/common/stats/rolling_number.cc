#include "common/stats/rolling_number.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Stats {

RollingNumberImpl::RollingNumberImpl(Envoy::TimeSource& time_source,
                                     std::chrono::milliseconds window_size)
    : time_source_(time_source), window_size_(window_size.count() / 1000) {}

double RollingNumberImpl::Sum(Envoy::SystemTime now) {
  double sum = 0;
  auto tick = unixTimeNowSeconds(now);

  for (auto it = buckets_.begin(); it != buckets_.end(); it++) {
    if (it->first > tick - window_size_) {
      sum += it->second->value;
    }
  }

  removeOldBuckets();

  return sum;
}

void RollingNumberImpl::Increment(double i) {
  auto bucket = getCurrentBucket();
  bucket->value += i;

  removeOldBuckets();
}

int64_t RollingNumberImpl::unixTimeNowSeconds() {
  return unixTimeNowSeconds(time_source_.systemTime());
}

int64_t RollingNumberImpl::unixTimeNowSeconds(Envoy::SystemTime now) {
  return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

RollingNumberImpl::BucketSharedPtr RollingNumberImpl::getCurrentBucket() {
  auto tick = unixTimeNowSeconds();
  auto it = buckets_.find(tick);
  if (it == buckets_.end()) {
    buckets_.emplace(tick, std::make_shared<Bucket>());
  }
  return buckets_[tick];
}

void RollingNumberImpl::removeOldBuckets() {
  auto tick = unixTimeNowSeconds() - window_size_;
  auto it = buckets_.lower_bound(tick);

  buckets_.erase(buckets_.begin(), it);
}

} // namespace Stats
} // namespace Common
} // namespace Proxy
} // namespace Envoy
