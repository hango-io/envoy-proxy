
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/waf/waf.h"

#include "api/proxy/filters/http/waf/v2/waf.pb.h"
#include "api/proxy/filters/http/waf/v2/waf.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Waf {

class WafFilterConfig : public Server::Configuration::NamedHttpFilterConfigFactory {
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

} // namespace Waf
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
