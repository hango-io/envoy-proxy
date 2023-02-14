#pragma once

#include <map>
#include <memory>

#include "envoy/common/time.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Stats {

class RollingNumber {
public:
  virtual ~RollingNumber() = default;

  virtual void Increment(double i) PURE;

  virtual double Sum(Envoy::SystemTime now) PURE;
};

using RollingNumberUniquePtr = std::unique_ptr<RollingNumber>;

class RollingNumberImpl : public RollingNumber {
public:
  RollingNumberImpl(Envoy::TimeSource& time_source, std::chrono::milliseconds window_size);

  struct Bucket {
    double value;
  };

  using BucketSharedPtr = std::shared_ptr<Bucket>;

  void Increment(double i);
  double Sum(Envoy::SystemTime now);

private:
  int64_t unixTimeNowSeconds();
  int64_t unixTimeNowSeconds(Envoy::SystemTime now);
  BucketSharedPtr getCurrentBucket();
  void removeOldBuckets();

private:
  Envoy::TimeSource& time_source_;
  int64_t window_size_;
  std::map<int64_t, BucketSharedPtr> buckets_;
};

} // namespace Stats
} // namespace Common
} // namespace Proxy
} // namespace Envoy
