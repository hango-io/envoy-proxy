#include "source/common/cache/redis_cache_impl.h"

#include "envoy/registry/registry.h"

#include "source/common/cache/http_cache_entry.h"
#include "source/common/common/hex.h"
#include "source/common/common/proxy_utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

class RedisCacheFactory : public CommonCacheFactory {
public:
  CommonCacheBasePtr create(const Cache& config,
                            Server::Configuration::FactoryContext& factory) override {
    if (config.cache_type_case() != Cache::CacheTypeCase::kRedis) {
      return nullptr;
    }
    auto cache = std::make_unique<RedisCache<HttpCacheEntry>>(config, factory);
    return cache;
  }

  std::string name() const override { return "RedisCache"; }
};

static Registry::RegisterFactory<RedisCacheFactory, CommonCacheFactory> register_;

class RedisRequestCacheFactory : public CommonCacheFactory {
public:
  CommonCacheBasePtr create(const Cache& config,
                            Server::Configuration::FactoryContext& factory) override {
    if (config.cache_type_case() != Cache::CacheTypeCase::kRedis) {
      return nullptr;
    }
    auto cache = std::make_unique<RedisCache<HttpRequestCacheEntry>>(config, factory);
    return cache;
  }

  std::string name() const override { return "RedisRequestCache"; }
};

static Registry::RegisterFactory<RedisRequestCacheFactory, CommonCacheFactory>
    request_cache_register_;

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
