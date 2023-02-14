
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/service_downgrade/service_downgrade.h"

#include "api/proxy/filters/http/service_downgrade/v2/service_downgrade.pb.h"
#include "api/proxy/filters/http/service_downgrade/v2/service_downgrade.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ServiceDowngrade {

class Factory
    : public Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig, ProtoRouteConfig> {
public:
  Factory() : FactoryBase(Filter::name()) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const ProtoRouteConfig&,
                                       Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override;
};

} // namespace ServiceDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
