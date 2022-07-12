#pragma once

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "envoy/server/filter_config.h"

#include "source/common/common/logger.h"
#include "source/common/http/header_utility.h"
#include "source/common/sender/cache_request_sender.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/super_cache/v2/super_cache.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace SuperCache {

using ProtoConfig = proxy::filters::http::super_cache::v2::HttpCache;
using RouteProtoConfig = proxy::filters::http::super_cache::v2::CacheConfig;

#define ALL_SUPER_CACHE_FILTER_STATS(COUNTER)                                                      \
  COUNTER(hit)                                                                                     \
  COUNTER(miss)

/**
 * Wrapper struct for Super cache filter stats. @see stats_macros.h
 */
struct SuperCacheFilterStats {
  ALL_SUPER_CACHE_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

class CommonCacheConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  CommonCacheConfig(const ProtoConfig& config, Server::Configuration::FactoryContext& context,
                    const std::string& stats_prefix);
  virtual ~CommonCacheConfig();

  Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr& usedCaches();

private:
  static std::map<std::string, std::string> ADMIN_HANDLER_UUID_MAP;
  std::string admin_handler_uuid_;
  std::string final_apis_prefix_;

  Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr used_caches_;
  Server::Configuration::FactoryContext& context_;

  SuperCacheFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "super_cache.";
    return {ALL_SUPER_CACHE_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

public:
  SuperCacheFilterStats stats_;
};
using CommonCacheConfigSharedPtr = std::shared_ptr<CommonCacheConfig>;

class RouteCacheConfig : public Router::RouteSpecificFilterConfig,
                         Logger::Loggable<Logger::Id::filter> {
public:
  enum Type { RQ, RP };
  RouteCacheConfig(const RouteProtoConfig& config);

  bool checkEnable(const Http::HeaderMap& headers, Type type) const;

  const Proxy::Common::Sender::SpecificCacheConfigSharedPtr& cacheConfig() const {
    return cache_config_;
  }

  bool lowlevelFill() const { return low_level_fill_; }

private:
  // construct all regex object at init to avoid repeated construct
  // at request
  std::vector<Http::HeaderUtility::HeaderDataPtr> enable_rqx_{};
  std::vector<Http::HeaderUtility::HeaderDataPtr> enable_rpx_{};

  Proxy::Common::Sender::SpecificCacheConfigSharedPtr cache_config_{nullptr};

  bool low_level_fill_;
};

class HttpCacheFilter : public Http::PassThroughFilter,
                        public Logger::Loggable<Logger::Id::filter>,
                        public Http::AsyncClient::Callbacks {
public:
  HttpCacheFilter(CommonCacheConfig* config, TimeSource& time_source, const std::string& name);

  void onDestroy() override;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;

  Http::FilterDataStatus encodeData(Buffer::Instance& buffer, bool end_stream) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers,
                                          bool end_stream) override;

  // callback
  void onSuccess(const Http::AsyncClient::Request& request,
                 Http::ResponseMessagePtr&& response) override;

  void onFailure(const Http::AsyncClient::Request& request,
                 Http::AsyncClient::FailureReason reason) override;

  void onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) override {}

private:
  Http::ResponseMessagePtr response_to_cache_{nullptr};

  bool cache_suspend_{false}; // 缓存搜索因错误而中止
  bool enable_caches_{false};

  bool hit_in_caches_{false};

  CommonCacheConfig* config_{nullptr};
  const std::string filter_name_;

  const RouteCacheConfig* route_config_{nullptr};

  Proxy::Common::Sender::CacheRequestSenderSharedPtr request_sender_{};
};
using HttpCacheFilterSharedPtr = std::shared_ptr<HttpCacheFilter>;

} // namespace SuperCache
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
