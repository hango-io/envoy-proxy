#include "source/common/sender/cache_request_sender.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

#include "envoy/http/header_map.h"

#include "source/common/cache/http_cache_entry.h"
#include "source/common/common/hex.h"
#include "source/common/common/proxy_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/proxy_base.h"
#include "source/common/http/utility.h"

#include "openssl/md5.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Sender {

namespace {

constexpr absl::string_view RedisCacheName = "RedisCache";
constexpr absl::string_view LocalCacheName = "LocalCache";

constexpr absl::string_view OldRedisCacheName = "RedisHttpCache";
constexpr absl::string_view OldLocalCacheName = "LocalHttpCache";

constexpr absl::string_view RedisRequestCache = "RedisRequestCache";

bool isRedisCache(const std::string& name) {
  return name == RedisCacheName || name == OldRedisCacheName;
}
bool isLocalCache(const std::string& name) {
  return name == LocalCacheName || name == OldLocalCacheName;
}

} // namespace

Cache::CacheKeyType HttpCacheUtil::cacheKey(const KeyMakerConfig& config,
                                            Envoy::Http::RequestHeaderMap& headers,
                                            const std::string& prefix) {
  ASSERT(headers.Method());
  ASSERT(headers.Path());
  ASSERT(headers.Host());

  std::string raw(prefix);
  raw.reserve(128);

  // TODO(wbpcode): use raw += headers.Method()->value().getStringView().data();
  if (!config.exclude_path()) {
    raw += std::string(headers.Method()->value().getStringView());
  }
  if (!config.exclude_host()) {
    raw += std::string(headers.Host()->value().getStringView());
  }

  // path and query string
  const auto path = headers.Path()->value().getStringView();
  if (!config.exclude_path()) {
    raw += std::string(Envoy::Http::PathUtil::removeQueryAndFragment(path));
  }
  auto query_params = Envoy::Http::Utility::parseQueryString(path);

  for (auto& query : config.query_params()) {
    if (query_params.find(query) != query_params.end()) {
      raw = raw + query + query_params[query];
    }
  }
  // headers
  for (auto& header : config.headers_keys()) {
    auto result = headers.get(Envoy::Http::LowerCaseString(header));
    if (result.empty()) {
      continue;
    }
    raw.append(std::string(result[0]->key().getStringView()));
    raw.append(std::string(result[0]->value().getStringView()));
  }

  if (config.ignore_case()) {
    std::transform(raw.begin(), raw.end(), raw.begin(), ::tolower);
  }

  ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::client), trace,
                      "Request Cache key string: {}", raw);

  uint8_t md5_result[MD5_DIGEST_LENGTH];
  MD5((const uint8_t*)raw.data(), raw.size(), md5_result);
  return Hex::encode(md5_result, MD5_DIGEST_LENGTH);
}

CacheGetterSetterConfig::CacheGetterSetterConfig(const ProtoCaches& caches,
                                                 Server::Configuration::FactoryContext& context,
                                                 bool request_cache) {
  std::set<absl::string_view> used_names;
  for (auto it = caches.begin(); it != caches.end(); it++) {
    // get cache name
    absl::string_view name;
    switch (it->cache_type_case()) {
    case Cache::Cache::CacheTypeCase::kRedis:
      name = RedisCacheName;
      break;
    case Cache::Cache::CacheTypeCase::kLocal:
      name = LocalCacheName;
      break;
    default:
      throw EnvoyException("Error cache type and should never run here");
    }
    // no repeated
    if (used_names.find(name) != used_names.end()) {
      continue;
    }
    used_names.insert(name);
    // create cache
    Envoy::Proxy::Common::Cache::CommonCacheFactory* factory = nullptr;

    if (name == RedisCacheName && request_cache) {
      factory = Registry::FactoryRegistry<Cache::CommonCacheFactory>::getFactory(RedisRequestCache);
    } else {
      factory = Registry::FactoryRegistry<Cache::CommonCacheFactory>::getFactory(name);
    }

    if (!factory) {
      ENVOY_LOG(error, "No supported cache factory for cache: {}", name);
      continue;
    }
    auto cache = factory->create(*it, context);
    if (!cache) {
      ENVOY_LOG(error, "Error get cache implemention for cache: {}", name);
      continue;
    }
    used_caches_.push_back({std::string(name), std::move(cache)});
  }
}

