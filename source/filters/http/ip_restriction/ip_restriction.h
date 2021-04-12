#pragma once

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "envoy/api/v2/route/route.pb.h"
#include "envoy/http/filter.h"
#include "envoy/runtime/runtime.h"
#include "envoy/server/filter_config.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/ip_restriction/v2/ip_restriction.pb.h"
#include "common/buffer/watermark_buffer.h"
#include "common/common/token_bucket_impl.h"
#include "common/http/header_utility.h"
#include "common/network/cidr_range.h"
#include "common/singleton/const_singleton.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace IpRestriction {

/**
 * All stats for the Ip Restriction filter. @see stats_macros.h
 */

#define ALL_IP_RESTRICTION_FILTER_STATS(COUNTER)                                                   \
  COUNTER(ok)                                                                                      \
  COUNTER(denied)

/**
 * Wrapper struct for Ip Restriction filter stats. @see stats_macros.h
 */
struct IpRestrictionFilterStats {
  ALL_IP_RESTRICTION_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

class BlackOrWhiteListConfig : public Router::RouteSpecificFilterConfig,
                               Logger::Loggable<Logger::Id::filter> {
public:
  BlackOrWhiteListConfig(const proxy::filters::http::iprestriction::v2::BlackOrWhiteList& list);
  bool isAllowed(const Network::Address::Instance* address) const;

public:
  static const std::regex REGEXIPV4;
  static const std::regex REGEXIPV6;
  static const std::regex IPV4WITHCIDR;
  static const std::regex IPV6WITHCIDR;

  enum IpType { RAWIPV4, RAWIPV6, CIDRV4, CIDRV6, NONE };

private:
  bool isBlackList_;
  std::unordered_set<uint32_t> rawIpV4;
  // absl:uint128无法存放进入unordered_set，目前使用字符串处理v6 ip
  // 有待优化
  std::unordered_set<std::string> rawIpV6;

  std::vector<Network::Address::CidrRange> cidrIpV4_;
  std::vector<Network::Address::CidrRange> cidrIpV6_;

  void parseIpEntry(const std::string& entry);
  IpType parseIpType(const std::string& entry);

  bool checkIpEntry(const Network::Address::Instance* address) const;
};

using BlackOrWhiteListConfigSharedPtr = std::shared_ptr<BlackOrWhiteListConfig>;

class ListGlobalConfig {
public:
  ListGlobalConfig(const proxy::filters::http::iprestriction::v2::ListGlobalConfig& config,
                   Server::Configuration::FactoryContext& context, const std::string& stats_prefix);

  Http::LowerCaseString ip_source_header_;

  const IpRestrictionFilterStats& stats() const { return stats_; }
  const Stats::Scope& scope() const { return scope_; }

private:
  IpRestrictionFilterStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    const std::string final_prefix = prefix + "ip_restricion.";
    return {ALL_IP_RESTRICTION_FILTER_STATS(POOL_COUNTER_PREFIX(scope, final_prefix))};
  }

  IpRestrictionFilterStats stats_;

  Stats::Scope& scope_;
};

using ListGlobalConfigSharedPtr = std::shared_ptr<ListGlobalConfig>;

class HttpIpRestrictionFilter : public Http::PassThroughDecoderFilter,
                                Logger::Loggable<Logger::Id::filter> {
public:
  HttpIpRestrictionFilter(const ListGlobalConfig* config) : config_(config) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.iprestriction");
  }

private:
  const ListGlobalConfig* config_{nullptr};
};

using IpRestrictionFilterSharedPtr = std::shared_ptr<HttpIpRestrictionFilter>;

} // namespace IpRestriction

} // namespace HttpFilters

} // namespace Proxy
} // namespace Envoy
