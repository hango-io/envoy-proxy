#pragma once

#include <list>

#include "envoy/server/factory_context.h"

#include "source/common/cache/cache_base.h"
#include "source/common/common/lock_guard.h"
#include "source/common/common/logger.h"
#include "source/common/common/thread.h"

#include "api/proxy/common/cache_api/v3/cache_api.pb.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

using LocalConfig = proxy::common::cache_api::v3::LocalCacheImpl;

using ListType = std::list<CacheKeyType>;
using DictType = std::map<CacheKeyType, std::pair<ListType::iterator, CacheEntryPtr>>;

/*
 * Specific implementation of lru cache . The cache must maintain its own data length and reclaim
 * memory space when it runs out of space. The current implementation does not support timer-based
 * memory recall. It will be implemented in the next version.
 */
class LruCacheImpl {
public:
  LruCacheImpl(uint64_t max_cache_length);

  uint64_t cacheNumber() const;
  uint64_t cacheLength() const;

  void insert(const CacheKeyType& key, CacheEntryPtr&& value);
  CacheEntryPtr lookup(const CacheKeyType& key);
  void remove(const CacheKeyType& key);

private:
  void removeImpl(const CacheKeyType& key);

  uint64_t drainNumber(uint64_t number);

  void checkMemory() {
    // If m_list_ size not equal to m_dict_ size, then there is some error. To avoid Crash, We just
    // clear all cache now.
    if (m_list_.size() != m_dict_.size()) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::client), error,
                          "Local cache memory error, please check your code carefully");

      m_dict_.clear();
      m_list_.clear();
      cache_length_ = 0;
    }
  }

  DictType m_dict_;
  ListType m_list_;

  const uint64_t max_cache_length_{0};
  uint64_t cache_length_{0};
  mutable Thread::MutexBasicLockable mutex_;
};

using LruCacheImplPtr = std::unique_ptr<LruCacheImpl>;

/*
 * Although Using TLS cache can solves the problem of multi-threaded data contention, there will be
 * new another risk. Requests are randomly assigned to different workers, and if we use TLS cache
 * then the same request will be repeatedly cached in the cache of multiple workers! If a single LRU
 * cache is directly shared among multiple threads, it will cause a lot of data contention and
 * performance degradation. Therefore, multiple LRU caches are designed here to store data in
 * different LRU according to their keys, which can share cache data and reduce the probability of
 * contention among multiple threads at the same time.
 */
class LocalCache : public CommonCacheBase {
public:
  LocalCache(const LocalConfig&, Server::Configuration::FactoryContext&);

  void insertCache(const CacheKeyType& key, CacheEntryPtr&& value) override;
  void removeCache(const CacheKeyType& key) override;

  CacheEntryPtr lookupCache(const CacheKeyType& key) override;

  void lookupCache(const CacheKeyType& key, AsyncCallback callback) override;

private:
  // We use TLS to ensure that the asynchronous callback function is executed on the correct worker.
  struct CacheThreadLocal : public ThreadLocal::ThreadLocalObject {
    CacheThreadLocal(Event::Dispatcher& dispather) : dispather_(dispather) {}
    Event::Dispatcher& dispather_;
  };
  ThreadLocal::SlotPtr tls_slot_;

  // TODO(wbpcode): Make this configurable.
  const uint8_t cache_list_number_{16};

  std::vector<LruCacheImplPtr> lru_caches_;
};

using LocalCacheSharedPtr = std::shared_ptr<LocalCache>;

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