SpecificCacheConfig::SpecificCacheConfig(const KeyMakerConfig& key_maker,
                                         const std::map<std::string, ProtoTTL>& proto_ttls,
                                         const std::string& prefix)
    : cache_key_prefix_(prefix), key_maker_config_(key_maker) {

  for (auto it = proto_ttls.begin(); it != proto_ttls.end(); it++) {
    std::string cache_name;
    // 兼容旧配置
    if (isRedisCache(it->first)) {
      cache_name = std::string(RedisCacheName);
    } else if (isLocalCache(it->first)) {
      cache_name = std::string(LocalCacheName);
    } else {
      cache_name = it->first;
    }
    cache_ttls_.insert({cache_name, std::pair<uint64_t, CustomsTTL>()});
    auto& ttl = cache_ttls_[cache_name];
    ttl.first = it->second.default_();
    for (auto& v : it->second.customs()) {
      ttl.second.push_back({std::regex(v.first), v.second});
    }
  }
}

uint64_t SpecificCacheConfig::cacheTTL(std::string name, std::string code) const {
  auto cache_ttl = cache_ttls_.find(name);
  if (cache_ttl == cache_ttls_.end()) {
    return 0;
  }
  for (const auto& i : cache_ttl->second.second) {
    if (std::regex_match(code, i.first)) {
      return i.second; // custom
    }
  }
  return cache_ttl->second.first; // default
}

CacheGetterSetter::CacheGetterSetter(CacheGetterSetterConfig* config,
                                     const SpecificCacheConfig* route_config)
    : used_caches_(config->usedCaches()), route_config_(route_config) {}

CacheLookupStatus CacheGetterSetter::lookupCache(CacheLookupCallback* callback) {
  if (used_caches_.size() <= 0 || callback == nullptr || !has_cache_key_) {
    return CacheLookupStatus::FAILED;
  }
  callback_ = callback;

  auto keep_self_live = shared_from_this();
  lookupCacheOnce(std::move(keep_self_live));

  return CacheLookupStatus::ONCALL;
}

void CacheGetterSetter::lookupCacheOnce(CacheGetterSetterSharedPtr keep_self_live) {
  if (lookup_cache_over_ || lookup_cache_stop_ || !callback_) {
    return;
  }
  if (current_cache_ >= used_caches_.size()) {
    lookup_cache_stop_ = true;
    callback_->onFailure();
    callback_ = nullptr;
    return;
  }

  auto cache_lookup_callback = [self = std::move(keep_self_live),
                                this](std::string, Cache::CacheEntryPtr&& entry) {
    // The request may be aborted externally while waiting for the callback function to be called.
    if (lookup_cache_over_ || lookup_cache_stop_ || !callback_) {
      return;
    }
    if (entry == nullptr) {
      // Try to lookup cache entry in the next cache.
      current_cache_++;
      lookupCacheOnce(std::move(self));
      return;
    }
    // Hit in the current cache.
    hit_in_cache_ = current_cache_;
    lookup_cache_over_ = true;

    callback_->onSuccess(std::move(entry));
    callback_ = nullptr;
  };

  used_caches_[current_cache_].second->lookupCache(cache_key_, cache_lookup_callback);
}

