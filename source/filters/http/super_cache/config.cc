#include "source/filters/http/super_cache/config.h"

#include <memory>

#include "envoy/common/time.h"
#include "envoy/registry/registry.h"
#include "envoy/stats/scope.h"

#include "source/filters/http/super_cache/cache_filter.h"

#include "api/proxy/filters/http/super_cache/v2/super_cache.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace SuperCache {

static constexpr char SuperCache[] = "proxy.filters.http.super_cache";
static constexpr char LocalCache[] = "proxy.filters.http.local_cache";
static constexpr char RedisCache[] = "proxy.filters.http.redis_cache";

using SuperCacheFilterFactory = CacheFilterFactory<SuperCache>;
using LocalCacheFilterFactory = CacheFilterFactory<LocalCache>;
using RedisCacheFilterFactory = CacheFilterFactory<RedisCache>;

REGISTER_FACTORY(SuperCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);
REGISTER_FACTORY(LocalCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);
REGISTER_FACTORY(RedisCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace SuperCache
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
