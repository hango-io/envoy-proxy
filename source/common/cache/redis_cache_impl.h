#pragma once

#include <chrono>
#include <cstdint>

#include "source/common/cache/cache_base.h"
#include "source/common/common/logger.h"
#include "source/common/common/proxy_utility.h"
#include "source/common/event/dispatcher_impl.h"
#include "source/common/http/message_impl.h"
#include "source/common/redis/async_client.h"

#include "src/sw/redis++/redis++.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

namespace {
constexpr uint64_t DEFAULT_REDIS_TIMEOUT = 5; // ms
} // namespace

using RedisConfig = proxy::common::cache_api::v3::RedisCacheImpl;

template <class T> class RedisCache : public CommonCacheBase, Logger::Loggable<Logger::Id::client> {
public:
  RedisCache(const Cache& config, Server::Configuration::FactoryContext& factory);

  void insertCache(const CacheKeyType& key, CacheEntryPtr&& value) override;
  void removeCache(const CacheKeyType& key) override;

  CacheEntryPtr lookupCache(const CacheKeyType& key) override;
  void lookupCache(const CacheKeyType& key, AsyncCallback callback) override;

private:
  using RedisClient = Proxy::Common::Redis::AsyncClient;
  using RedisClientPtr = std::unique_ptr<RedisClient>;

  void connectToRemote(RedisClient& client, const RedisConfig& config);

  struct CacheThreadLocal : public ThreadLocal::ThreadLocalObject {
    CacheThreadLocal(Event::Dispatcher& dispatcher, RedisClientPtr&& client)
        : dispatcher_(dispatcher), client_(std::move(client)) {}

    Event::Dispatcher& dispatcher_;
    RedisClientPtr client_{nullptr};
  };

  ThreadLocal::SlotPtr tls_slot_;
};

template <class T>
RedisCache<T>::RedisCache(const Cache& config, Server::Configuration::FactoryContext& factory) {

  if (config.redis().redis_type_case() != RedisConfig::RedisTypeCase::kGeneral) {
    throw EnvoyException("Unsupported redis mode and please check your config.");
  }
  // set some default value.
  RedisConfig cache_config(config.redis());
  if (!cache_config.has_timeout() || cache_config.timeout().value() == 0) {
    cache_config.mutable_timeout()->set_value(DEFAULT_REDIS_TIMEOUT);
  }

  tls_slot_ = factory.threadLocal().allocateSlot();
  tls_slot_->set([cache_config](Event::Dispatcher& dispatcher) {
    Event::DispatcherImpl* impl = dynamic_cast<Event::DispatcherImpl*>(&dispatcher);
    ASSERT(impl);

    RedisClient::Endpoint ep = {cache_config.general().host(),
                                int(cache_config.general().port().value())};

    auto client = std::make_unique<RedisClient>(ep, cache_config.password(), &impl->base(),
                                                cache_config.timeout().value());
    auto result = std::make_unique<CacheThreadLocal>(dispatcher, std::move(client));
    return result;
  });
}

template <class T> void RedisCache<T>::insertCache(const CacheKeyType& key, CacheEntryPtr&& value) {
  auto& thread_lcoal = tls_slot_->getTyped<CacheThreadLocal>();
  if (thread_lcoal.client_->clientStatus() != RedisClient::Status::WORKING) {
    ENVOY_LOG(debug, "Redis client is disconnected with Redis server and cannot set key: {}", key);
    return;
  }

  ENVOY_LOG(debug, "Try to insert key: {} to Redis cache", key);
  try {
    auto ttl_count = value->cacheExpire() - Common::TimeUtil::createTimestamp();
    if (ttl_count <= 0) {
      return;
    }
    auto string_value = value->serializeAsString();
    if (!string_value.has_value()) {
      return;
    }
    thread_lcoal.client_->set(key, string_value.value(), ttl_count);
  } catch (std::exception& e) {
    ENVOY_LOG(error, "Insert {} to Redis cache error: {}", key, e.what());
  }
}

template <class T> void RedisCache<T>::removeCache(const CacheKeyType& key) {
  auto& thread_lcoal = tls_slot_->getTyped<CacheThreadLocal>();
  if (thread_lcoal.client_->clientStatus() != RedisClient::Status::WORKING) {
    ENVOY_LOG(debug, "Redis client is disconnected with Redis server and cannot del key: {}", key);
    return;
  }
  try {
    thread_lcoal.client_->del(key);
  } catch (std::exception& e) {
    ENVOY_LOG(error, "Remove key: {} from Redis cache error: {}", key, e.what());
  }
}

template <class T> CacheEntryPtr RedisCache<T>::lookupCache(const CacheKeyType&) {
  static_assert(std::is_base_of<CacheEntry, T>::value, "'T' must subclass of 'CacheEntry'");
  ENVOY_LOG(error, "Sync redis command is not supported now and please use async lookup");
  return nullptr;
}

template <class T>
void RedisCache<T>::lookupCache(const CacheKeyType& key, AsyncCallback callback) {
  static_assert(std::is_base_of<CacheEntry, T>::value, "'T' must subclass of 'CacheEntry'");

  auto& thread_lcoal = tls_slot_->getTyped<CacheThreadLocal>();
  if (thread_lcoal.client_->clientStatus() != RedisClient::Status::WORKING) {
    ENVOY_LOG(debug, "Redis client is disconnected with Redis server and cannot get key: {}", key);
    thread_lcoal.dispatcher_.post(
        [cb = std::move(callback)]() { cb("NO CONNECTION TO REDIS", nullptr); });
    return;
  }
  try {
    thread_lcoal.client_->get(key, [&thread_lcoal, callback = std::move(callback)](
                                       absl::optional<RedisClient::Reply> reply,
                                       absl::optional<RedisClient::Error> error) {
      if (error.has_value() || !reply.has_value()) {
        thread_lcoal.dispatcher_.post([cb = std::move(callback)]() { cb("", nullptr); });
        return;
      }
      auto entry = std::make_unique<T>();
      entry->loadFromString(reply.value());
      auto entry_wrapper = std::make_shared<CacheEntryPtr>(std::move(entry));

      thread_lcoal.dispatcher_.post(
          [cb = std::move(callback), en = std::move(entry_wrapper)]() { cb("", std::move(*en)); });
    });
  } catch (std::exception& e) {
    ENVOY_LOG(error, "Lookup key: {} from redis cache error: {}", key, e.what());
  }
}

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
