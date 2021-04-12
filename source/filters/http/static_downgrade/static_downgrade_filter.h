#pragma once

#include "api/proxy/filters/http/static_downgrade/v2/static_downgrade.pb.h"
#include "common/http/header_utility.h"
#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "extensions/filters/http/common/pass_through_filter.h"

#include <string>

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace StaticDowngrade {

using ProtoCommonConfig = proxy::filters::http::static_downgrade::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::static_downgrade::v2::ProtoRouteConfig;
using ProtoDowngradeConfig = proxy::filters::http::static_downgrade::v2::DowngradeConfig;

class StaticDowngradeConfig : Logger::Loggable<Logger::Id::filter> {
public:
  StaticDowngradeConfig(const ProtoDowngradeConfig& config,
                        Server::Configuration::ServerFactoryContext& context);

  bool shouldDowngrade(Http::HeaderMap& rqx, Http::HeaderMap& rpx) const;

  std::vector<Http::HeaderUtility::HeaderDataPtr> downgrade_rqx_;
  std::vector<Http::HeaderUtility::HeaderDataPtr> downgrade_rpx_;

  uint32_t static_http_status_{200};
  std::map<Http::LowerCaseString, std::string> static_http_headers_;
  std::string static_http_body_;
  bool has_no_donwgrade_rpx_{false};
};

using StaticDowngradeConfigPtr = std::unique_ptr<StaticDowngradeConfig>;

class StaticDowngradeRouteConfig : public Router::RouteSpecificFilterConfig {
public:
  StaticDowngradeRouteConfig(const ProtoRouteConfig& config,
                             Server::Configuration::ServerFactoryContext& context);

  const StaticDowngradeConfig* downgradeConfig(Http::HeaderMap& rqx, Http::HeaderMap& rpx) const;

  std::vector<StaticDowngradeConfigPtr> downgrade_configs_;
};

class StaticDowngradeCommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  StaticDowngradeCommonConfig(const ProtoCommonConfig& config) : config_(config) {}

private:
  ProtoCommonConfig config_;
};

class HttpStaticDowngradeFilter : public Http::PassThroughFilter,
                                  Logger::Loggable<Logger::Id::filter> {
public:
  HttpStaticDowngradeFilter() {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;
  Http::FilterDataStatus encodeData(Buffer::Instance&, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.staticdowngrade");
  }

private:
  Http::HeaderMap* rqx_headers_{nullptr};
  Http::HeaderMap* rpx_headers_{nullptr};

  bool downgrade_complete_{false};

  const StaticDowngradeConfig* downgrade_config_{nullptr};
};

} // namespace StaticDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
