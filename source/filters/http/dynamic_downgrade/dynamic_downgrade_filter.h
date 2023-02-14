#pragma once

#include <regex>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/router/router.h"
#include "envoy/server/filter_config.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/proxy_base.h"
#include "source/common/http/utility.h"
#include "source/common/sender/cache_request_sender.h"
#include "source/common/sender/httpx_request_sender.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/dynamic_downgrade/v2/dynamic_downgrade.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DynamicDowngrade {

using ProtoCommonConfig = proxy::filters::http::dynamic_downgrade::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::dynamic_downgrade::v2::ProtoRouteConfig;

class DynamicDowngradeRouteConfig : public Router::RouteSpecificFilterConfig,
                                    Logger::Loggable<Logger::Id::filter> {
public:
  DynamicDowngradeRouteConfig(const ProtoRouteConfig& config);

  bool shouldDowngrade(Http::HeaderMap& rqx, Proxy::Common::Http::Direction) const;

  bool checkEnableCache(const Http::HeaderMap& headers) const;

  bool downgradeWithCache() const;

  const Proxy::Common::Http::HttpUriPtr& downgradeUri() const { return downgrade_uri_; }

  const auto& overrideRemote() const { return override_remote_; }

  const Proxy::Common::Sender::SpecificCacheConfigSharedPtr& cacheConfig() const {
    return cache_config_;
  }

private:
  std::vector<Http::HeaderUtility::HeaderDataPtr> downgrade_rqx_;
  std::vector<Http::HeaderUtility::HeaderDataPtr> downgrade_rpx_;

  std::vector<Http::HeaderUtility::HeaderDataPtr> cache_rpx_rpx_;

  bool has_no_donwgrade_rpx_{false};
  bool downgrade_with_cache_{false};

  Proxy::Common::Sender::SpecificCacheConfigSharedPtr cache_config_{nullptr};
  Proxy::Common::Http::HttpUriPtr downgrade_uri_{nullptr};

  // 如果不为空将用于覆盖 HTTP 接口降级时所指向俄 cluster 和 timeout
  Proxy::Common::Sender::HttpxRequestSenderConfigSharedPtr override_remote_;
  static constexpr uint64_t DefaultTimeout = 200;
};

class DynamicDowngradeCommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  DynamicDowngradeCommonConfig(const ProtoCommonConfig& config,
                               Server::Configuration::FactoryContext& context);
  ~DynamicDowngradeCommonConfig();
  Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr& usedCaches();

  Proxy::Common::Sender::HttpxRequestSenderConfigSharedPtr& usedRemote();

  Server::Configuration::FactoryContext& context() { return context_; }

private:
  static std::map<std::string, std::string> ADMIN_HANDLER_UUID_MAP;
  std::string admin_handler_uuid_;
  std::string final_apis_prefix_;

  static constexpr uint64_t DefaultTimeout = 200;

  Server::Configuration::FactoryContext& context_;

  Proxy::Common::Sender::HttpxRequestSenderConfigSharedPtr used_remote_;
  Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr used_caches_;
};

class HttpDynamicDowngradeFilter : public Http::PassThroughFilter,
                                   Logger::Loggable<Logger::Id::filter>,
                                   public Http::AsyncClient::Callbacks {
public:
  HttpDynamicDowngradeFilter(DynamicDowngradeCommonConfig* config) : config_(config){};

  void onDestroy() override;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;

  // callback
  void onSuccess(const Http::AsyncClient::Request& request,
                 Http::ResponseMessagePtr&& response) override;

  void onFailure(const Http::AsyncClient::Request& request,
                 Http::AsyncClient::FailureReason reason) override;

  void onBeforeFinalizeUpstreamSpan(Tracing::Span& span,
                                    const Http::ResponseHeaderMap* response_headers) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.dynamicdowngrade");
  }

private:
  Http::RequestHeaderMap* rqx_headers_{nullptr};
  Http::ResponseHeaderMap* rpx_headers_{nullptr};

  Proxy::Common::Cache::CacheKeyType cache_key_;
  Http::ResponseMessagePtr response_to_cache_{nullptr};

  // 使用HTTPx降级时缓存请求体
  Buffer::OwnedImpl buffered_rqx_body_{};
  // 降级时用于存储从外部获取的响应体；缓存时用户缓存响应体
  Buffer::OwnedImpl buffered_rpx_body_{};

  bool should_downgrade_{false}; // 根据配置判断是否应该降级

  bool downgrade_complete_{false}; // 降级是否已经完成
  bool downgrade_suspend_{false};  // 降级因错误而中止

  bool should_cache_rpx_{false};  // 根据配置判断是否应该缓存
  bool headers_only_resp_{false}; // 原始响应是否具有响应体

  bool is_sending_request_{false}; // 降级请求正在进行当中
  bool local_request_over_{false}; // 降级请求在本地直接完成

  Proxy::Common::Sender::RequestSenderSharedPtr request_sender_{};

  DynamicDowngradeCommonConfig* config_{nullptr};
  const DynamicDowngradeRouteConfig* router_config_{nullptr};
};

} // namespace DynamicDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
