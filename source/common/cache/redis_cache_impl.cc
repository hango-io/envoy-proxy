#include "source/common/cache/redis_cache_impl.h"

#include "envoy/registry/registry.h"

#include "source/common/cache/http_cache_entry.h"
#include "source/common/common/hex.h"
#include "source/common/common/proxy_utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

RedisCache::RedisCache(const RedisConfig& cache_config,
                       Server::Configuration::FactoryContext& factory, CacheEntryCreator creator)
    : cache_entry_creator_(std::move(creator)) {
  ASSERT(cache_entry_creator_ != nullptr);

  if (cache_config.redis_type_case() != RedisConfig::RedisTypeCase::kGeneral) {
    throw EnvoyException("Unsupported redis mode and please check your config.");
  }

  tls_slot_ = factory.threadLocal().allocateSlot();
  tls_slot_->set([cache_config](Event::Dispatcher& dispatcher) {
    Event::DispatcherImpl* impl = dynamic_cast<Event::DispatcherImpl*>(&dispatcher);
    ASSERT(impl != nullptr);

    RedisClient::Endpoint ep = {cache_config.general().host(),
                                int(cache_config.general().port().value())};

    auto client = std::make_unique<RedisClient>(
        ep, cache_config.password(), &impl->base(),
        cache_config.has_timeout() ? cache_config.timeout().value() : DEFAULT_REDIS_TIMEOUT);
    return std::make_unique<CacheThreadLocal>(dispatcher, std::move(client));
  });
}

void RedisCache::insertCache(const CacheKeyType& key, CacheEntryPtr&& value) {
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

void RedisCache::removeCache(const CacheKeyType& key) {
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

CacheEntryPtr RedisCache::lookupCache(const CacheKeyType&) {
  ENVOY_LOG(error, "Sync redis command is not supported now and please use async lookup");
  return nullptr;
}

void RedisCache::lookupCache(const CacheKeyType& key, AsyncCallback callback) {
  auto& thread_lcoal = tls_slot_->getTyped<CacheThreadLocal>();
  if (thread_lcoal.client_->clientStatus() != RedisClient::Status::WORKING) {
    ENVOY_LOG(debug, "Redis client is disconnected with Redis server and cannot get key: {}", key);
    thread_lcoal.dispatcher_.post(
        [cb = std::move(callback)]() { cb("NO CONNECTION TO REDIS", nullptr); });
    return;
  }

  auto& dispatcher = thread_lcoal.dispatcher_;

  try {
    thread_lcoal.client_->get(key, [creator = cache_entry_creator_, callback = std::move(callback),
                                    &dispatcher](absl::optional<RedisClient::Reply> reply,
                                                 absl::optional<RedisClient::Error> error) {
      if (error.has_value() || !reply.has_value()) {
        dispatcher.post([cb = std::move(callback)]() { cb("", nullptr); });
        return;
      }
      auto entry = creator();
      entry->loadFromString(std::move(reply.value()));
      auto entry_wrapper = std::make_shared<CacheEntryPtr>(std::move(entry));

      dispatcher.post(
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
