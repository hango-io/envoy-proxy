
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/header_restriction/header_restriction.h"

#include "api/proxy/filters/http/header_restriction/v2/header_restriction.pb.h"
#include "api/proxy/filters/http/header_restriction/v2/header_restriction.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRestriction {

template <const char* ALIAS_NAME>
class Factory
    : public Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig, ProtoRouteConfig> {
public:
  Factory() : FactoryBase(std::string(ALIAS_NAME)) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config, const std::string&,
                                    Server::Configuration::FactoryContext&) override {
    auto shared_config = std::make_shared<CommonConfig>(proto_config);
    std::string alias_name = std::string(ALIAS_NAME);
    return [shared_config, alias_name](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      callbacks.addStreamFilter(
          Http::StreamFilterSharedPtr{new Filter(alias_name, shared_config.get())});
    };
  }

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const ProtoRouteConfig& proto_config,
                                       Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override {
    return std::make_shared<RouteConfig>(proto_config);
  }
};

} // namespace HeaderRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
