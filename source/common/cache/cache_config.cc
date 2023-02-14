#include "source/common/cache/cache_config.h"

#include "source/common/cache/local_cache_impl.h"
#include "source/common/cache/redis_cache_impl.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

CommonCacheBasePtr CacheConfig::create(const ProtoCache& cache_config,
                                       Server::Configuration::FactoryContext& context,
                                       CacheEntryCreator creator) {
  switch (cache_config.cache_type_case()) {
  case ProtoCache::CacheTypeCase::kRedis:
    return std::make_unique<RedisCache>(cache_config.redis(), context, creator);
  case ProtoCache::CacheTypeCase::kLocal:
    return std::make_unique<LocalCache>(cache_config.local(), context);
  default:
    throw EnvoyException("Error cache type and should never run here");
  }
  return nullptr;
}

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
