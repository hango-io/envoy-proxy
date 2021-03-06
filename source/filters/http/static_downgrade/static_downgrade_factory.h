#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/static_downgrade/static_downgrade_filter.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace StaticDowngrade {

class HttpStaticDowngradeFilterFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message& config, const std::string&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  ProtobufTypes::MessagePtr createEmptyRouteConfigProto() override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfig(const Protobuf::Message& config,
                                  Server::Configuration::ServerFactoryContext& context,
                                  ProtobufMessage::ValidationVisitor&) override;

  std::string name() const override { return HttpStaticDowngradeFilter::name(); }
};

} // namespace StaticDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
