#pragma once

#include <chrono>
#include <cstdint>

#include "envoy/server/filter_config.h"

#include "source/common/cache/cache_base.h"
#include "source/common/common/logger.h"
#include "source/common/common/proxy_utility.h"
#include "source/common/event/dispatcher_impl.h"
#include "source/common/http/message_impl.h"
#include "source/common/redis/async_client.h"

#include "api/proxy/common/cache_api/v3/cache_api.pb.h"
#include "src/sw/redis++/redis++.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Cache {

namespace {
constexpr uint64_t DEFAULT_REDIS_TIMEOUT = 5; // ms
} // namespace

using RedisConfig = proxy::common::cache_api::v3::RedisCacheImpl;

class RedisCache : public CommonCacheBase, Logger::Loggable<Logger::Id::client> {
public:
  RedisCache(const RedisConfig& config, Server::Configuration::FactoryContext& factory,
             CacheEntryCreator creator);

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

  CacheEntryCreator cache_entry_creator_;

  ThreadLocal::SlotPtr tls_slot_;
};

} // namespace Cache
} // namespace Common
} // namespace Proxy
} // namespace Envoy
