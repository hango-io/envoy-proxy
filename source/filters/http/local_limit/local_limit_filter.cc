#include "source/filters/http/local_limit/local_limit_filter.h"
#include "envoy/common/exception.h"
#include "source/common/common/assert.h"

#include "source/common/http/utility.h"
namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {

bool RateLimitEntry::limit(const Http::HeaderMap& headers) const {
  if (!Http::HeaderUtility::matchHeaders(headers, enable_rqx_)) {
    // When the conditions do not match,the current entry does not restrict the request
    return false;
  }

  uint64_t token = token_bucket_ptr_->consume(1, false);

  // If cannot consume enough tokens,it means that need to limit current
  return token < 1;
}

bool LocalLimitRouteConfig::limit(const Http::HeaderMap& headers) const {
  bool limit = false;
  for (const auto& rate_limit_entry : rate_limit_entries_) {
    limit |= rate_limit_entry->limit(headers);
  }
  return limit;
}

Http::FilterHeadersStatus HttpLocalLimitFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                              bool) {

  auto route = decoder_callbacks_->route();
  if (!route || !route->routeEntry()) {
    ENVOY_LOG(debug, "No route or route entry for local rate limit and continue filters chain");
    return Http::FilterHeadersStatus::Continue;
  }

  auto route_config =
      Http::Utility::resolveMostSpecificPerFilterConfig<LocalLimitRouteConfig>(name(), route);

  if (!route_config) {
    ENVOY_LOG(debug, "No route or vh config local rate limit and continue filters chain");
    return Http::FilterHeadersStatus::Continue;
  }

  if (!route_config->limit(headers)) {
    ENVOY_STREAM_LOG(debug, "No local limit for current request and continue filters chain",
                     *decoder_callbacks_);
    return Http::FilterHeadersStatus::Continue;
  }

  decoder_callbacks_->sendLocalReply(Http::Code::TooManyRequests, "", nullptr, absl::nullopt,
                                     "local_rate_limit");

  return Http::FilterHeadersStatus::StopIteration;
}

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
