#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "source/common/cache/cache_base.h"

#include "api/proxy/common/cache_api/v3/cache_api.pb.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

using ProtoCache = proxy::common::cache_api::v3::Cache;

class CacheConfig {
public:
  static CommonCacheBasePtr create(const ProtoCache& cache_config,
                                   Server::Configuration::FactoryContext&, CacheEntryCreator);
};

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
