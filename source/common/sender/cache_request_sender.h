#pragma once

#include <regex>

#include "envoy/registry/registry.h"

#include "source/common/cache/cache_base.h"
#include "source/common/common/empty_string.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/path_utility.h"
#include "source/common/sender/request_sender.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Sender {

using KeyMakerConfig = proxy::common::cache_api::v3::HttpCacheKeyMaker;
using ProtoTTL = proxy::common::cache_api::v3::CacheTTL;

using ProtoCaches = google::protobuf::RepeatedPtrField<Cache::Cache>;

class CacheLookupCallback {
public:
  virtual ~CacheLookupCallback() = default;

  virtual void onSuccess(Cache::CacheEntryPtr&& entry) PURE;
  virtual void onFailure() PURE;
};

class CacheLookupHandler {
public:
  virtual ~CacheLookupHandler() = default;

  virtual void cancelLookup() PURE;
};

enum class CacheLookupStatus {
  FAILED,
  ONCALL,
};

class HttpCacheUtil {
public:
  static Cache::CacheKeyType cacheKey(const KeyMakerConfig& config,
                                      Envoy::Http::RequestHeaderMap& headers,
                                      const std::string& prefix);
};

class CacheGetterSetterConfig : public Logger::Loggable<Logger::Id::client> {
public:
  using CacheList = std::vector<std::pair<std::string, Cache::CommonCacheBasePtr>>;

  CacheGetterSetterConfig(const ProtoCaches&, Server::Configuration::FactoryContext&,
                          bool request_cache = false);

  CacheList& usedCaches() { return used_caches_; }

private:
  CacheList used_caches_;
};

class SpecificCacheConfig : public Logger::Loggable<Logger::Id::config> {
public:
  SpecificCacheConfig(const KeyMakerConfig&, const std::map<std::string, ProtoTTL>&,
                      const std::string& prefix = "v1");

  uint64_t cacheTTL(std::string name, std::string code) const;

  Cache::CacheKeyType cacheKey(Envoy::Http::RequestHeaderMap& headers) const {
    return HttpCacheUtil::cacheKey(key_maker_config_, headers, cache_key_prefix_);
  }

private:
  using CustomsTTL = std::vector<std::pair<std::regex, uint64_t>>;

  std::map<std::string, std::pair<uint64_t, CustomsTTL>> cache_ttls_{};

  const std::string cache_key_prefix_{};
  const KeyMakerConfig key_maker_config_{};
};

using SpecificCacheConfigPtr = std::unique_ptr<SpecificCacheConfig>;
using SpecificCacheConfigSharedPtr = std::shared_ptr<SpecificCacheConfig>;

using CacheGetterSetterConfigPtr = std::shared_ptr<CacheGetterSetterConfig>;
using CacheGetterSetterConfigSharedPtr = std::shared_ptr<CacheGetterSetterConfig>;

class CacheGetterSetter;
using CacheGetterSetterSharedPtr = std::shared_ptr<CacheGetterSetter>;

class CacheGetterSetter : public std::enable_shared_from_this<CacheGetterSetter>,
                          public Logger::Loggable<Logger::Id::client>,
                          public CacheLookupHandler {
public:
  CacheGetterSetter(CacheGetterSetterConfig* config, const SpecificCacheConfig* route_config);

  CacheGetterSetter& setCacheKey(const std::string& cache_key) {
    if (!has_cache_key_) {
      cache_key_ = cache_key;
      has_cache_key_ = true;
    }
    return *this;
  }

  const Cache::CacheKeyType& cacheKey() { return cache_key_; }

  void cancelLookup() override {
    lookup_cache_stop_ = true;
    callback_ = nullptr;
  }

  CacheLookupStatus lookupCache(CacheLookupCallback*);
  void insertCache(Cache::CacheEntryPtr&& entry, std::string key_for_ttl);
  void removeCache();

  const std::string& reqeustHitInCache() const {
    return hit_in_cache_ == -1 ? EMPTY_STRING : used_caches_[hit_in_cache_].first;
  }

protected:
  void lookupCacheOnce(CacheGetterSetterSharedPtr keep_self_live);

  bool has_cache_key_{false};
  Cache::CacheKeyType cache_key_;

  bool lookup_cache_over_{false};
  bool lookup_cache_stop_{false};
  int32_t hit_in_cache_{-1};

  uint32_t current_cache_{0};

  CacheLookupCallback* callback_{nullptr};
  CacheGetterSetterConfig::CacheList& used_caches_;
  const SpecificCacheConfig* route_config_{};
};

class CacheRequestSender;
using CacheRequestSenderSharedPtr = std::shared_ptr<CacheRequestSender>;

class CacheRequestSender : public CacheGetterSetter,
                           public RequestSender,
                           public CacheLookupCallback {
public:
  CacheRequestSender(CacheGetterSetterConfig* config, const SpecificCacheConfig* route_config);

  SendRequestStatus sendRequest(Envoy::Http::RequestMessagePtr&&,
                                Envoy::Http::AsyncClient::Callbacks*) override;

  void insertResponse(const Cache::CacheKeyType& key_for_no_request,
                      Envoy::Http::ResponseMessagePtr&&);

  void removeResponse(const Cache::CacheKeyType& key_for_no_request);

  void setStreamId(uint64_t stream_id) { request_stream_id_ = std::to_string(stream_id); }

  // CacheLookupCallback
  void onSuccess(Cache::CacheEntryPtr&& entry) override;
  void onFailure() override;

  void cancel() override { cancelLookup(); }

private:
  // For debugging.
  std::string request_stream_id_{"-"};
  Envoy::Http::AsyncClient::Callbacks* origin_callback_{nullptr};
};

using CacheRequestSenderPtr = std::unique_ptr<CacheRequestSender>;
using CacheRequestSenderSharedPtr = std::shared_ptr<CacheRequestSender>;

} // namespace Sender
} // namespace Common
} // namespace Proxy
} // namespace Envoy
