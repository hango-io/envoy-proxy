#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/local_limit/local_limit_filter.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {

class HttpLocalLimitFilterFactory : public Server::Configuration::NamedHttpFilterConfigFactory {
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

  std::string name() const override { return HttpLocalLimitFilter::name(); }
};

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
