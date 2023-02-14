#pragma once

#include <string>

#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "source/common/common/token_bucket.h"
#include "source/common/http/header_utility.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/local_limit/v2/local_limit.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {

using ProtoCommonConfig = proxy::filters::http::local_limit::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::local_limit::v2::ProtoRouteConfig;
using ProtoCommonRateLimit = proxy::filters::http::local_limit::v2::CommonRateLimit;

class RateLimitEntry {
public:
  RateLimitEntry(Envoy::Server::Configuration::ServerFactoryContext& context,
                 const ProtoCommonRateLimit& rate_limit,
                 bool use_thread_local_token_bucket = false) {
    for (auto& header_data : rate_limit.matcher().headers()) {
      enable_rqx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
    }

    static const double CYCLE[] = {1.0, 60 * 1, 60 * 60 * 1.0, 24 * 60 * 60 * 1.0};

    uint64_t rate = rate_limit.config().rate();
    double fill_rate = double(rate) / CYCLE[rate_limit.config().unit()];

    if (use_thread_local_token_bucket) {
      token_bucket_ptr_ =
          std::make_unique<Common::Common::TheadLocalTokenBucket>(context, rate, fill_rate);
    } else {
      token_bucket_ptr_ =
          std::make_unique<Common::Common::AccurateTokenBucket>(context, rate, fill_rate);
    }
  }

  bool limit(const Http::HeaderMap& headers) const;

private:
  std::vector<Http::HeaderUtility::HeaderDataPtr> enable_rqx_{};

  Envoy::TokenBucketPtr token_bucket_ptr_;
};

using RateLimitEntryPtr = std::unique_ptr<RateLimitEntry>;

class LocalLimitRouteConfig : public Router::RouteSpecificFilterConfig,
                              Logger::Loggable<Logger::Id::filter> {
public:
  LocalLimitRouteConfig(const ProtoRouteConfig& config,
                        Envoy::Server::Configuration::ServerFactoryContext& context)
      : context_(context) {
    for (const auto& rate_limit_entry : config.rate_limit()) {
      rate_limit_entries_.push_back(std::make_unique<RateLimitEntry>(
          context_, rate_limit_entry, config.use_thread_local_token_bucket().value()));
    }
  }

  bool limit(const Http::HeaderMap& headers) const;

private:
  std::vector<RateLimitEntryPtr> rate_limit_entries_;
  Envoy::Server::Configuration::ServerFactoryContext& context_;
};

class LocalLimitCommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  LocalLimitCommonConfig(const ProtoCommonConfig&) {}
};

class HttpLocalLimitFilter : public Http::PassThroughDecoderFilter,
                             public Logger::Loggable<Logger::Id::filter> {
public:
  HttpLocalLimitFilter(LocalLimitCommonConfig* config) : config_(config){};
  ~HttpLocalLimitFilter() = default;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.locallimit");
  }

private:
  LocalLimitCommonConfig* config_{nullptr};
};

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
