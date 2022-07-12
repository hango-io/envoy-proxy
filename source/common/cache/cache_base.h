#pragma once

#include <functional>
#include <string>
#include <vector>

#include "envoy/buffer/buffer.h"
#include "envoy/server/filter_config.h"
#include "envoy/thread_local/thread_local.h"

#include "source/common/common/assert.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/proxy/common/cache_api/v3/cache_api.pb.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

using Cache = proxy::common::cache_api::v3::Cache;

using CacheKeyType = std::string;

class CacheEntry;
using CacheEntryPtr = std::unique_ptr<CacheEntry>;

/*
 * Abstract base class of cache entry. This class defined basic method that a cache entry must
 * implement such as serialize, deserialize, create copy and get cache entry memory size.
 */
class CacheEntry {
public:
  virtual void loadFromString(absl::string_view string_value) PURE;
  virtual CacheEntryPtr createCopy() PURE;
  virtual uint64_t cacheExpire() PURE;
  virtual void cacheExpire(uint64_t) PURE;
  virtual uint64_t cacheLength() PURE;
  virtual absl::optional<std::string> serializeAsString() PURE;
  virtual ~CacheEntry() = default;
};

/*
 * Abstract base class of cache entry. This class defined basic method that a cache must implement
 * such as lookup cache entry, insert new cache entry and delete old cache entry.
 */
class CommonCacheBase {
public:
  using AsyncCallback = std::function<void(std::string, CacheEntryPtr&&)>;

  virtual CacheEntryPtr lookupCache(const CacheKeyType&) PURE;
  virtual void lookupCache(const CacheKeyType& key, AsyncCallback callback) PURE;

  virtual void removeCache(const CacheKeyType& key) PURE;
  virtual void insertCache(const CacheKeyType& key, CacheEntryPtr&& value) PURE;

  virtual ~CommonCacheBase() = default;
};

using CommonCacheBasePtr = std::unique_ptr<CommonCacheBase>;
using CommonCacheBaseSharedPtr = std::shared_ptr<CommonCacheBase>;

// Factory interface for cache implementations to implement and register.
class CommonCacheFactory {
public:
  virtual CommonCacheBasePtr create(const Cache&, Server::Configuration::FactoryContext&) PURE;

  virtual std::string name() const PURE;

  virtual std::string category() const { return "proxy.cache"; }

  virtual ~CommonCacheFactory() = default;
};

using CommonCacheFactorySharedPtr = std::shared_ptr<CommonCacheFactory>;

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
