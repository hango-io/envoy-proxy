#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/dynamic_downgrade/dynamic_downgrade_filter.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DynamicDowngrade {

class HttpDynamicDowngradeFilterFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message& config, const std::string&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  ProtobufTypes::MessagePtr createEmptyRouteConfigProto() override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfig(const Protobuf::Message& config,
                                  Server::Configuration::ServerFactoryContext&,
                                  ProtobufMessage::ValidationVisitor&) override;

  std::string name() const override { return HttpDynamicDowngradeFilter::name(); }
};

} // namespace DynamicDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