void CacheGetterSetter::insertCache(Cache::CacheEntryPtr&& entry, std::string key_for_ttl) {
  ASSERT(has_cache_key_);
  ASSERT(hit_in_cache_ < int32_t(used_caches_.size()));

  int32_t insert_number = hit_in_cache_ == -1 ? used_caches_.size() : hit_in_cache_;

  uint64_t current_timpestamp = Proxy::Common::Common::TimeUtil::createTimestamp();

  for (int32_t i = 0; i < insert_number; i++) {
    // Except for the last insertion, a copy of the cache entry must be created.
    auto entry_copy = i == insert_number - 1 ? std::move(entry) : entry->createCopy();
    auto lifetime = route_config_->cacheTTL(used_caches_[i].first, key_for_ttl);

    if (lifetime == 0 || !entry_copy) {
      ENVOY_LOG(trace, "Skip cache: {} for zero TTL of entry.", used_caches_[i].first);
      continue;
    }

    entry_copy->cacheExpire(current_timpestamp + lifetime);
    ENVOY_LOG(trace, "Insert entry: {} to cache: {}", cache_key_, used_caches_[i].first);
    used_caches_[i].second->insertCache(cache_key_, std::move(entry_copy));
  }
}

void CacheGetterSetter::removeCache() {
  ASSERT(has_cache_key_);

  for (auto& cache_instance : used_caches_) {
    cache_instance.second->removeCache(cache_key_);
  }
}

CacheRequestSender::CacheRequestSender(CacheGetterSetterConfig* config,
                                       const SpecificCacheConfig* route_config)
    : CacheGetterSetter(config, route_config) {}

SendRequestStatus CacheRequestSender::sendRequest(Envoy::Http::RequestMessagePtr&& message,
                                                  Envoy::Http::AsyncClient::Callbacks* callback) {
  if (message == nullptr || callback == nullptr || route_config_ == nullptr) {
    return SendRequestStatus::FAILED;
  }
  origin_callback_ = callback;
  setCacheKey(route_config_->cacheKey(message->headers()));

  if (auto status = lookupCache(this); status == CacheLookupStatus::FAILED) {
    return SendRequestStatus::FAILED;
  }
  return SendRequestStatus::ONCALL;
}

void CacheRequestSender::onSuccess(Cache::CacheEntryPtr&& entry) {
  ASSERT(origin_callback_);
  Cache::HttpCacheEntry* http_result = dynamic_cast<Cache::HttpCacheEntry*>(entry.get());

  if (!http_result || !http_result->cacheMessage()) {
    ENVOY_LOG(debug, "'{}/{}' hit non-http cache or empty entry.", cache_key_, request_stream_id_);
    origin_callback_->onFailure(*this, Envoy::Http::AsyncClient::FailureReason::Reset);
    origin_callback_ = nullptr;
    return;
  }

  ENVOY_LOG(debug, "'{}/{}' hit in {}", cache_key_, request_stream_id_,
            used_caches_[hit_in_cache_].first);
  origin_callback_->onSuccess(*this, std::move(http_result->cacheMessage()));
  callback_ = nullptr;
}

void CacheRequestSender::onFailure() {
  ASSERT(origin_callback_);
  ENVOY_LOG(debug, "'{}/{}' miss in all cache level.", cache_key_, request_stream_id_);
  origin_callback_->onFailure(*this, Envoy::Http::AsyncClient::FailureReason::Reset);
  origin_callback_ = nullptr;
}

void CacheRequestSender::insertResponse(const Cache::CacheKeyType& cache_key,
                                        Envoy::Http::ResponseMessagePtr&& message) {
  ASSERT(message != nullptr);

  setCacheKey(cache_key);

  static constexpr size_t MAX_CACHE_ENTRY_SIZE = 512 * 1024; // 512 KB
  size_t message_size = message->headers().byteSize() + message->body().length();
  if (message_size > MAX_CACHE_ENTRY_SIZE) {
    return;
  }

  auto status = message->headers().getStatusValue();

  auto entry = std::make_unique<Cache::HttpCacheEntry>(std::move(message), 0);
  insertCache(std::move(entry), std::string(status));
}

void CacheRequestSender::removeResponse(const Cache::CacheKeyType& cache_key) {
  setCacheKey(cache_key);
  removeCache();
}

} // namespace Sender
} // namespace Common
} // namespace Proxy
} // namespace Envoy
