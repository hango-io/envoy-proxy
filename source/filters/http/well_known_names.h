#pragma once

#include <string>

#include "common/singleton/const_singleton.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {

/**
 * Well-known Netease.com HTTP filter names.
 */
class HttpFilterNameValues {
public:
  const std::string MetadataHub = "proxy.filters.http.metadatahub";
  const std::string IpRestriction = "proxy.filters.http.iprestriction";
  const std::string StaticDowngrade = "proxy.filters.http.staticdowngrade";
  const std::string LocalLimit = "proxy.filters.http.locallimit";
  const std::string Rider = "proxy.filters.http.rider";
};

typedef ConstSingleton<HttpFilterNameValues> HttpFilterNames;

} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
