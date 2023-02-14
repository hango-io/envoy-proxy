#include "source/common/cache/local_cache_impl.h"

#include "envoy/registry/registry.h"

#include "source/common/common/proxy_utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

constexpr uint64_t DEFAULT_MAX_CACHE_SIZE = 128 * 1024 * 1024; // 128 MB

LruCacheImpl::LruCacheImpl(uint64_t max_cache_length) : max_cache_length_(max_cache_length) {}

void LruCacheImpl::insert(const CacheKeyType& key, CacheEntryPtr&& value) {
  Thread::LockGuard lock(mutex_);
  // remove old data.
  removeImpl(key);
  // insert new data.
  cache_length_ += value->cacheLength();
  m_list_.push_front(key);
  m_dict_[key] = {m_list_.begin(), std::move(value)};

  // When memory usage exceeds the limit, the cache memory recall action is triggered. The last 10%
  // of the LRU queue is currently reclaimed directly until the memory is reduced below the maximum
  // memory space.
  while (cache_length_ > max_cache_length_) {
    drainNumber(std::max(m_dict_.size() / 10, 1UL));
  }
}

CacheEntryPtr LruCacheImpl::lookup(const CacheKeyType& key) {
  Thread::LockGuard lock(mutex_);

  checkMemory();

  auto iter = m_dict_.find(key);
  if (iter == m_dict_.end()) {
    return nullptr;
  }
  // Cache entry is find but it is expired and just remove it.
  if (Common::TimeUtil::createTimestamp() >= iter->second.second->cacheExpire()) {
    removeImpl(key);
    return nullptr;
  }
  if (iter->second.first != m_list_.begin()) {
    m_list_.erase(iter->second.first);
    m_list_.push_front(key);
    iter->second.first = m_list_.begin();
  }
  return iter->second.second->createCopy();
}

void LruCacheImpl::remove(const CacheKeyType& key) {
  Thread::LockGuard lock(mutex_);
  removeImpl(key);
}

void LruCacheImpl::removeImpl(const CacheKeyType& key) {
  auto iter = m_dict_.find(key);
  if (iter == m_dict_.end()) {
    return;
  }
  cache_length_ -= iter->second.second->cacheLength();
  m_list_.erase(iter->second.first);
  m_dict_.erase(iter);
}

uint64_t LruCacheImpl::cacheNumber() const {
  Thread::LockGuard lock(mutex_);
  return m_dict_.size();
}

uint64_t LruCacheImpl::cacheLength() const {
  Thread::LockGuard lock(mutex_);
  return cache_length_;
}

uint64_t LruCacheImpl::drainNumber(uint64_t number) {
  uint64_t true_number = 0;
  for (size_t i = 0; i < number; i++) {
    if (m_dict_.empty()) {
      break;
    }
    CacheKeyType to_remove = *m_list_.rbegin();
    removeImpl(to_remove);
    true_number++;
  }
  return true_number;
}

LocalCache::LocalCache(const LocalConfig& config, Server::Configuration::FactoryContext& factory) {
  tls_slot_ = factory.threadLocal().allocateSlot();
  tls_slot_->set(
      [](Event::Dispatcher& dispatcher) { return std::make_unique<CacheThreadLocal>(dispatcher); });

  lru_caches_.reserve(cache_list_number_);
  uint64_t max_cache_size =
      config.has_max_cache_size() ? config.max_cache_size().value() : DEFAULT_MAX_CACHE_SIZE;

  uint64_t single_max_cache_size = max_cache_size / cache_list_number_;
  for (size_t i = 0; i < cache_list_number_; i++) {
    lru_caches_.push_back(std::make_unique<LruCacheImpl>(single_max_cache_size));
  }
}

CacheEntryPtr LocalCache::lookupCache(const CacheKeyType& key) {
  ASSERT(!key.empty());
  return lru_caches_[static_cast<uint8_t>(key[0]) % cache_list_number_]->lookup(key);
}

void LocalCache::lookupCache(const CacheKeyType& key, AsyncCallback callback) {
  auto result_wrapper = std::make_shared<CacheEntryPtr>(lookupCache(key));
  auto& thread_lcoal = tls_slot_->getTyped<CacheThreadLocal>();
  thread_lcoal.dispather_.post(
      [result_wrapper, callback]() { callback("", std::move(*result_wrapper)); });
}

void LocalCache::insertCache(const CacheKeyType& key, CacheEntryPtr&& value) {
  lru_caches_[static_cast<uint8_t>(key[0]) % cache_list_number_]->insert(key, std::move(value));
}

void LocalCache::removeCache(const CacheKeyType& key) {
  lru_caches_[static_cast<uint8_t>(key[0]) % cache_list_number_]->remove(key);
}

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
