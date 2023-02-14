#include "source/filters/http/header_restriction/header_restriction.h"

#include "source/common/http/utility.h"
#include "source/common/network/address_impl.h"
#include "source/common/network/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRestriction {

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  auto route = decoder_callbacks_->route();
  route_config_ =
      Http::Utility::resolveMostSpecificPerFilterConfig<RouteConfig>(filter_name_, route);

  if (route_config_ && route_config_->disabled()) {
    return Http::FilterHeadersStatus::Continue;
  }

  bool is_white_list;
  const std::vector<CommonMatcherPtr>* common_list = nullptr;
  const std::vector<CommonMatcherPtr>* route_list = nullptr;

  // by default we'll use route-level config to override filter-level config.
  // as blacklist and whitelist are mutually exclusive, when list-type is different
  // we only consider route-level config when means enable-override.
  bool override_common =
      route_config_ && (route_config_->strategy() == ProtoStrategy::OVERRIDE ||
                        route_config_->config_.isBlackList() != config_->config_.isBlackList());

  if (route_config_) {
    is_white_list = !route_config_->config_.isBlackList();
    route_list = &route_config_->config_.list_;
    if (!override_common) {
      common_list = &config_->config_.list_;
    }
  } else {
    is_white_list = !config_->config_.isBlackList();
    common_list = &config_->config_.list_;
  }

  Common::Http::HttpCommonMatcherContext context(headers);

  bool denied = false;
  if (route_config_) {
    for (const auto& list_item : *route_list) {
      if (list_item->match(context)) {
        if (is_white_list) {
          return Http::FilterHeadersStatus::Continue;
        } else {
          denied = true;
          break;
        }
      }
    }
  }
  if (!denied && !override_common) {
    for (const auto& list_item : *common_list) {
      if (list_item->match(context)) {
        if (is_white_list) {
          return Http::FilterHeadersStatus::Continue;
        } else {
          denied = true;
          break;
        }
      }
    }
  }
  if (!denied && is_white_list) {
    denied = true;
  }

  if (!denied) {
    return Http::FilterHeadersStatus::Continue;
  }
  const static std::function<void(Http::HeaderMap & headers)> add_header =
      [](Http::HeaderMap& headers) {
        const static auto flag_header_key = Http::LowerCaseString("x-header-restriction-denied");
        const static std::string flag_header_value = "true";
        headers.addReference(flag_header_key, flag_header_value);
      };

  decoder_callbacks_->sendLocalReply(Http::Code::Forbidden, "Your request is not allowed.",
                                     add_header, absl::nullopt, "forbidden_by_header_restriction");
  // config_->stats().denied_.inc();
  ENVOY_LOG(debug, "Access from address {} is forbidden by header restriction.",
            decoder_callbacks_->streamInfo()
                .downstreamAddressProvider()
                .remoteAddress()
                ->ip()
                ->addressAsString());

  return Http::FilterHeadersStatus::StopIteration;
}

// Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap&, bool) {
//   ENVOY_LOG(debug, "Hello, my first envoy C++ encode filter.");
//   return Http::FilterHeadersStatus::Continue;
// }

} // namespace HeaderRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
