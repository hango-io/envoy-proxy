
#pragma once

#include <string>

#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "source/common/http/proxy_matcher.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/header_restriction/v2/header_restriction.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRestriction {

using ProtoCommonConfig = proxy::filters::http::header_restriction::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::header_restriction::v2::ProtoRouteConfig;
using ProtoHeaderRestriction = proxy::filters::http::header_restriction::v2::HeaderRestriction;
using ProtoStrategy = proxy::filters::http::header_restriction::v2::Strategy;
using ProtoListType = proxy::filters::http::header_restriction::v2::ListType;
using CommonMatcherPtr = std::unique_ptr<Common::Http::CommonMatcher>;

class HeaderRestrictionConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  HeaderRestrictionConfig(const ProtoHeaderRestriction& proto_config) {

    switch (proto_config.type()) {
    case ProtoListType::BLACK:
      is_black_list_ = true;
      break;
    case ProtoListType::WHITE:
      break;
    default:
      ENVOY_LOG(error, "ListType must be BLACK or WHITE.");
      break;
    }

    for (const auto& item : proto_config.list()) {
      list_.push_back(std::make_unique<Common::Http::CommonMatcher>(item));
    }
  }

  bool isBlackList() const { return is_black_list_; }

  std::vector<CommonMatcherPtr> list_;

private:
  bool is_black_list_{false};
};

class RouteConfig : public Router::RouteSpecificFilterConfig, Logger::Loggable<Logger::Id::filter> {
public:
  RouteConfig(const ProtoRouteConfig& proto_config)
      : config_(proto_config.config()), disabled_(proto_config.disabled()),
        strategy_(proto_config.strategy()) {}

  bool disabled() const { return disabled_; }
  ProtoStrategy strategy() const { return strategy_; }

  HeaderRestrictionConfig config_;

private:
  const bool disabled_{false};
  const ProtoStrategy strategy_;
};

class CommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  CommonConfig(const ProtoCommonConfig& proto_config) : config_(proto_config.config()) {}

  HeaderRestrictionConfig config_;
};

/*
 * Filter inherits from PassthroughFilter instead of StreamFilter because PassthroughFilter
 * implements all the StreamFilter's virtual mehotds by default. This reduces a lot of unnecessary
 * code.
 */
class Filter : public Envoy::Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(const std::string& filter_name, CommonConfig* config)
      : filter_name_(filter_name), config_(config){};

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

private:
  const std::string filter_name_;

  CommonConfig* config_{nullptr};
  const RouteConfig* route_config_{nullptr};
};

} // namespace HeaderRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
