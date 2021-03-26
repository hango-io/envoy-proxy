#pragma once

#include "envoy/registry/registry.h"

#include "extensions/filters/http/common/factory_base.h"
#include "filters/http/ip_restriction/ip_restriction.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace IpRestriction {

class HttpIpRestrictionFilterConfig : public Server::Configuration::NamedHttpFilterConfigFactory {
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

  std::string name() const override;
};

} // namespace IpRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
