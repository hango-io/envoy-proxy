#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/header_rewrite/header_rewrite.h"

#include "api/proxy/filters/http/header_rewrite/v2/header_rewrite.pb.h"
#include "api/proxy/filters/http/header_rewrite/v2/header_rewrite.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {

template <const char* ALIAS_NAME>
class Factory
    : public Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig, ProtoRouteConfig>,
      public Logger::Loggable<Logger::Id::filter> {
public:
  Factory() : FactoryBase(std::string(ALIAS_NAME)) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config, const std::string&,
                                    Server::Configuration::FactoryContext&) override {
    std::string alias_name = std::string(ALIAS_NAME);
    auto shared_config = std::make_shared<CommonConfig>(proto_config);
    return [shared_config, alias_name](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamFilter(
          Http::StreamFilterSharedPtr{new Filter(shared_config.get(), alias_name)});
    };
  }

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const ProtoRouteConfig& proto_config,
                                       Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override {
    return std::make_shared<RouteConfig>(proto_config);
  }
};

} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
