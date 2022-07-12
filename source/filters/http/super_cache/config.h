#pragma once

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/super_cache/cache_filter.h"

#include "api/proxy/filters/http/super_cache/v2/super_cache.pb.h"
#include "api/proxy/filters/http/super_cache/v2/super_cache.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace SuperCache {

template <const char* ALIAS_NAME>
class CacheFilterFactory
    : public Extensions::HttpFilters::Common::FactoryBase<ProtoConfig, RouteProtoConfig> {
public:
  CacheFilterFactory() : FactoryBase(std::string(ALIAS_NAME)) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoConfig& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override {
    auto config = std::make_shared<CommonCacheConfig>(proto_config, context, stats_prefix);
    std::string alias_name = std::string(ALIAS_NAME);
    return [config, &context, alias_name](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamFilter(
          std::make_shared<HttpCacheFilter>(config.get(), context.timeSource(), alias_name));
    };
  }

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const RouteProtoConfig& proto_config,
                                       Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override {
    return std::make_shared<RouteCacheConfig>(proto_config);
  }
};

} // namespace SuperCache
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
